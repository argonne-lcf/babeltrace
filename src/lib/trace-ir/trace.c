/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/TRACE"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include <babeltrace2/trace-ir/trace.h>
#include <babeltrace2/trace-ir/event-class.h>
#include "ctf-writer/functor.h"
#include "ctf-writer/clock.h"
#include "compat/compiler.h"
#include <babeltrace2/value.h>
#include "lib/value.h"
#include <babeltrace2/types.h>
#include "compat/endian.h"
#include "common/assert.h"
#include "compat/glib.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "attributes.h"
#include "clock-class.h"
#include "event-class.h"
#include "event.h"
#include "field-class.h"
#include "field-wrapper.h"
#include "resolve-field-path.h"
#include "stream-class.h"
#include "stream.h"
#include "trace-class.h"
#include "trace.h"
#include "utils.h"
#include "lib/value.h"
#include "lib/func-status.h"

struct bt_trace_destruction_listener_elem {
	bt_trace_destruction_listener_func func;
	void *data;
};

#define BT_ASSERT_PRE_DEV_TRACE_HOT(_trace)				\
	BT_ASSERT_PRE_DEV_HOT("trace", (_trace), "Trace", ": %!+t", (_trace))

#define DESTRUCTION_LISTENER_FUNC_NAME					\
	"bt_trace_class_destruction_listener_func"

static
void destroy_trace(struct bt_object *obj)
{
	struct bt_trace *trace = (void *) obj;

	BT_LIB_LOGD("Destroying trace object: %!+t", trace);
	BT_OBJECT_PUT_REF_AND_RESET(trace->user_attributes);

	/*
	 * Call destruction listener functions so that everything else
	 * still exists in the trace.
	 */
	if (trace->destruction_listeners) {
		uint64_t i;
		const struct bt_error *saved_error;

		BT_LIB_LOGD("Calling trace destruction listener(s): %!+t", trace);

		/*
		* The trace's reference count is 0 if we're here. Increment
		* it to avoid a double-destroy (possibly infinitely recursive).
		* This could happen for example if a destruction listener did
		* bt_object_get_ref() (or anything that causes
		* bt_object_get_ref() to be called) on the trace (ref.
		* count goes from 0 to 1), and then bt_object_put_ref(): the
		* reference count would go from 1 to 0 again and this function
		* would be called again.
		*/
		trace->base.ref_count++;

		saved_error = bt_current_thread_take_error();

		/* Call all the trace destruction listeners */
		for (i = 0; i < trace->destruction_listeners->len; i++) {
			struct bt_trace_destruction_listener_elem elem =
				bt_g_array_index(trace->destruction_listeners,
					struct bt_trace_destruction_listener_elem, i);

			if (elem.func) {
				elem.func(trace, elem.data);
				BT_ASSERT_POST_NO_ERROR(
					DESTRUCTION_LISTENER_FUNC_NAME);
			}

			/*
			 * The destruction listener should not have kept a
			 * reference to the trace.
			 */
			BT_ASSERT_POST(DESTRUCTION_LISTENER_FUNC_NAME,
				"trace-reference-count-not-changed",
				trace->base.ref_count == 1,
				"Destruction listener kept a reference to the trace being destroyed: %![trace-]+t",
				trace);
		}
		g_array_free(trace->destruction_listeners, TRUE);
		trace->destruction_listeners = NULL;

		if (saved_error) {
			BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(saved_error);
		}
	}

	if (trace->name.str) {
		g_string_free(trace->name.str, TRUE);
		trace->name.str = NULL;
		trace->name.value = NULL;
	}

	if (trace->environment) {
		BT_LOGD_STR("Destroying environment attributes.");
		bt_attributes_destroy(trace->environment);
		trace->environment = NULL;
	}

	if (trace->streams) {
		BT_LOGD_STR("Destroying streams.");
		g_ptr_array_free(trace->streams, TRUE);
		trace->streams = NULL;
	}

	if (trace->stream_classes_stream_count) {
		g_hash_table_destroy(trace->stream_classes_stream_count);
		trace->stream_classes_stream_count = NULL;
	}

	BT_LOGD_STR("Putting trace's class.");
	bt_object_put_ref(trace->class);
	trace->class = NULL;
	g_free(trace);
}

BT_EXPORT
struct bt_trace *bt_trace_create(struct bt_trace_class *tc)
{
	struct bt_trace *trace = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	BT_LIB_LOGD("Creating trace object: %![tc-]+T", tc);
	trace = g_new0(struct bt_trace, 1);
	if (!trace) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one trace.");
		goto error;
	}

	bt_object_init_shared(&trace->base, destroy_trace);
	trace->user_attributes = bt_value_map_create();
	if (!trace->user_attributes) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create a map value object.");
		goto error;
	}

	trace->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!trace->streams) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}

	trace->stream_classes_stream_count = g_hash_table_new(g_direct_hash,
		g_direct_equal);
	if (!trace->stream_classes_stream_count) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GHashTable.");
		goto error;
	}

	trace->name.str = g_string_new(NULL);
	if (!trace->name.str) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GString.");
		goto error;
	}

	trace->environment = bt_attributes_create();
	if (!trace->environment) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty attributes object.");
		goto error;
	}

	trace->destruction_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_trace_destruction_listener_elem));
	if (!trace->destruction_listeners) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GArray.");
		goto error;
	}

	trace->class = tc;
	bt_object_get_ref_no_null_check(trace->class);
	BT_LIB_LOGD("Created trace object: %!+t", trace);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(trace);

end:
	return trace;
}

BT_EXPORT
const char *bt_trace_get_name(const struct bt_trace *trace)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	return trace->name.value;
}

BT_EXPORT
enum bt_trace_set_name_status bt_trace_set_name(struct bt_trace *trace,
		const char *name)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_NAME_NON_NULL(name);
	BT_ASSERT_PRE_DEV_TRACE_HOT(trace);
	g_string_assign(trace->name.str, name);
	trace->name.value = trace->name.str->str;
	BT_LIB_LOGD("Set trace's name: %!+t", trace);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
bt_uuid bt_trace_get_uuid(const struct bt_trace *trace)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	return trace->uuid.value;
}

BT_EXPORT
void bt_trace_set_uuid(struct bt_trace *trace, bt_uuid uuid)
{
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_UUID_NON_NULL(uuid);
	BT_ASSERT_PRE_DEV_TRACE_HOT(trace);
	bt_uuid_copy(trace->uuid.uuid, uuid);
	trace->uuid.value = trace->uuid.uuid;
	BT_LIB_LOGD("Set trace's UUID: %!+t", trace);
}

static
bool trace_has_environment_entry(const struct bt_trace *trace, const char *name)
{
	BT_ASSERT(trace);

	return bt_attributes_borrow_field_value_by_name(
		trace->environment, name);
}

static
enum bt_trace_set_environment_entry_status set_environment_entry(
		struct bt_trace *trace,
		const char *name, struct bt_value *value)
{
	int ret;

	BT_ASSERT(trace);
	BT_ASSERT(name);
	BT_ASSERT(value);
	BT_ASSERT_PRE("not-frozen:trace",
		!trace->frozen ||
			!trace_has_environment_entry(trace, name),
		"Trace is frozen: cannot replace environment entry: "
		"%![trace-]+t, entry-name=\"%s\"", trace, name);
	ret = bt_attributes_set_field_value(trace->environment, name,
		value);
	if (ret) {
		ret = BT_FUNC_STATUS_MEMORY_ERROR;
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot set trace's environment entry: "
			"%![trace-]+t, entry-name=\"%s\"", trace, name);
	} else {
		bt_value_freeze(value);
		BT_LIB_LOGD("Set trace's environment entry: "
			"%![trace-]+t, entry-name=\"%s\"", trace, name);
	}

	return ret;
}

BT_EXPORT
enum bt_trace_set_environment_entry_status
bt_trace_set_environment_entry_string(
		struct bt_trace *trace, const char *name, const char *value)
{
	int ret;
	struct bt_value *value_obj;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_NAME_NON_NULL(name);
	BT_ASSERT_PRE_NON_NULL("value", value, "Value");

	value_obj = bt_value_string_create_init(value);
	if (!value_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot create a string value object.");
		ret = -1;
		goto end;
	}

	/* set_environment_entry() logs errors */
	ret = set_environment_entry(trace, name, value_obj);

end:
	bt_object_put_ref(value_obj);
	return ret;
}

BT_EXPORT
enum bt_trace_set_environment_entry_status
bt_trace_set_environment_entry_integer(
		struct bt_trace *trace, const char *name, int64_t value)
{
	int ret;
	struct bt_value *value_obj;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_NAME_NON_NULL(name);

	value_obj = bt_value_integer_signed_create_init(value);
	if (!value_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot create an integer value object.");
		ret = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	/* set_environment_entry() logs errors */
	ret = set_environment_entry(trace, name, value_obj);

end:
	bt_object_put_ref(value_obj);
	return ret;
}

BT_EXPORT
uint64_t bt_trace_get_environment_entry_count(const struct bt_trace *trace)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	return bt_attributes_get_count(trace->environment);
}

BT_EXPORT
void bt_trace_borrow_environment_entry_by_index_const(
		const struct bt_trace *trace, uint64_t index,
		const char **name, const struct bt_value **value)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_DEV_NAME_NON_NULL(name);
	BT_ASSERT_PRE_DEV_NON_NULL("value-object-output", value,
		"Value object (output)");
	BT_ASSERT_PRE_DEV_VALID_INDEX(index,
		bt_attributes_get_count(trace->environment));
	*value = bt_attributes_borrow_field_value(trace->environment, index);
	BT_ASSERT(*value);
	*name = bt_attributes_get_field_name(trace->environment, index);
	BT_ASSERT(*name);
}

BT_EXPORT
const struct bt_value *bt_trace_borrow_environment_entry_value_by_name_const(
		const struct bt_trace *trace, const char *name)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_DEV_NAME_NON_NULL(name);
	return bt_attributes_borrow_field_value_by_name(trace->environment,
		name);
}

BT_EXPORT
uint64_t bt_trace_get_stream_count(const struct bt_trace *trace)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	return (uint64_t) trace->streams->len;
}

BT_EXPORT
struct bt_stream *bt_trace_borrow_stream_by_index(
		struct bt_trace *trace, uint64_t index)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, trace->streams->len);
	return g_ptr_array_index(trace->streams, index);
}

BT_EXPORT
const struct bt_stream *bt_trace_borrow_stream_by_index_const(
		const struct bt_trace *trace, uint64_t index)
{
	return bt_trace_borrow_stream_by_index((void *) trace, index);
}

BT_EXPORT
struct bt_stream *bt_trace_borrow_stream_by_id(struct bt_trace *trace,
		uint64_t id)
{
	struct bt_stream *stream = NULL;
	uint64_t i;

	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);

	for (i = 0; i < trace->streams->len; i++) {
		struct bt_stream *stream_candidate =
			g_ptr_array_index(trace->streams, i);

		if (stream_candidate->id == id) {
			stream = stream_candidate;
			goto end;
		}
	}

end:
	return stream;
}

BT_EXPORT
const struct bt_stream *bt_trace_borrow_stream_by_id_const(
		const struct bt_trace *trace, uint64_t id)
{
	return bt_trace_borrow_stream_by_id((void *) trace, id);
}

BT_EXPORT
enum bt_trace_add_listener_status bt_trace_add_destruction_listener(
		const struct bt_trace *c_trace,
		bt_trace_destruction_listener_func listener,
		void *data, bt_listener_id *listener_id)
{
	struct bt_trace *trace = (void *) c_trace;
	uint64_t i;
	struct bt_trace_destruction_listener_elem new_elem = {
		.func = listener,
		.data = data,
	};

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_LISTENER_FUNC_NON_NULL(listener);

	/* Find the next available spot */
	for (i = 0; i < trace->destruction_listeners->len; i++) {
		struct bt_trace_destruction_listener_elem elem =
			bt_g_array_index(trace->destruction_listeners,
				struct bt_trace_destruction_listener_elem, i);

		if (!elem.func) {
			break;
		}
	}

	if (i == trace->destruction_listeners->len) {
		g_array_append_val(trace->destruction_listeners, new_elem);
	} else {
		g_array_insert_val(trace->destruction_listeners, i, new_elem);
	}

	if (listener_id) {
		*listener_id = i;
	}

	BT_LIB_LOGD("Added destruction listener: " "%![trace-]+t, "
			"listener-id=%" PRIu64, trace, i);
	return BT_FUNC_STATUS_OK;
}

static
bool has_listener_id(const struct bt_trace *trace, uint64_t listener_id)
{
	BT_ASSERT(listener_id < trace->destruction_listeners->len);
	return (&bt_g_array_index(trace->destruction_listeners,
			struct bt_trace_destruction_listener_elem,
			listener_id))->func;
}

BT_EXPORT
enum bt_trace_remove_listener_status bt_trace_remove_destruction_listener(
		const struct bt_trace *c_trace, bt_listener_id listener_id)
{
	struct bt_trace *trace = (void *) c_trace;
	struct bt_trace_destruction_listener_elem *elem;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE("listener-id-exists",
		has_listener_id(trace, listener_id),
		"Trace has no such trace destruction listener ID: "
		"%![trace-]+t, %" PRIu64, trace, listener_id);
	elem = &bt_g_array_index(trace->destruction_listeners,
			struct bt_trace_destruction_listener_elem,
			listener_id);
	BT_ASSERT(elem->func);

	elem->func = NULL;
	elem->data = NULL;
	BT_LIB_LOGD("Removed \"trace destruction listener: "
		"%![trace-]+t, listener-id=%" PRIu64,
		trace, listener_id);
	return BT_FUNC_STATUS_OK;
}

void _bt_trace_freeze(const struct bt_trace *trace)
{
	BT_ASSERT(trace);
	BT_LIB_LOGD("Freezing trace's class: %!+T", trace->class);
	bt_trace_class_freeze(trace->class);
	BT_LIB_LOGD("Freezing trace's user attributes: %!+v",
		trace->user_attributes);
	bt_value_freeze(trace->user_attributes);
	BT_LIB_LOGD("Freezing trace: %!+t", trace);
	((struct bt_trace *) trace)->frozen = true;
}

void bt_trace_add_stream(struct bt_trace *trace, struct bt_stream *stream)
{
	guint count = 0;

	bt_object_set_parent(&stream->base, &trace->base);
	g_ptr_array_add(trace->streams, stream);
	bt_trace_freeze(trace);

	if (bt_g_hash_table_contains(trace->stream_classes_stream_count,
			stream->class)) {
		count = GPOINTER_TO_UINT(g_hash_table_lookup(
			trace->stream_classes_stream_count, stream->class));
	}

	g_hash_table_insert(trace->stream_classes_stream_count,
		stream->class, GUINT_TO_POINTER(count + 1));
}

uint64_t bt_trace_get_automatic_stream_id(const struct bt_trace *trace,
		const struct bt_stream_class *stream_class)
{
	gpointer orig_key;
	gpointer value;
	uint64_t id = 0;

	BT_ASSERT(stream_class);
	BT_ASSERT(trace);
	if (g_hash_table_lookup_extended(trace->stream_classes_stream_count,
			stream_class, &orig_key, &value)) {
		id = (uint64_t) GPOINTER_TO_UINT(value);
	}

	return id;
}

BT_EXPORT
struct bt_trace_class *bt_trace_borrow_class(struct bt_trace *trace)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	return trace->class;
}

BT_EXPORT
const struct bt_trace_class *bt_trace_borrow_class_const(
		const struct bt_trace *trace)
{
	return bt_trace_borrow_class((void *) trace);
}

BT_EXPORT
const struct bt_value *bt_trace_borrow_user_attributes_const(
		const struct bt_trace *trace)
{
	BT_ASSERT_PRE_DEV_TRACE_NON_NULL(trace);
	return trace->user_attributes;
}

BT_EXPORT
struct bt_value *bt_trace_borrow_user_attributes(struct bt_trace *trace)
{
	return (void *) bt_trace_borrow_user_attributes_const((void *) trace);
}

BT_EXPORT
void bt_trace_set_user_attributes(
		struct bt_trace *trace,
		const struct bt_value *user_attributes)
{
	BT_ASSERT_PRE_TRACE_NON_NULL(trace);
	BT_ASSERT_PRE_USER_ATTRS_NON_NULL(user_attributes);
	BT_ASSERT_PRE_USER_ATTRS_IS_MAP(user_attributes);
	BT_ASSERT_PRE_DEV_TRACE_HOT(trace);
	bt_object_put_ref_no_null_check(trace->user_attributes);
	trace->user_attributes = (void *) user_attributes;
	bt_object_get_ref_no_null_check(trace->user_attributes);
}

BT_EXPORT
void bt_trace_get_ref(const struct bt_trace *trace)
{
	bt_object_get_ref(trace);
}

BT_EXPORT
void bt_trace_put_ref(const struct bt_trace *trace)
{
	bt_object_put_ref(trace);
}
