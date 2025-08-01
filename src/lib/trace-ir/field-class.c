/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/FIELD-CLASS"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field.h>
#include <babeltrace2/trace-ir/clock-class.h>
#include "lib/object.h"
#include "compat/compiler.h"
#include "compat/endian.h"
#include "common/assert.h"
#include "common/common.h"
#include "compat/glib.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "field-class.h"
#include "field-path.h"
#include "lib/func-status.h"
#include "lib/integer-range-set.h"
#include "lib/value.h"
#include "lib/trace-ir/trace-class.h"

BT_EXPORT
enum bt_field_class_type bt_field_class_get_type(
		const struct bt_field_class *fc)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	return fc->type;
}

static
int init_field_class(struct bt_field_class *fc, enum bt_field_class_type type,
		bt_object_release_func release_func,
		const struct bt_trace_class *trace_class)
{
	int ret = 0;

	BT_ASSERT(fc);
	BT_ASSERT(release_func);
	bt_object_init_shared(&fc->base, release_func);
	fc->type = type;
	fc->user_attributes = bt_value_map_create();
	if (!fc->user_attributes) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create a map value object.");
		ret = -1;
		goto end;
	}

	fc->mip_version = trace_class->mip_version;

end:
	return ret;
}

static
void finalize_field_class(struct bt_field_class *fc)
{
	BT_OBJECT_PUT_REF_AND_RESET(fc->user_attributes);
}

#define _BT_ASSERT_PRE_BIT_ARRAY_FC_FLAG_NAME	"Bit array field class flag"
#define _BT_ASSERT_PRE_BIT_ARRAY_FC_FLAG_ID	"bit-array-field-class-flag"

#define BT_ASSERT_PRE_DEV_BIT_ARRAY_FC_FLAG_NON_NULL(_flag)	\
	BT_ASSERT_PRE_DEV_NON_NULL(				\
		_BT_ASSERT_PRE_BIT_ARRAY_FC_FLAG_ID, (_flag),	\
		_BT_ASSERT_PRE_BIT_ARRAY_FC_FLAG_NAME)

static
void destroy_bit_array_field_class(struct bt_object *obj)
{
	const struct bt_field_class_bit_array *ba_fc;

	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying bit array field class object: %!+F", obj);

	ba_fc = (const void *) obj;

	if (ba_fc->flags) {
		g_ptr_array_free(ba_fc->flags, TRUE);
	}

	finalize_field_class((void *) obj);
	g_free(obj);
}

static
void destroy_bit_array_flag(struct bt_field_class_bit_array_flag *flag)
{
	if (!flag) {
		goto end;
	}

	g_free(flag->label);
	bt_object_put_ref(flag->range_set);
	g_free(flag);

end:
	return;
}

static
void destroy_bit_array_flag_void(gpointer ptr)
{
	destroy_bit_array_flag((struct bt_field_class_bit_array_flag *) ptr);
}

BT_EXPORT
struct bt_field_class *bt_field_class_bit_array_create(
		struct bt_trace_class *trace_class, uint64_t length)
{
	struct bt_field_class_bit_array *ba_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE("valid-length", length > 0 && length <= 64,
		"Unsupported length for bit array field class "
		"(minimum is 1, maximum is 64): length=%" PRIu64, length);
	BT_LOGD("Creating default bit array field class object.");
	ba_fc = g_new0(struct bt_field_class_bit_array, 1);
	if (!ba_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one bit array field class.");
		goto error;
	}

	if (init_field_class((void *) ba_fc, BT_FIELD_CLASS_TYPE_BIT_ARRAY,
			destroy_bit_array_field_class, trace_class)) {
		goto error;
	}

	ba_fc->length = length;
	ba_fc->flags = g_ptr_array_new_with_free_func(
		destroy_bit_array_flag_void);
	if (!ba_fc->flags) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		goto error;
	}

	ba_fc->label_buf = g_ptr_array_new();
	if (!ba_fc->label_buf) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		goto error;
	}

	BT_LIB_LOGD("Created bit array field class object: %!+F", ba_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(ba_fc);

end:
	return (void *) ba_fc;
}

BT_EXPORT
uint64_t bt_field_class_bit_array_get_length(const struct bt_field_class *fc)
{
	const struct bt_field_class_bit_array *ba_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "bit-array",
		BT_FIELD_CLASS_TYPE_BIT_ARRAY, "Field class");
	return ba_fc->length;
}

BT_EXPORT
uint64_t bt_field_class_bit_array_get_flag_count(const bt_field_class *fc)
{
	const struct bt_field_class_bit_array *ba_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "bit-array",
		BT_FIELD_CLASS_TYPE_BIT_ARRAY, "Field class");
	return ba_fc->flags->len;
}

BT_EXPORT
bt_field_class_bit_array_add_flag_status bt_field_class_bit_array_add_flag(
		struct bt_field_class *fc, const char *label,
		const bt_integer_range_set_unsigned *index_ranges)
{
	struct bt_field_class_bit_array *ba_fc = (void *) fc;
	struct bt_field_class_bit_array_flag *flag = NULL;
	bt_field_class_bit_array_add_flag_status status;
	struct bt_integer_range_set *index_ranges_internal =
		(bt_integer_range_set *) index_ranges;
	guint range_i;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc, "bit-array",
		BT_FIELD_CLASS_TYPE_BIT_ARRAY, "Field class");
	BT_ASSERT_PRE_NON_NULL("label", label, "Label");
	BT_ASSERT_PRE("bit-array-field-class-flag-label-is-unique",
		!bt_field_class_bit_array_borrow_flag_by_label_const(fc, label),
		"Duplicate flag name in bit array field class: "
		"%![bit-array-fc-]+F, label=\"%s\"", fc, label);
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL(index_ranges);

	for (range_i = 0; range_i < index_ranges_internal->ranges->len; ++range_i) {
		struct bt_integer_range *range
			= BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(index_ranges_internal,
				range_i);

		BT_ASSERT_PRE("bit-array-field-class-flag-bit-index-is-less-than-field-class-length",
			range->upper.u < ba_fc->length,
			"Flag bit index range's upper bound is greater than or "
			"equal to bit array field length: %![bit-array-fc-]+F, "
			"range-index=%u, upper-bound=%" PRIu64,
			ba_fc, range_i, range->upper.u);
	}

	flag = g_new0(struct bt_field_class_bit_array_flag, 1);
	if (!flag) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a bit_array_flag.");
		status = BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_MEMORY_ERROR;
		goto end;
	}

	flag->label = g_strdup(label);
	if (!flag->label) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate memory for bit array flag label.");
		status = BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_MEMORY_ERROR;
		goto end;
	}

	flag->range_set = index_ranges;
	bt_integer_range_set_unsigned_get_ref(flag->range_set);

	/* Set flag->mask */
	for (range_i = 0; range_i < index_ranges_internal->ranges->len; range_i++) {
		const struct bt_integer_range *range = (const void *)
			BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(index_ranges_internal,
				range_i);
		uint64_t bit_index;

		for (bit_index = range->lower.u; bit_index <= range->upper.u;
				++bit_index) {
			flag->mask |= UINT64_C(1) << bit_index;
		}
	}

	g_ptr_array_add(ba_fc->flags, flag);
	flag = NULL;

	status = BT_FIELD_CLASS_BIT_ARRAY_ADD_FLAG_STATUS_OK;
	goto end;

end:
	destroy_bit_array_flag(flag);
	return status;
}

BT_EXPORT
const bt_field_class_bit_array_flag *
bt_field_class_bit_array_borrow_flag_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	struct bt_field_class_bit_array *ba_fc = (void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "bit-array",
		BT_FIELD_CLASS_TYPE_BIT_ARRAY, "Field class");
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, ba_fc->flags->len);
	return ba_fc->flags->pdata[index];
}

BT_EXPORT
const bt_field_class_bit_array_flag *
bt_field_class_bit_array_borrow_flag_by_label_const(
		const struct bt_field_class *fc, const char *label)
{
	struct bt_field_class_bit_array *ba_fc = (void *) fc;
	const struct bt_field_class_bit_array_flag *flag = NULL;
	uint64_t i;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "bit-array",
		BT_FIELD_CLASS_TYPE_BIT_ARRAY, "Field class");
	BT_ASSERT_PRE_DEV_NON_NULL("label", label, "Label");

	for (i = 0; i < ba_fc->flags->len; i++) {
		const struct bt_field_class_bit_array_flag *candidate =
			ba_fc->flags->pdata[i];

		if (strcmp(candidate->label, label) == 0) {
			flag = candidate;
			break;
		}
	}

	return flag;
}

BT_EXPORT
bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer_status
bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer(
		const struct bt_field_class *fc, uint64_t value_as_integer,
		bt_field_class_bit_array_flag_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_class_bit_array *ba_fc = (void *) fc;
	uint64_t i;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "bit-array",
		BT_FIELD_CLASS_TYPE_BIT_ARRAY, "Field class");
	BT_ASSERT_PRE_DEV_NON_NULL("label-array-output", label_array,
		"Label array (output)");
	BT_ASSERT_PRE_DEV_NON_NULL("count-output", count, "Count (output)");

	g_ptr_array_set_size(ba_fc->label_buf, 0);

	for (i = 0; i < ba_fc->flags->len; ++i) {
		const struct bt_field_class_bit_array_flag *flag =
			ba_fc->flags->pdata[i];

		if (value_as_integer & flag->mask) {
			g_ptr_array_add(ba_fc->label_buf, flag->label);
		}
	}

	*label_array = (void *) ba_fc->label_buf->pdata;
	*count = (uint64_t) ba_fc->label_buf->len;

	return BT_FIELD_CLASS_BIT_ARRAY_GET_ACTIVE_FLAG_LABELS_FOR_VALUE_AS_INTEGER_STATUS_OK;
}

BT_EXPORT
const char *bt_field_class_bit_array_flag_get_label(
		const struct bt_field_class_bit_array_flag *flag)
{
	BT_ASSERT_PRE_DEV_BIT_ARRAY_FC_FLAG_NON_NULL(flag);

	return flag->label;
}

BT_EXPORT
const bt_integer_range_set_unsigned *
bt_field_class_bit_array_flag_borrow_index_ranges_const(
		const struct bt_field_class_bit_array_flag *flag)
{
	BT_ASSERT_PRE_DEV_BIT_ARRAY_FC_FLAG_NON_NULL(flag);

	return flag->range_set;
}

static
void destroy_bool_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying boolean field class object: %!+F", obj);
	finalize_field_class((void *) obj);
	g_free(obj);
}

BT_EXPORT
struct bt_field_class *bt_field_class_bool_create(
		bt_trace_class *trace_class)
{
	struct bt_field_class_bool *bool_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD("Creating default boolean field class object.");
	bool_fc = g_new0(struct bt_field_class_bool, 1);
	if (!bool_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one boolean field class.");
		goto error;
	}

	if (init_field_class((void *) bool_fc, BT_FIELD_CLASS_TYPE_BOOL,
			destroy_bool_field_class, trace_class)) {
		goto error;
	}

	BT_LIB_LOGD("Created boolean field class object: %!+F", bool_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(bool_fc);

end:
	return (void *) bool_fc;
}

static
int init_integer_field_class(struct bt_field_class_integer *fc,
		enum bt_field_class_type type,
		bt_object_release_func release_func,
		const struct bt_trace_class *trace_class)
{
	int ret;

	ret = init_field_class((void *) fc, type, release_func,
		trace_class);
	if (ret) {
		goto end;
	}

	fc->range = 64;
	fc->base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL;

end:
	return ret;
}

static
void destroy_integer_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying integer field class object: %!+F", obj);
	finalize_field_class((void *) obj);
	g_free(obj);
}

static inline
struct bt_field_class *create_integer_field_class(bt_trace_class *trace_class,
		enum bt_field_class_type type)
{
	struct bt_field_class_integer *int_fc = NULL;

	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD("Creating default integer field class object: type=%s",
		bt_common_field_class_type_string(type));
	int_fc = g_new0(struct bt_field_class_integer, 1);
	if (!int_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one integer field class.");
		goto error;
	}

	if (init_integer_field_class(int_fc, type,
			destroy_integer_field_class, trace_class)) {
		goto error;
	}

	BT_LIB_LOGD("Created integer field class object: %!+F", int_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(int_fc);

end:
	return (void *) int_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_integer_unsigned_create(
		bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();

	return create_integer_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER);
}

BT_EXPORT
struct bt_field_class *bt_field_class_integer_signed_create(
		bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();

	return create_integer_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_SIGNED_INTEGER);
}

BT_EXPORT
uint64_t bt_field_class_integer_get_field_value_range(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_integer *int_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_INT("field-class", fc, "Field class");
	return int_fc->range;
}

static
bool size_is_valid_for_enumeration_field_class(
		struct bt_field_class *fc __attribute__((unused)),
		uint64_t size __attribute__((unused)))
{
	// TODO
	return true;
}

BT_EXPORT
void bt_field_class_integer_set_field_value_range(
		struct bt_field_class *fc, uint64_t size)
{
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_INT("field-class", fc, "Field class");
	BT_ASSERT_PRE_DEV_FC_HOT(fc);
	BT_ASSERT_PRE("valid-n",
		size >= 1 && size <= 64,
		"Unsupported size for integer field class's field value range "
		"(minimum is 1, maximum is 64): size=%" PRIu64, size);
	BT_ASSERT_PRE("valid-n-for-integer-field-class",
		int_fc->common.type == BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER ||
		int_fc->common.type == BT_FIELD_CLASS_TYPE_SIGNED_INTEGER ||
		size_is_valid_for_enumeration_field_class(fc, size),
		"Invalid field value range for integer field class: "
		"at least one of the current mapping ranges contains values "
		"which are outside this range: %!+F, size=%" PRIu64, fc, size);
	int_fc->range = size;
	BT_LIB_LOGD("Set integer field class's field value range: %!+F", fc);
}

BT_EXPORT
void bt_field_class_integer_set_field_value_hints(
		struct bt_field_class *fc, uint64_t hints)
{
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_INT("field-class", fc, "Field class");
	BT_ASSERT_PRE_DEV_FC_HOT(fc);
	BT_ASSERT_PRE_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_PRE("hint-exists",
		hints == 0 || hints == BT_FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL,
		"Integral hints value contains an unknown hint: "
		"%!+F, hints=%" PRIu64, fc, hints);
	int_fc->hints = hints;
	BT_LIB_LOGD("Set integer field class's field value hints: %!+F", fc);
}

BT_EXPORT
uint64_t bt_field_class_integer_get_field_value_hints(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_integer *int_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_INT("field-class", fc, "Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_GE(fc, 1);
	return int_fc->hints;
}

BT_EXPORT
enum bt_field_class_integer_preferred_display_base
bt_field_class_integer_get_preferred_display_base(const struct bt_field_class *fc)
{
	const struct bt_field_class_integer *int_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_INT("field-class", fc, "Field class");
	return int_fc->base;
}

BT_EXPORT
void bt_field_class_integer_set_preferred_display_base(
		struct bt_field_class *fc,
		enum bt_field_class_integer_preferred_display_base base)
{
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_INT("field-class", fc, "Field class");
	BT_ASSERT_PRE_DEV_FC_HOT(fc);
	int_fc->base = base;
	BT_LIB_LOGD("Set integer field class's preferred display base: %!+F", fc);
}

static
void finalize_enumeration_field_class_mapping(
		struct bt_field_class_enumeration_mapping *mapping)
{
	BT_ASSERT(mapping);

	if (mapping->label) {
		g_string_free(mapping->label, TRUE);
		mapping->label = NULL;
	}

	BT_OBJECT_PUT_REF_AND_RESET(mapping->range_set);
}

static
void destroy_enumeration_field_class(struct bt_object *obj)
{
	struct bt_field_class_enumeration *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying enumeration field class object: %!+F", fc);
	finalize_field_class((void *) obj);

	if (fc->mappings) {
		uint64_t i;

		for (i = 0; i < fc->mappings->len; i++) {
			finalize_enumeration_field_class_mapping(
				BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, i));
		}

		g_array_free(fc->mappings, TRUE);
		fc->mappings = NULL;
	}

	if (fc->label_buf) {
		g_ptr_array_free(fc->label_buf, TRUE);
		fc->label_buf = NULL;
	}

	g_free(fc);
}

static
struct bt_field_class *create_enumeration_field_class(
		bt_trace_class *trace_class, enum bt_field_class_type type)
{
	struct bt_field_class_enumeration *enum_fc = NULL;

	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD("Creating default enumeration field class object: type=%s",
		bt_common_field_class_type_string(type));
	enum_fc = g_new0(struct bt_field_class_enumeration, 1);
	if (!enum_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one enumeration field class.");
		goto error;
	}

	if (init_integer_field_class((void *) enum_fc, type,
			destroy_enumeration_field_class, trace_class)) {
		goto error;
	}

	enum_fc->mappings = g_array_new(FALSE, TRUE,
		sizeof(struct bt_field_class_enumeration_mapping));
	if (!enum_fc->mappings) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GArray.");
		goto error;
	}

	enum_fc->label_buf = g_ptr_array_new();
	if (!enum_fc->label_buf) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		goto error;
	}

	BT_LIB_LOGD("Created enumeration field class object: %!+F", enum_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(enum_fc);

end:
	return (void *) enum_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_enumeration_unsigned_create(
		bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();

	return create_enumeration_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION);
}

BT_EXPORT
struct bt_field_class *bt_field_class_enumeration_signed_create(
		bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();

	return create_enumeration_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION);
}

BT_EXPORT
uint64_t bt_field_class_enumeration_get_mapping_count(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_enumeration *enum_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_ENUM("field-class", fc, "Field class");
	return (uint64_t) enum_fc->mappings->len;
}

BT_EXPORT
const struct bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	const struct bt_field_class_enumeration *enum_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, enum_fc->mappings->len);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "unsigned-enumeration",
		BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION, "Field class");
	return (const void *) BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, index);
}

BT_EXPORT
const struct bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	const struct bt_field_class_enumeration *enum_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, enum_fc->mappings->len);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "signed-enumeration",
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION, "Field class");
	return (const void *) BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, index);
}

static
const struct bt_field_class_enumeration_mapping *
borrow_enumeration_field_class_mapping_by_label(
		const struct bt_field_class_enumeration *fc, const char *label,
		const char *api_func)
{
	struct bt_field_class_enumeration_mapping *mapping = NULL;
	uint64_t i;

	BT_ASSERT_DBG(fc);
	BT_ASSERT_PRE_DEV_NON_NULL_FROM_FUNC(api_func, "label", label,
		"Label");

	for (i = 0; i < fc->mappings->len; i++) {
		struct bt_field_class_enumeration_mapping *this_mapping =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, i);

		if (strcmp(this_mapping->label->str, label) == 0) {
			mapping = this_mapping;
			goto end;
		}
	}

end:
	return mapping;
}

BT_EXPORT
const struct bt_field_class_enumeration_signed_mapping *
bt_field_class_enumeration_signed_borrow_mapping_by_label_const(
		const struct bt_field_class *fc, const char *label)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "signed-enumeration",
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION, "Field class");
	return (const void *) borrow_enumeration_field_class_mapping_by_label(
		(const void *) fc, label, __func__);
}

BT_EXPORT
const struct bt_field_class_enumeration_unsigned_mapping *
bt_field_class_enumeration_unsigned_borrow_mapping_by_label_const(
		const struct bt_field_class *fc, const char *label)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "unsigned-enumeration",
		BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION, "Field class");
	return (const void *) borrow_enumeration_field_class_mapping_by_label(
		(const void *) fc, label, __func__);
}

BT_EXPORT
const char *bt_field_class_enumeration_mapping_get_label(
		const struct bt_field_class_enumeration_mapping *mapping)
{
	BT_ASSERT_PRE_DEV_NON_NULL("enumeration-field-class-mapping",
		mapping, "Enumeration field class mapping");
	return mapping->label->str;
}

BT_EXPORT
const struct bt_integer_range_set_unsigned *
bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
		const struct bt_field_class_enumeration_unsigned_mapping *u_mapping)
{
	const struct bt_field_class_enumeration_mapping *mapping =
		(const void *) u_mapping;

	BT_ASSERT_PRE_DEV_NON_NULL("enumeration-field-class-mapping",
		mapping, "Enumeration field class mapping");
	return (const void *) mapping->range_set;
}

BT_EXPORT
const struct bt_integer_range_set_signed *
bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
		const struct bt_field_class_enumeration_signed_mapping *s_mapping)
{
	const struct bt_field_class_enumeration_mapping *mapping =
		(const void *) s_mapping;

	BT_ASSERT_PRE_DEV_NON_NULL("enumeration-field-class-mapping",
		mapping, "Enumeration field class mapping");
	return (const void *) mapping->range_set;
}

BT_EXPORT
enum bt_field_class_enumeration_get_mapping_labels_for_value_status
bt_field_class_enumeration_unsigned_get_mapping_labels_for_value(
		const struct bt_field_class *fc, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	const struct bt_field_class_enumeration *enum_fc = (const void *) fc;
	uint64_t i;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_NON_NULL("label-array-output", label_array,
		"Label array (output)");
	BT_ASSERT_PRE_DEV_NON_NULL("count-output", count, "Count (output)");
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "unsigned-enumeration",
		BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION, "Field class");
	g_ptr_array_set_size(enum_fc->label_buf, 0);

	for (i = 0; i < enum_fc->mappings->len; i++) {
		uint64_t j;
		const struct bt_field_class_enumeration_mapping *mapping =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc, i);

		for (j = 0; j < mapping->range_set->ranges->len; j++) {
			const struct bt_integer_range *range = (const void *)
				BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
					mapping->range_set, j);

			if (value >= range->lower.u &&
					value <= range->upper.u) {
				g_ptr_array_add(enum_fc->label_buf,
					mapping->label->str);
				break;
			}
		}
	}

	*label_array = (void *) enum_fc->label_buf->pdata;
	*count = (uint64_t) enum_fc->label_buf->len;
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
enum bt_field_class_enumeration_get_mapping_labels_for_value_status
bt_field_class_enumeration_signed_get_mapping_labels_for_value(
		const struct bt_field_class *fc, int64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	const struct bt_field_class_enumeration *enum_fc = (const void *) fc;
	uint64_t i;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_NON_NULL("label-array-output", label_array,
		"Label array (output)");
	BT_ASSERT_PRE_DEV_NON_NULL("count-output", count, "Count (output)");
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc, "signed-enumeration",
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION, "Field class");
	g_ptr_array_set_size(enum_fc->label_buf, 0);

	for (i = 0; i < enum_fc->mappings->len; i++) {
		uint64_t j;
		const struct bt_field_class_enumeration_mapping *mapping =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc, i);

		for (j = 0; j < mapping->range_set->ranges->len; j++) {
			const struct bt_integer_range *range = (const void *)
				BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
					mapping->range_set, j);

			if (value >= range->lower.i &&
					value <= range->upper.i) {
				g_ptr_array_add(enum_fc->label_buf,
					mapping->label->str);
				break;
			}
		}
	}

	*label_array = (void *) enum_fc->label_buf->pdata;
	*count = (uint64_t) enum_fc->label_buf->len;
	return BT_FUNC_STATUS_OK;
}

static
bool enumeration_field_class_has_mapping_with_label(
		const struct bt_field_class_enumeration *enum_fc,
		const char *label)
{
	uint64_t i;
	bool exists = false;

	BT_ASSERT(enum_fc);
	BT_ASSERT(label);

	for (i = 0; i < enum_fc->mappings->len; i++) {
		struct bt_field_class_enumeration_mapping *mapping_candidate =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc, i);

		if (strcmp(mapping_candidate->label->str, label) == 0) {
			exists = true;
			goto end;
		}
	}

end:
	return exists;
}

static inline
enum bt_field_class_enumeration_add_mapping_status
add_mapping_to_enumeration_field_class(struct bt_field_class *fc,
		const char *label, const struct bt_integer_range_set *range_set,
		const char *api_func)
{
	enum bt_field_class_enumeration_add_mapping_status status =
		BT_FUNC_STATUS_OK;
	struct bt_field_class_enumeration *enum_fc = (void *) fc;
	struct bt_field_class_enumeration_mapping mapping = { 0	};

	BT_ASSERT_PRE_NO_ERROR_FROM_FUNC(api_func);
	BT_ASSERT(fc);
	BT_ASSERT_PRE_NON_NULL_FROM_FUNC(api_func, "label", label, "Label");
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL_FROM_FUNC(api_func, range_set);
	BT_ASSERT_PRE_FROM_FUNC(api_func,
		"enumeration-field-class-mapping-label-is-unique",
		!enumeration_field_class_has_mapping_with_label(
			enum_fc, label),
		"Duplicate mapping name in enumeration field class: "
		"%![enum-fc-]+F, label=\"%s\"", fc, label);
	mapping.range_set = range_set;
	bt_object_get_ref(mapping.range_set);
	mapping.label = g_string_new(label);
	if (!mapping.label) {
		finalize_enumeration_field_class_mapping(&mapping);
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	g_array_append_val(enum_fc->mappings, mapping);
	BT_LIB_LOGD("Added mapping to enumeration field class: "
		"%![fc-]+F, label=\"%s\"", fc, label);

end:
	return status;
}

BT_EXPORT
enum bt_field_class_enumeration_add_mapping_status
bt_field_class_enumeration_unsigned_add_mapping(
		struct bt_field_class *fc, const char *label,
		const struct bt_integer_range_set_unsigned *range_set)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"unsigned-enumeration-field-class",
		BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION, "Field class");
	return add_mapping_to_enumeration_field_class(fc, label,
		(const void *) range_set, __func__);
}

BT_EXPORT
enum bt_field_class_enumeration_add_mapping_status
bt_field_class_enumeration_signed_add_mapping(
		struct bt_field_class *fc, const char *label,
		const struct bt_integer_range_set_signed *range_set)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"signed-enumeration-field-class",
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION, "Field class");
	return add_mapping_to_enumeration_field_class(fc, label,
		(const void *) range_set, __func__);
}

static
void destroy_real_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying real field class object: %!+F", obj);
	finalize_field_class((void *) obj);
	g_free(obj);
}

static
struct bt_field_class *create_real_field_class(bt_trace_class *trace_class,
		enum bt_field_class_type type)
{
	struct bt_field_class_real *real_fc = NULL;

	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD("Creating default real field class object: type=%s",
		bt_common_field_class_type_string(type));
	real_fc = g_new0(struct bt_field_class_real, 1);
	if (!real_fc) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one real field class.");
		goto error;
	}

	if (init_field_class((void *) real_fc, type, destroy_real_field_class,
			trace_class)) {
		goto error;
	}

	BT_LIB_LOGD("Created real field class object: %!+F", real_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(real_fc);

end:
	return (void *) real_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_real_single_precision_create(
	bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();

	return create_real_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL);
}

BT_EXPORT
struct bt_field_class *bt_field_class_real_double_precision_create(
	bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();

	return create_real_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL);
}

static
int init_named_field_classes_container(
		struct bt_field_class_named_field_class_container *fc,
		enum bt_field_class_type type,
		bt_object_release_func fc_release_func,
		GDestroyNotify named_fc_destroy_func,
		const struct bt_trace_class *trace_class)
{
	int ret = 0;

	ret = init_field_class((void *) fc, type, fc_release_func,
		trace_class);
	if (ret) {
		goto end;
	}

	fc->named_fcs = g_ptr_array_new_with_free_func(named_fc_destroy_func);
	if (!fc->named_fcs) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		ret = -1;
		goto end;
	}

	fc->name_to_index = g_hash_table_new(g_str_hash, g_str_equal);
	if (!fc->name_to_index) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GHashTable.");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
void finalize_named_field_class(struct bt_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);
	BT_LIB_LOGD("Finalizing named field class: "
		"addr=%p, name=\"%s\", %![fc-]+F",
		named_fc, named_fc->name ? named_fc->name->str : NULL,
		named_fc->fc);
	BT_OBJECT_PUT_REF_AND_RESET(named_fc->user_attributes);

	if (named_fc->name) {
		g_string_free(named_fc->name, TRUE);
		named_fc->name = NULL;
	}

	BT_LOGD_STR("Putting named field class's field class.");
	BT_OBJECT_PUT_REF_AND_RESET(named_fc->fc);
}

static
void destroy_named_field_class(gpointer ptr)
{
	struct bt_named_field_class *named_fc = ptr;

	if (ptr) {
		BT_OBJECT_PUT_REF_AND_RESET(named_fc->user_attributes);
		finalize_named_field_class(ptr);
		g_free(ptr);
	}
}

static
void destroy_variant_with_selector_field_option(gpointer ptr)
{
	struct bt_field_class_variant_with_selector_field_option *opt = ptr;

	if (ptr) {
		finalize_named_field_class(&opt->common);
		BT_OBJECT_PUT_REF_AND_RESET(opt->range_set);
		g_free(ptr);
	}
}

static
void finalize_named_field_classes_container(
		struct bt_field_class_named_field_class_container *fc)
{
	BT_ASSERT(fc);

	if (fc->named_fcs) {
		g_ptr_array_free(fc->named_fcs, TRUE);
		fc->named_fcs = NULL;

	}

	if (fc->name_to_index) {
		g_hash_table_destroy(fc->name_to_index);
		fc->name_to_index = NULL;
	}
}

static
void destroy_structure_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying structure field class object: %!+F", obj);
	finalize_field_class((void *) obj);
	finalize_named_field_classes_container((void *) obj);
	g_free(obj);
}

BT_EXPORT
struct bt_field_class *bt_field_class_structure_create(
		bt_trace_class *trace_class)
{
	int ret;
	struct bt_field_class_structure *struct_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD_STR("Creating default structure field class object.");
	struct_fc = g_new0(struct bt_field_class_structure, 1);
	if (!struct_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one structure field class.");
		goto error;
	}

	ret = init_named_field_classes_container((void *) struct_fc,
		BT_FIELD_CLASS_TYPE_STRUCTURE, destroy_structure_field_class,
		destroy_named_field_class, trace_class);
	if (ret) {
		/* init_named_field_classes_container() logs errors */
		goto error;
	}

	BT_LIB_LOGD("Created structure field class object: %!+F", struct_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(struct_fc);

end:
	return (void *) struct_fc;
}

static
int init_named_field_class(struct bt_named_field_class *named_fc,
		const char *name, struct bt_field_class *fc)
{
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT(named_fc);
	BT_ASSERT(fc);

	if (name) {
		named_fc->name = g_string_new(name);
		if (!named_fc->name) {
			BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GString.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}
	}

	named_fc->user_attributes = bt_value_map_create();
	if (!named_fc->user_attributes) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create a map value object.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	named_fc->fc = fc;
	bt_object_get_ref_no_null_check(named_fc->fc);

end:
	return status;
}

static
struct bt_named_field_class *create_named_field_class(const char *name,
		struct bt_field_class *fc)
{
	struct bt_named_field_class *named_fc = g_new0(
		struct bt_named_field_class, 1);

	if (!named_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate a named field class.");
		goto error;
	}

	if (init_named_field_class(named_fc, name, fc)) {
		/* init_named_field_class() logs errors */
		goto error;
	}

	goto end;

error:
	destroy_named_field_class(named_fc);
	named_fc = NULL;

end:
	return named_fc;
}

static
struct bt_field_class_variant_with_selector_field_option *
create_variant_with_selector_field_option(
		const char *name, struct bt_field_class *fc,
		const struct bt_integer_range_set *range_set)
{
	struct bt_field_class_variant_with_selector_field_option *opt = g_new0(
		struct bt_field_class_variant_with_selector_field_option, 1);

	BT_ASSERT(range_set);

	if (!opt) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate a named field class.");
		goto error;
	}

	if (init_named_field_class(&opt->common, name, fc)) {
		goto error;
	}

	opt->range_set = range_set;
	bt_object_get_ref_no_null_check(opt->range_set);
	bt_integer_range_set_freeze(range_set);
	goto end;

error:
	destroy_variant_with_selector_field_option(opt);
	opt = NULL;

end:
	return opt;
}

static
int append_named_field_class_to_container_field_class(
		struct bt_field_class_named_field_class_container *container_fc,
		struct bt_named_field_class *named_fc, const char *api_func,
		const char *unique_entry_precond_id)
{
	BT_ASSERT(container_fc);
	BT_ASSERT(named_fc);
	BT_ASSERT_PRE_DEV_FC_HOT_FROM_FUNC(api_func, container_fc);
	BT_ASSERT_PRE_FROM_FUNC(api_func, unique_entry_precond_id,
		!named_fc->name ||
			!bt_g_hash_table_contains(container_fc->name_to_index,
				named_fc->name->str),
		"Duplicate member/option name in structure/variant field class: "
		"%![container-fc-]+F, name=\"%s\"", container_fc,
		named_fc->name->str);

	/*
	 * Freeze the contained field class, but not the named field
	 * class itself, as it's still possible afterwards to modify
	 * properties of the member/option object.
	 */
	bt_field_class_freeze(named_fc->fc);
	g_ptr_array_add(container_fc->named_fcs, named_fc);

	if (named_fc->name) {
		/*
		 * MIP > 0: a variant field class option may have no
		 * name.
		 */
		g_hash_table_insert(container_fc->name_to_index,
			named_fc->name->str,
			GUINT_TO_POINTER(container_fc->named_fcs->len - 1));
	}

	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
enum bt_field_class_structure_append_member_status
bt_field_class_structure_append_member(
		struct bt_field_class *fc, const char *name,
		struct bt_field_class *member_fc)
{
	enum bt_field_class_structure_append_member_status status;
	struct bt_named_field_class *named_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_NAME_NON_NULL(name);
	BT_ASSERT_PRE_FC_IS_STRUCT("field-class", fc, "Field class");
	named_fc = create_named_field_class(name, member_fc);
	if (!named_fc) {
		/* create_named_field_class() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	status = append_named_field_class_to_container_field_class((void *) fc,
		named_fc, __func__,
		"structure-field-class-member-name-is-unique");
	if (status == BT_FUNC_STATUS_OK) {
		/* Moved to the container */
		named_fc = NULL;
	}

end:
	return status;
}

BT_EXPORT
uint64_t bt_field_class_structure_get_member_count(
		const struct bt_field_class *fc)
{
	struct bt_field_class_structure *struct_fc = (void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_STRUCT("field-class", fc, "Field class");
	return (uint64_t) struct_fc->common.named_fcs->len;
}

static
struct bt_named_field_class *
borrow_named_field_class_from_container_field_class_at_index(
		struct bt_field_class_named_field_class_container *fc,
		uint64_t index, const char *api_func)
{
	BT_ASSERT_DBG(fc);
	BT_ASSERT_PRE_DEV_VALID_INDEX_FROM_FUNC(api_func, index,
		fc->named_fcs->len);
	return fc->named_fcs->pdata[index];
}

BT_EXPORT
const struct bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_STRUCT("field-class", fc, "Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_at_index(
			(void *) fc, index, __func__);
}

BT_EXPORT
struct bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_index(
		struct bt_field_class *fc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_STRUCT("field-class", fc, "Field class");
	return (void *)
		borrow_named_field_class_from_container_field_class_at_index(
			(void *) fc, index, __func__);
}

static
struct bt_named_field_class *
borrow_named_field_class_from_container_field_class_by_name(
		struct bt_field_class_named_field_class_container *fc,
		const char *name, const char *api_func)
{
	struct bt_named_field_class *named_fc = NULL;
	gpointer orig_key;
	gpointer value;

	BT_ASSERT_DBG(fc);
	BT_ASSERT_PRE_DEV_NAME_NON_NULL_FROM_FUNC(api_func, name);
	if (!g_hash_table_lookup_extended(fc->name_to_index, name, &orig_key,
			&value)) {
		goto end;
	}

	named_fc = fc->named_fcs->pdata[GPOINTER_TO_UINT(value)];

end:
	return named_fc;
}

BT_EXPORT
const struct bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name_const(
		const struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_STRUCT("field-class", fc, "Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_by_name(
			(void *) fc, name, __func__);
}

BT_EXPORT
struct bt_field_class_structure_member *
bt_field_class_structure_borrow_member_by_name(
		struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_STRUCT("field-class", fc, "Field class");
	return (void *)
		borrow_named_field_class_from_container_field_class_by_name(
			(void *) fc, name, __func__);
}

BT_EXPORT
const char *bt_field_class_structure_member_get_name(
		const struct bt_field_class_structure_member *member)
{
	const struct bt_named_field_class *named_fc = (const void *) member;

	BT_ASSERT_PRE_DEV_STRUCT_FC_MEMBER_NON_NULL(member);
	BT_ASSERT_DBG(named_fc->name);
	return named_fc->name->str;
}

BT_EXPORT
const struct bt_field_class *
bt_field_class_structure_member_borrow_field_class_const(
		const struct bt_field_class_structure_member *member)
{
	const struct bt_named_field_class *named_fc = (const void *) member;

	BT_ASSERT_PRE_DEV_STRUCT_FC_MEMBER_NON_NULL(member);
	return named_fc->fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_structure_member_borrow_field_class(
		struct bt_field_class_structure_member *member)
{
	struct bt_named_field_class *named_fc = (void *) member;

	BT_ASSERT_PRE_DEV_STRUCT_FC_MEMBER_NON_NULL(member);
	return named_fc->fc;
}

static
void destroy_option_field_class(struct bt_object *obj)
{
	struct bt_field_class_option *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying option field class object: %!+F", fc);
	finalize_field_class((void *) obj);
	BT_LOGD_STR("Putting content field class.");
	BT_OBJECT_PUT_REF_AND_RESET(fc->content_fc);

	if (fc->common.type != BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD) {
		struct bt_field_class_option_with_selector_field *with_sel_fc =
			(void *) obj;

		switch (with_sel_fc->selector_field_xref_kind) {
		case FIELD_XREF_KIND_PATH:
			BT_LOGD_STR("Putting selector field path.");
			BT_OBJECT_PUT_REF_AND_RESET(with_sel_fc->selector_field.path.path);
			BT_LOGD_STR("Putting selector field class.");
			BT_OBJECT_PUT_REF_AND_RESET(with_sel_fc->selector_field.path.class);
			break;
		case FIELD_XREF_KIND_LOCATION:
			BT_LOGD_STR("Putting selector field location.");
			BT_OBJECT_PUT_REF_AND_RESET(with_sel_fc->selector_field.location);
			break;
		};

		if (fc->common.type != BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD) {
			struct bt_field_class_option_with_selector_field_integer *with_int_sel_fc =
				(void *) obj;

			BT_LOGD_STR("Putting integer range set.");
			BT_OBJECT_PUT_REF_AND_RESET(with_int_sel_fc->range_set);
		}
	}

	g_free(fc);
}

static
struct bt_field_class *create_option_field_class(
		struct bt_trace_class *trace_class,
		enum bt_field_class_type fc_type,
		struct bt_field_class *content_fc,
		struct bt_field_class *selector_fc,
		const struct bt_field_location *selector_fl,
		const char *api_func)
{
	struct bt_field_class_option *opt_fc = NULL;

	BT_ASSERT_PRE_NON_NULL_FROM_FUNC(api_func, "content-field-class",
		content_fc, "Content field class");

	BT_ASSERT(!(selector_fc && selector_fl));

	BT_LIB_LOGD("Creating option field class: "
		"type=%s, %![content-fc-]+F, %![sel-fc-]+F",
		bt_common_field_class_type_string(fc_type),
		content_fc, selector_fc);

	if (fc_type != BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD) {
		struct bt_field_class_option_with_selector_field *opt_with_sel_fc = NULL;

		BT_ASSERT(selector_fc || selector_fl);

		if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD) {
			opt_with_sel_fc = (void *) g_new0(
				struct bt_field_class_option_with_selector_field_bool, 1);
		} else {
			opt_with_sel_fc = (void *) g_new0(
				struct bt_field_class_option_with_selector_field_integer, 1);
		}

		if (!opt_with_sel_fc) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to allocate one option with selector field class.");
			goto error;
		}

		if (selector_fc) {
			opt_with_sel_fc->selector_field_xref_kind = FIELD_XREF_KIND_PATH;
			opt_with_sel_fc->selector_field.path.class = selector_fc;
			bt_object_get_ref_no_null_check(opt_with_sel_fc->selector_field.path.class);
		} else {
			opt_with_sel_fc->selector_field_xref_kind = FIELD_XREF_KIND_LOCATION;
			opt_with_sel_fc->selector_field.location = selector_fl;
			bt_object_get_ref_no_null_check(opt_with_sel_fc->selector_field.location);
		}

		opt_fc = (void *) opt_with_sel_fc;
	} else {
		BT_ASSERT(!selector_fc);
		BT_ASSERT(!selector_fl);

		opt_fc = g_new0(struct bt_field_class_option, 1);
		if (!opt_fc) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to allocate one option field class.");
			goto error;
		}
	}

	BT_ASSERT(opt_fc);

	if (init_field_class((void *) opt_fc, fc_type,
			destroy_option_field_class, trace_class)) {
		goto error;
	}

	opt_fc->content_fc = content_fc;
	bt_object_get_ref_no_null_check(opt_fc->content_fc);
	bt_field_class_freeze(opt_fc->content_fc);

	if (selector_fc) {
		bt_field_class_freeze(selector_fc);
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(opt_fc);

end:
	return (void *) opt_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_option_without_selector_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc)
{
	struct bt_field_class *fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_EQ(trace_class, 0);

	fc = create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD,
		content_fc, NULL, NULL, __func__);

	BT_LIB_LOGD("Created option field class without selector field class: "
		"%![opt-fc-]+F", fc);

	return fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_option_without_selector_field_location_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc)
{
	struct bt_field_class *fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	fc = create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD,
		content_fc, NULL, NULL, __func__);

	BT_LIB_LOGD("Created option field class without selector field location: "
		"%![opt-fc-]+F", fc);

	return fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_option_with_selector_field_bool_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc,
		struct bt_field_class *selector_fc)
{
	struct bt_field_class *fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_EQ(trace_class, 0);
	BT_ASSERT_PRE_SELECTOR_FC_NON_NULL(selector_fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("selector-field-class", selector_fc,
			"boolean-field-class", BT_FIELD_CLASS_TYPE_BOOL,
			"Selector field class");

	fc = create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD,
		content_fc, selector_fc, NULL, __func__);

	BT_LIB_LOGD("Created option field class with boolean selector field class: "
		"%![opt-fc-]+F, %![sel-fc-]+F", fc, selector_fc);

	return fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_option_with_selector_field_location_bool_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc,
		const struct bt_field_location *selector_fl)
{
	struct bt_field_class *fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);
	BT_ASSERT_PRE_SELECTOR_FL_NON_NULL(selector_fl);

	fc = create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD,
		content_fc, NULL, selector_fl, __func__);

	BT_LIB_LOGD("Created option field class with boolean selector field location: "
		"%![opt-fc-]+F, %![sel-fl-]+L", fc, selector_fl);

	return fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_option_with_selector_field_integer_unsigned_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc,
		struct bt_field_class *selector_fc,
		const struct bt_integer_range_set_unsigned *u_range_set)
{
	struct bt_field_class_option_with_selector_field_integer *fc;
	const struct bt_integer_range_set *range_set =
		(const void *) u_range_set;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_EQ(trace_class, 0);
	BT_ASSERT_PRE_SELECTOR_FC_NON_NULL(selector_fc);
	BT_ASSERT_PRE_FC_IS_UNSIGNED_INT("selector-field-class",
		selector_fc, "Selector field class");
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL(range_set);
	BT_ASSERT_PRE_INT_RANGE_SET_NOT_EMPTY(range_set);

 	fc = (void *) create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		content_fc, selector_fc, NULL, __func__);
	if (!fc) {
		goto end;
	}

	fc->range_set = range_set;
	bt_object_get_ref_no_null_check(fc->range_set);
	bt_integer_range_set_freeze(range_set);

	BT_LIB_LOGD("Created option field class with unsigned integer selector field class: "
		"%![opt-fc-]+F, %![sel-fc-]+F", fc, selector_fc);

end:
	return (void *) fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_option_with_selector_field_location_integer_unsigned_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc,
		const struct bt_field_location *selector_fl,
		const struct bt_integer_range_set_unsigned *u_range_set)
{
	struct bt_field_class_option_with_selector_field_integer *fc;
	const struct bt_integer_range_set *range_set =
		(const void *) u_range_set;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);
	BT_ASSERT_PRE_SELECTOR_FL_NON_NULL(selector_fl);
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL(range_set);
	BT_ASSERT_PRE_INT_RANGE_SET_NOT_EMPTY(range_set);

 	fc = (void *) create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		content_fc, NULL, selector_fl, __func__);
	if (!fc) {
		goto end;
	}

	fc->range_set = range_set;
	bt_object_get_ref_no_null_check(fc->range_set);
	bt_integer_range_set_freeze(range_set);

	BT_LIB_LOGD("Created option field class with unsigned integer selector field location: "
		"%![opt-fc-]+F, %![sel-fl-]+L", fc, selector_fl);

end:
	return (void *) fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_option_with_selector_field_integer_signed_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc,
		struct bt_field_class *selector_fc,
		const struct bt_integer_range_set_signed *i_range_set)
{
	struct bt_field_class_option_with_selector_field_integer *fc;
	const struct bt_integer_range_set *range_set =
		(const void *) i_range_set;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_EQ(trace_class, 0);
	BT_ASSERT_PRE_SELECTOR_FC_NON_NULL(selector_fc);
	BT_ASSERT_PRE_FC_IS_SIGNED_INT("selector-field-class",
		selector_fc, "Selector field class");
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL(range_set);
	BT_ASSERT_PRE_INT_RANGE_SET_NOT_EMPTY(range_set);

 	fc = (void *) create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		content_fc, selector_fc, NULL, __func__);
	if (!fc) {
		goto end;
	}

	fc->range_set = range_set;
	bt_object_get_ref_no_null_check(fc->range_set);
	bt_integer_range_set_freeze(range_set);

	BT_LIB_LOGD("Created option field class with signed integer selector field class: "
		"%![opt-fc-]+F, %![sel-fc-]+F", fc, selector_fc);

end:
	return (void *) fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_option_with_selector_field_location_integer_signed_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *content_fc,
		const struct bt_field_location *selector_fl,
		const struct bt_integer_range_set_signed *i_range_set)
{
	struct bt_field_class_option_with_selector_field_integer *fc;
	const struct bt_integer_range_set *range_set =
		(const void *) i_range_set;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);
	BT_ASSERT_PRE_SELECTOR_FL_NON_NULL(selector_fl);
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL(range_set);
	BT_ASSERT_PRE_INT_RANGE_SET_NOT_EMPTY(range_set);

 	fc = (void *) create_option_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		content_fc, NULL, selector_fl, __func__);
	if (!fc) {
		goto end;
	}

	fc->range_set = range_set;
	bt_object_get_ref_no_null_check(fc->range_set);
	bt_integer_range_set_freeze(range_set);

	BT_LIB_LOGD("Created option field class with signed integer selector field location: "
		"%![opt-fc-]+F, %![sel-fl-]+L", fc, selector_fl);

end:
	return (void *) fc;
}

BT_EXPORT
const struct bt_field_class *bt_field_class_option_borrow_field_class_const(
			const struct bt_field_class *fc)
{
	struct bt_field_class_option *opt_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_OPTION("field-class", fc, "Field class");
	return opt_fc->content_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_option_borrow_field_class(
			struct bt_field_class *fc)
{
	struct bt_field_class_option *opt_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_OPTION("field-class", fc, "Field class");
	return opt_fc->content_fc;
}

BT_EXPORT
const struct bt_field_path *
bt_field_class_option_with_selector_field_borrow_selector_field_path_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_option_with_selector_field *opt_fc =
		(const void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_OPTION_WITH_SEL("field-class", fc, "Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_EQ(fc, 0);
	BT_ASSERT_DBG(opt_fc->selector_field_xref_kind == FIELD_XREF_KIND_PATH);
	return opt_fc->selector_field.path.path;
}

BT_EXPORT
const struct bt_field_location *
bt_field_class_option_with_selector_field_borrow_selector_field_location_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_option_with_selector_field *opt_fc =
		(const void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_OPTION_WITH_SEL("field-class", fc, "Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_DBG(opt_fc->selector_field_xref_kind == FIELD_XREF_KIND_LOCATION);
	return opt_fc->selector_field.location;
}

BT_EXPORT
void bt_field_class_option_with_selector_field_bool_set_selector_is_reversed(
		struct bt_field_class *fc, bt_bool sel_is_reversed)
{
	struct bt_field_class_option_with_selector_field_bool *opt_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"option-field-class-with-boolean-selector-field",
		BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD,
		"Field class");
	BT_ASSERT_PRE_DEV_FC_HOT(fc);
	opt_fc->sel_is_reversed = sel_is_reversed;
}

BT_EXPORT
bt_bool bt_field_class_option_with_selector_field_bool_selector_is_reversed(
		const struct bt_field_class *fc)
{
	struct bt_field_class_option_with_selector_field_bool *opt_fc = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"option-field-class-with-boolean-selector-field",
		BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD,
		"Field class");
	return opt_fc->sel_is_reversed;
}

BT_EXPORT
const struct bt_integer_range_set_unsigned *
bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const(
		const struct bt_field_class *fc)
{
	struct bt_field_class_option_with_selector_field_integer *opt_fc =
		(void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_OPTION_WITH_INT_SEL("field-class", fc,
		"Field class");
	return (const void *) opt_fc->range_set;
}

BT_EXPORT
const struct bt_integer_range_set_signed *
bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const(
		const struct bt_field_class *fc)
{
	struct bt_field_class_option_with_selector_field_integer *opt_fc =
		(void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_OPTION_WITH_INT_SEL("field-class", fc,
		"Field class");
	return (const void *) opt_fc->range_set;
}

static
void finalize_variant_field_class(struct bt_field_class_variant *var_fc)
{
	BT_ASSERT(var_fc);
	BT_LIB_LOGD("Finalizing variant field class object: %!+F", var_fc);
	finalize_field_class((void *) var_fc);
	finalize_named_field_classes_container((void *) var_fc);
}

static
void destroy_variant_field_class(struct bt_object *obj)
{
	struct bt_field_class_variant *fc = (void *) obj;

	BT_ASSERT(fc);
	finalize_variant_field_class(fc);
	g_free(fc);
}

static
void destroy_variant_with_selector_field_field_class(struct bt_object *obj)
{
	struct bt_field_class_variant_with_selector_field *fc = (void *) obj;

	BT_ASSERT(fc);
	finalize_variant_field_class(&fc->common);

	if (fc->selector_field_xref_kind == FIELD_XREF_KIND_PATH) {
		BT_LOGD_STR("Putting selector field path.");
		BT_OBJECT_PUT_REF_AND_RESET(fc->selector_field.path.path);
		BT_LOGD_STR("Putting selector field class.");
		BT_OBJECT_PUT_REF_AND_RESET(fc->selector_field.path.class);
	} else {
		BT_ASSERT(fc->selector_field_xref_kind == FIELD_XREF_KIND_LOCATION);
		BT_LOGD_STR("Putting selector field location.");
		BT_OBJECT_PUT_REF_AND_RESET(fc->selector_field.location);
	}

	g_free(fc);
}

static
struct bt_field_class *create_variant_field_class(
		struct bt_trace_class *trace_class,
		enum bt_field_class_type fc_type,
		struct bt_field_class *selector_fc,
		const struct bt_field_location *selector_fl)
{
	int ret;
	struct bt_field_class_variant *var_fc = NULL;
	struct bt_field_class_variant_with_selector_field *var_with_sel_fc = NULL;

	if (fc_type != BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD) {
		BT_ASSERT((selector_fc && !selector_fl) ||
			(!selector_fc && selector_fl));


		if (selector_fc) {
			BT_LIB_LOGD("Creating default variant field class with selector field class: %![sel-fc-]+F",
				selector_fc);
		} else {
			BT_LIB_LOGD("Creating default variant field class with selector field location: %![sel-fl-]+L",
				selector_fl);
		}

		var_with_sel_fc = g_new0(
			struct bt_field_class_variant_with_selector_field, 1);
		if (!var_with_sel_fc) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to allocate one variant field class with selector.");
			goto error;
		}

		ret = init_named_field_classes_container(
			(void *) var_with_sel_fc, fc_type,
			destroy_variant_with_selector_field_field_class,
			destroy_variant_with_selector_field_option,
			trace_class);
		if (ret) {
			/* init_named_field_classes_container() logs errors */
			goto error;
		}

		if (selector_fc) {
			var_with_sel_fc->selector_field_xref_kind = FIELD_XREF_KIND_PATH;
			var_with_sel_fc->selector_field.path.class = selector_fc;
			bt_object_get_ref_no_null_check(selector_fc);
			bt_field_class_freeze(selector_fc);

			BT_LIB_LOGD("Created default variant field class with selector field class: "
				"%![var-fc-]+F, %![sel-fc-]+F", var_with_sel_fc, selector_fc);
		} else {
			var_with_sel_fc->selector_field_xref_kind = FIELD_XREF_KIND_LOCATION;
			var_with_sel_fc->selector_field.location = selector_fl;
			bt_object_get_ref_no_null_check(var_with_sel_fc->selector_field.location);

			BT_LIB_LOGD("Created default variant field class with selector field location: "
				"%![var-fc-]+F, %![sel-fl-]+L", var_fc, selector_fl);
		}

		var_fc = (void *) var_with_sel_fc;
	} else {
		BT_LIB_LOGD("Creating default variant field class without selector.");

		var_fc = g_new0(struct bt_field_class_variant, 1);
		if (!var_fc) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Failed to allocate one variant field class without selector.");
			goto error;
		}

		ret = init_named_field_classes_container((void *) var_fc, fc_type,
			destroy_variant_field_class, destroy_named_field_class,
			trace_class);
		if (ret) {
			/* init_named_field_classes_container() logs errors */
			goto error;
		}
		BT_LIB_LOGD("Created default variant field class without selector object: "
			"%![var-fc-]+F", var_fc);
	}

	BT_ASSERT(var_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(var_fc);
	BT_OBJECT_PUT_REF_AND_RESET(var_with_sel_fc);

end:
	return (void *) var_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_variant_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *selector_fc)
{
	enum bt_field_class_type fc_type;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_EQ(trace_class, 0);

	if (selector_fc) {
		bt_field_class_type selector_fc_type;

		BT_ASSERT_PRE_FC_IS_INT("selector-field-class", selector_fc,
			"Selector field class");

		selector_fc_type = bt_field_class_get_type(selector_fc);

		if (bt_field_class_type_is(selector_fc_type,
				BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER)) {
			fc_type = BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD;
		} else {
			fc_type = BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD;
		}
	} else {
		fc_type = BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD;
	}

	return create_variant_field_class(trace_class, fc_type, selector_fc, NULL);
}

BT_EXPORT
struct bt_field_class *bt_field_class_variant_without_selector_field_location_create(
		struct bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	return create_variant_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD, NULL, NULL);
}

BT_EXPORT
struct bt_field_class *bt_field_class_variant_with_selector_field_location_integer_unsigned_create(
		struct bt_trace_class *trace_class,
		const struct bt_field_location *selector_fl)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);
	BT_ASSERT_PRE_SELECTOR_FL_NON_NULL(selector_fl);

	return create_variant_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		NULL, selector_fl);
}

BT_EXPORT
struct bt_field_class *bt_field_class_variant_with_selector_field_location_integer_signed_create(
		struct bt_trace_class *trace_class,
		const struct bt_field_location *selector_fl)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);
	BT_ASSERT_PRE_SELECTOR_FL_NON_NULL(selector_fl);

	return create_variant_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		NULL, selector_fl);
}

#define VAR_FC_OPT_NAME_IS_UNIQUE_ID					\
	"variant-field-class-option-name-is-unique"

BT_EXPORT
enum bt_field_class_variant_without_selector_append_option_status
bt_field_class_variant_without_selector_append_option(struct bt_field_class *fc,
		const char *name, struct bt_field_class *option_fc)
{
	enum bt_field_class_variant_without_selector_append_option_status status;
	struct bt_named_field_class *named_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);

	/* Name is mandatory in MIP 0, optional later. */
	if (fc->mip_version == 0) {
		BT_ASSERT_PRE_NAME_NON_NULL(name);
	}

	BT_ASSERT_PRE_NON_NULL("option-field-class", option_fc,
		"Option field class");
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-without-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD,
		"Field class");
	named_fc = create_named_field_class(name, option_fc);
	if (!named_fc) {
		/* create_named_field_class() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	status = append_named_field_class_to_container_field_class((void *) fc,
		named_fc, __func__, VAR_FC_OPT_NAME_IS_UNIQUE_ID);
	if (status == BT_FUNC_STATUS_OK) {
		/* Moved to the container */
		named_fc = NULL;
	}

end:
	if (named_fc) {
		destroy_named_field_class(named_fc);
	}

	return status;
}

static
int ranges_overlap(GPtrArray *var_fc_opts, const struct bt_integer_range_set *range_set,
		bool is_signed, bool *has_overlap)
{
	int status = BT_FUNC_STATUS_OK;
	struct bt_integer_range_set *full_range_set;
	uint64_t i;

	*has_overlap = false;

	/*
	 * Build a single range set with all the ranges and test for
	 * overlaps.
	 */
	if (is_signed) {
		full_range_set = (void *) bt_integer_range_set_signed_create();
	} else {
		full_range_set = (void *) bt_integer_range_set_unsigned_create();
	}

	if (!full_range_set) {
		BT_LOGE_STR("Failed to create a range set.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	/* Add existing option ranges */
	for (i = 0; i < var_fc_opts->len; i++) {
		struct bt_field_class_variant_with_selector_field_option *opt =
			var_fc_opts->pdata[i];
		uint64_t j;

		for (j = 0; j < opt->range_set->ranges->len; j++) {
			struct bt_integer_range *range = BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
				opt->range_set, j);

			if (is_signed) {
				status = bt_integer_range_set_signed_add_range(
					(void *) full_range_set, range->lower.i,
					range->upper.i);
			} else {
				status = bt_integer_range_set_unsigned_add_range(
					(void *) full_range_set, range->lower.u,
					range->upper.u);
			}

			if (status) {
				goto end;
			}
		}
	}

	/* Add new ranges */
	for (i = 0; i < range_set->ranges->len; i++) {
		struct bt_integer_range *range = BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(
			range_set, i);

		if (is_signed) {
			status = bt_integer_range_set_signed_add_range(
				(void *) full_range_set, range->lower.i,
				range->upper.i);
		} else {
			status = bt_integer_range_set_unsigned_add_range(
				(void *) full_range_set, range->lower.u,
				range->upper.u);
		}

		if (status) {
			goto end;
		}
	}

	/* Check overlaps */
	if (is_signed) {
		*has_overlap = bt_integer_range_set_signed_has_overlaps(full_range_set);
	} else {
		*has_overlap = bt_integer_range_set_unsigned_has_overlaps(
			full_range_set);
	}

end:
	bt_object_put_ref(full_range_set);
	return status;
}

static
int append_option_to_variant_with_selector_field_field_class(
		struct bt_field_class *fc, const char *name,
		struct bt_field_class *option_fc,
		const struct bt_integer_range_set *range_set,
		enum bt_field_class_type expected_type,
		const char *api_func)
{
	int status;
	struct bt_field_class_variant_with_selector_field *var_fc = (void *) fc;
	struct bt_field_class_variant_with_selector_field_option *opt = NULL;
	bool has_overlap;

	BT_ASSERT(fc);

	/* Name is mandatory in MIP 0, optional later. */
	if (fc->mip_version == 0) {
		BT_ASSERT_PRE_NAME_NON_NULL_FROM_FUNC(api_func, name);
	}

	BT_ASSERT_PRE_NON_NULL_FROM_FUNC(api_func, "option-field-class",
		option_fc, "Option field class");
	BT_ASSERT_PRE_INT_RANGE_SET_NON_NULL_FROM_FUNC(api_func, range_set);
	BT_ASSERT_PRE_INT_RANGE_SET_NOT_EMPTY_FROM_FUNC(api_func, range_set);
	status = ranges_overlap(var_fc->common.common.named_fcs, range_set,
		expected_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		&has_overlap);
	if (status) {
		/* ranges_overlap() logs errors */
		goto end;
	}

	BT_ASSERT_PRE_FROM_FUNC(api_func, "ranges-do-not-overlap",
		!has_overlap,
		"Integer range set's ranges and existing ranges have an overlap: "
		"%!+R", range_set);
	opt = create_variant_with_selector_field_option(name, option_fc, range_set);
	if (!opt) {
		/* create_variant_with_selector_field_option() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	status = append_named_field_class_to_container_field_class((void *) fc,
		&opt->common, __func__, VAR_FC_OPT_NAME_IS_UNIQUE_ID);
	if (status == BT_FUNC_STATUS_OK) {
		/* Moved to the container */
		opt = NULL;
	}

end:
	if (opt) {
		destroy_variant_with_selector_field_option(opt);
	}

	return status;
}

BT_EXPORT
enum bt_field_class_variant_with_selector_field_integer_append_option_status
bt_field_class_variant_with_selector_field_integer_unsigned_append_option(
		struct bt_field_class *fc, const char *name,
		struct bt_field_class *option_fc,
		const struct bt_integer_range_set_unsigned *range_set)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-with-unsigned-integer-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		"Field class");
	return append_option_to_variant_with_selector_field_field_class(fc,
		name, option_fc, (const void *) range_set,
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		__func__);
}

BT_EXPORT
enum bt_field_class_variant_with_selector_field_integer_append_option_status
bt_field_class_variant_with_selector_field_integer_signed_append_option(
		struct bt_field_class *fc, const char *name,
		struct bt_field_class *option_fc,
		const struct bt_integer_range_set_signed *range_set)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-with-signed-integer-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		"Field class");
	return append_option_to_variant_with_selector_field_field_class(fc,
		name, option_fc, (const void *) range_set,
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		__func__);
}

BT_EXPORT
uint64_t bt_field_class_variant_get_option_count(const struct bt_field_class *fc)
{
	const struct bt_field_class_variant *var_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT("field-class", fc, "Field class");
	return (uint64_t) var_fc->common.named_fcs->len;
}

BT_EXPORT
const struct bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name_const(
		const struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT("field-class", fc, "Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_by_name(
			(void *) fc, name, __func__);
}

BT_EXPORT
const struct bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT("field-class", fc, "Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_at_index(
			(void *) fc, index, __func__);
}

BT_EXPORT
struct bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_name(
		struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT("field-class", fc, "Field class");
	return (void *)
		borrow_named_field_class_from_container_field_class_by_name(
			(void *) fc, name, __func__);
}

BT_EXPORT
struct bt_field_class_variant_option *
bt_field_class_variant_borrow_option_by_index(
		struct bt_field_class *fc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT("field-class", fc, "Field class");
	return (void *)
		borrow_named_field_class_from_container_field_class_at_index(
			(void *) fc, index, __func__);
}

BT_EXPORT
const struct bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const(
		const struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-with-unsigned-integer-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		"Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_by_name(
			(void *) fc, name, __func__);
}

BT_EXPORT
const struct bt_field_class_variant_with_selector_field_integer_unsigned_option *
bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-with-unsigned-integer-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD,
		"Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_at_index(
			(void *) fc, index, __func__);
}

BT_EXPORT
const struct bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const(
		const struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-with-signed-integer-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		"Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_by_name(
			(void *) fc, name, __func__);
}

BT_EXPORT
const struct bt_field_class_variant_with_selector_field_integer_signed_option *
bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
		const struct bt_field_class *fc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc,
		"variant-field-class-with-signed-integer-selector-field",
		BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD,
		"Field class");
	return (const void *)
		borrow_named_field_class_from_container_field_class_at_index(
			(void *) fc, index, __func__);
}

BT_EXPORT
const char *bt_field_class_variant_option_get_name(
		const struct bt_field_class_variant_option *option)
{
	const struct bt_named_field_class *named_fc = (const void *) option;

	BT_ASSERT_PRE_DEV_VAR_FC_OPT_NON_NULL(option);

	/* MIP > 0: a variant field class option may have no name */
	return named_fc->name ? named_fc->name->str : NULL;
}

BT_EXPORT
const struct bt_field_class *
bt_field_class_variant_option_borrow_field_class_const(
		const struct bt_field_class_variant_option *option)
{
	const struct bt_named_field_class *named_fc = (const void *) option;

	BT_ASSERT_PRE_DEV_VAR_FC_OPT_NON_NULL(option);
	return named_fc->fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_variant_option_borrow_field_class(
		struct bt_field_class_variant_option *option)
{
	struct bt_named_field_class *named_fc = (void *) option;

	BT_ASSERT_PRE_DEV_VAR_FC_OPT_NON_NULL(option);
	return named_fc->fc;
}

BT_EXPORT
const struct bt_integer_range_set_unsigned *
bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const(
		const struct bt_field_class_variant_with_selector_field_integer_unsigned_option *option)
{
	const struct bt_field_class_variant_with_selector_field_option *opt =
		(const void *) option;

	BT_ASSERT_PRE_DEV_VAR_FC_OPT_NON_NULL(option);
	return (const void *) opt->range_set;
}

BT_EXPORT
const struct bt_integer_range_set_signed *
bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const(
		const struct bt_field_class_variant_with_selector_field_integer_signed_option *option)
{
	const struct bt_field_class_variant_with_selector_field_option *opt =
		(const void *) option;

	BT_ASSERT_PRE_DEV_VAR_FC_OPT_NON_NULL(option);
	return (const void *) opt->range_set;
}

BT_EXPORT
const struct bt_field_path *
bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_variant_with_selector_field *var_fc =
		(const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT_WITH_SEL("field-class", fc,
		"Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_EQ(fc, 0);
	BT_ASSERT_DBG(var_fc->selector_field_xref_kind == FIELD_XREF_KIND_PATH);
	return var_fc->selector_field.path.path;
}

BT_EXPORT
const struct bt_field_location *
bt_field_class_variant_with_selector_field_borrow_selector_field_location_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_variant_with_selector_field *var_fc =
		(const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_VARIANT_WITH_SEL("field-class", fc,
		"Field class");
	BT_ASSERT_PRE_DEV_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_DBG(var_fc->selector_field_xref_kind == FIELD_XREF_KIND_LOCATION);
	return var_fc->selector_field.location;
}

static
int init_array_field_class(struct bt_field_class_array *fc,
		enum bt_field_class_type type, bt_object_release_func release_func,
		struct bt_field_class *element_fc,
		const struct bt_trace_class *trace_class,
		const char *api_func)
{
	int ret;

	BT_ASSERT_PRE_NON_NULL_FROM_FUNC(api_func, "element-field-class",
		element_fc, "Element field class");

	ret = init_field_class((void *) fc, type, release_func,
		trace_class);
	if (ret) {
		goto end;
	}

	fc->element_fc = element_fc;
	bt_object_get_ref_no_null_check(fc->element_fc);
	bt_field_class_freeze(element_fc);

end:
	return ret;
}

static
void finalize_array_field_class(struct bt_field_class_array *array_fc)
{
	BT_ASSERT(array_fc);
	BT_LOGD_STR("Putting element field class.");
	finalize_field_class((void *) array_fc);
	BT_OBJECT_PUT_REF_AND_RESET(array_fc->element_fc);
}

static
void destroy_static_array_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying static array field class object: %!+F", obj);
	finalize_array_field_class((void *) obj);
	g_free(obj);
}

BT_EXPORT
struct bt_field_class *
bt_field_class_array_static_create(bt_trace_class *trace_class,
		struct bt_field_class *element_fc, uint64_t length)
{
	struct bt_field_class_array_static *array_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD_STR("Creating default static array field class object.");
	array_fc = g_new0(struct bt_field_class_array_static, 1);
	if (!array_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one static array field class.");
		goto error;
	}

	if (init_array_field_class((void *) array_fc,
			BT_FIELD_CLASS_TYPE_STATIC_ARRAY,
			destroy_static_array_field_class, element_fc,
			trace_class, __func__)) {
		goto error;
	}

	array_fc->length = length;
	BT_LIB_LOGD("Created static array field class object: %!+F", array_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return (void *) array_fc;
}

BT_EXPORT
const struct bt_field_class *
bt_field_class_array_borrow_element_field_class_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_array *array_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_ARRAY("field-class", fc, "Field class");
	return array_fc->element_fc;
}

BT_EXPORT
struct bt_field_class *
bt_field_class_array_borrow_element_field_class(struct bt_field_class *fc)
{
	struct bt_field_class_array *array_fc = (void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_IS_ARRAY("field-class", fc, "Field class");
	return array_fc->element_fc;
}

BT_EXPORT
uint64_t bt_field_class_array_static_get_length(const struct bt_field_class *fc)
{
	const struct bt_field_class_array_static *array_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc,
		"static-array-field-class", BT_FIELD_CLASS_TYPE_STATIC_ARRAY,
		"Field class");
	return (uint64_t) array_fc->length;
}

static
void destroy_dynamic_array_field_class(struct bt_object *obj)
{
	struct bt_field_class_array_dynamic *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying dynamic array field class object: %!+F", fc);
	finalize_array_field_class((void *) fc);

	if (fc->common.common.type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD) {
		switch (fc->length_field.xref_kind) {
		case FIELD_XREF_KIND_PATH:
			BT_LOGD_STR("Putting length field class.");
			BT_OBJECT_PUT_REF_AND_RESET(fc->length_field.path.class);
			fc->length_field.path.class = NULL;

			BT_LOGD_STR("Putting length field path.");
			BT_OBJECT_PUT_REF_AND_RESET(fc->length_field.path.path);
			fc->length_field.path.path = NULL;
			break;
		case FIELD_XREF_KIND_LOCATION:
			BT_LOGD_STR("Putting length field location.");
			BT_OBJECT_PUT_REF_AND_RESET(fc->length_field.location);
			fc->length_field.location = NULL;
			break;
		};
	}

	g_free(fc);
}

static
struct bt_field_class_array_dynamic *create_dynamic_array_field_class(
		struct bt_trace_class *trace_class,
		struct bt_field_class *element_fc,
		enum bt_field_class_type fc_type,
		const char *api_func)
{
	struct bt_field_class_array_dynamic *array_fc = NULL;

	BT_LOGD_STR("Creating default dynamic array field class object.");
	array_fc = g_new0(struct bt_field_class_array_dynamic, 1);
	if (!array_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one dynamic array field class.");
		goto error;
	}

	if (init_array_field_class((void *) array_fc, fc_type,
			destroy_dynamic_array_field_class, element_fc,
			trace_class, api_func)) {
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return array_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_array_dynamic_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *element_fc,
		struct bt_field_class *length_fc)
{
	struct bt_field_class_array_dynamic *array_fc = NULL;


	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_EQ(trace_class, 0);

	array_fc = create_dynamic_array_field_class(trace_class, element_fc,
		length_fc ?
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD :
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD,
		 __func__);
	if (!array_fc) {
		goto error;
	}

	if (length_fc) {
		array_fc->length_field.xref_kind = FIELD_XREF_KIND_PATH;

		BT_ASSERT_PRE_FC_IS_UNSIGNED_INT("length-field-class",
			length_fc, "Length field class");
		array_fc->length_field.path.class = length_fc;
		bt_object_get_ref_no_null_check(length_fc);
		bt_field_class_freeze(length_fc);
	}

	BT_LIB_LOGD("Created dynamic array field class object: %!+F", array_fc);

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return (void *) array_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_array_dynamic_without_length_field_location_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *element_field_class)
{
	struct bt_field_class_array_dynamic *array_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	array_fc = create_dynamic_array_field_class(trace_class,
		element_field_class, BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD,
		__func__);
	if (!array_fc) {
		goto error;
	}

	BT_LIB_LOGD("Created dynamic array field class without field location object: %!+F",
		array_fc);

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return (void *) array_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_array_dynamic_with_length_field_location_create(
		struct bt_trace_class *trace_class,
		struct bt_field_class *element_field_class,
		const struct bt_field_location *length_field_location)
{
	struct bt_field_class_array_dynamic *array_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_FL_NON_NULL(length_field_location);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	array_fc = create_dynamic_array_field_class(trace_class,
		element_field_class, BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD,
		__func__);
	if (!array_fc) {
		goto error;
	}

	array_fc->length_field.xref_kind = FIELD_XREF_KIND_LOCATION;
	array_fc->length_field.location = length_field_location;
	bt_object_get_ref_no_null_check(length_field_location);

	BT_LIB_LOGD("Created dynamic array field class with field location object: %!+F",
		array_fc);

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return (void *) array_fc;
}

BT_EXPORT
const struct bt_field_path *
bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_array_dynamic *seq_fc = (const void *) fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"dynamic-array-field-class-with-length-field",
		BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD,
		"Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_EQ(fc, 0);
	BT_ASSERT_DBG(seq_fc->length_field.xref_kind == FIELD_XREF_KIND_PATH);
	return seq_fc->length_field.path.path;
}

BT_EXPORT
const struct bt_field_location *
bt_field_class_array_dynamic_with_length_field_borrow_length_field_location_const(
		const struct bt_field_class *fc)
{
	const struct bt_field_class_array_dynamic *seq_fc = (const void *) fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"dynamic-array-field-class-with-length-field",
		BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD,
		"Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT_DBG(seq_fc->length_field.xref_kind == FIELD_XREF_KIND_LOCATION);
	return seq_fc->length_field.location;
}

static
void destroy_string_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying string field class object: %!+F", obj);
	finalize_field_class((void *) obj);
	g_free(obj);
}

BT_EXPORT
struct bt_field_class *bt_field_class_string_create(bt_trace_class *trace_class)
{
	struct bt_field_class_string *string_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_LOGD_STR("Creating default string field class object.");
	string_fc = g_new0(struct bt_field_class_string, 1);
	if (!string_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one string field class.");
		goto error;
	}

	if (init_field_class((void *) string_fc, BT_FIELD_CLASS_TYPE_STRING,
			destroy_string_field_class, trace_class)) {
		goto error;
	}

	BT_LIB_LOGD("Created string field class object: %!+F", string_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(string_fc);

end:
	return (void *) string_fc;
}

void _bt_field_class_freeze(const struct bt_field_class *c_fc)
{
	struct bt_field_class *fc = (void *) c_fc;

	/*
	 * Element/member/option field classes are frozen when added to
	 * their owner.
	 */
	BT_ASSERT(fc);
	bt_value_freeze(fc->user_attributes);
	fc->frozen = true;

	if (fc->type == BT_FIELD_CLASS_TYPE_STRUCTURE ||
			bt_field_class_type_is(fc->type,
				BT_FIELD_CLASS_TYPE_VARIANT)) {
		struct bt_field_class_named_field_class_container *container_fc =
			(void *) fc;
		uint64_t i;

		for (i = 0; i < container_fc->named_fcs->len; i++) {
			bt_named_field_class_freeze(
				container_fc->named_fcs->pdata[i]);
		}
	}
}

void _bt_named_field_class_freeze(const struct bt_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);
	BT_ASSERT(named_fc->fc->frozen);
	BT_LIB_LOGD("Freezing named field class's user attributes: %!+v",
		named_fc->user_attributes);
	bt_value_freeze(named_fc->user_attributes);
	((struct bt_named_field_class *) named_fc)->frozen = true;
}

void bt_field_class_make_part_of_trace_class(const struct bt_field_class *c_fc)
{
	struct bt_field_class *fc = (void *) c_fc;

	BT_ASSERT(fc);
	BT_ASSERT_PRE("field-class-is-not-part-of-trace-class",
		!fc->part_of_trace_class,
		"Field class is already part of a trace class: %!+F", fc);
	fc->part_of_trace_class = true;

	if (fc->type == BT_FIELD_CLASS_TYPE_STRUCTURE ||
			bt_field_class_type_is(fc->type,
				BT_FIELD_CLASS_TYPE_VARIANT)) {
		struct bt_field_class_named_field_class_container *container_fc =
			(void *) fc;
		uint64_t i;

		for (i = 0; i < container_fc->named_fcs->len; i++) {
			struct bt_named_field_class *named_fc =
				container_fc->named_fcs->pdata[i];

			bt_field_class_make_part_of_trace_class(named_fc->fc);
		}
	} else if (bt_field_class_type_is(fc->type,
			BT_FIELD_CLASS_TYPE_ARRAY)) {
		struct bt_field_class_array *array_fc = (void *) fc;

		bt_field_class_make_part_of_trace_class(array_fc->element_fc);
	}
}

BT_EXPORT
const struct bt_value *bt_field_class_borrow_user_attributes_const(
		const struct bt_field_class *fc)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	return fc->user_attributes;
}

BT_EXPORT
struct bt_value *bt_field_class_borrow_user_attributes(
		struct bt_field_class *field_class)
{
	return (void *) bt_field_class_borrow_user_attributes_const(
		(void *) field_class);
}


BT_EXPORT
void bt_field_class_set_user_attributes(
		struct bt_field_class *fc,
		const struct bt_value *user_attributes)
{
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_USER_ATTRS_NON_NULL(user_attributes);
	BT_ASSERT_PRE_USER_ATTRS_IS_MAP(user_attributes);
	BT_ASSERT_PRE_DEV_FC_HOT(fc);
	bt_object_put_ref_no_null_check(fc->user_attributes);
	fc->user_attributes = (void *) user_attributes;
	bt_object_get_ref_no_null_check(fc->user_attributes);
}

static
const struct bt_value *bt_named_field_class_borrow_user_attributes_const(
		const struct bt_named_field_class *named_fc)
{
	return named_fc->user_attributes;
}

static
void set_named_field_class_user_attributes(
		struct bt_named_field_class *named_fc,
		const struct bt_value *user_attributes, const char *api_func)
{
	BT_ASSERT_PRE_USER_ATTRS_NON_NULL_FROM_FUNC(api_func, user_attributes);
	BT_ASSERT_PRE_USER_ATTRS_NON_NULL_FROM_FUNC(api_func, user_attributes);
	bt_object_put_ref_no_null_check(named_fc->user_attributes);
	named_fc->user_attributes = (void *) user_attributes;
	bt_object_get_ref_no_null_check(named_fc->user_attributes);
}

BT_EXPORT
const struct bt_value *
bt_field_class_structure_member_borrow_user_attributes_const(
		const struct bt_field_class_structure_member *member)
{
	BT_ASSERT_PRE_STRUCT_FC_MEMBER_NON_NULL(member);
	return bt_named_field_class_borrow_user_attributes_const(
		(const void *) member);
}

BT_EXPORT
struct bt_value *
bt_field_class_structure_member_borrow_user_attributes(
		struct bt_field_class_structure_member *member)
{
	BT_ASSERT_PRE_STRUCT_FC_MEMBER_NON_NULL(member);
	return (void *) bt_named_field_class_borrow_user_attributes_const(
		(void *) member);
}

BT_EXPORT
void bt_field_class_structure_member_set_user_attributes(
		struct bt_field_class_structure_member *member,
		const struct bt_value *user_attributes)
{
	BT_ASSERT_PRE_STRUCT_FC_MEMBER_NON_NULL(member);
	BT_ASSERT_PRE_DEV_HOT("structure-field-class-member",
		(struct bt_named_field_class *) member,
		"Structure field class member", ".");
	set_named_field_class_user_attributes((void *) member,
		user_attributes, __func__);
}

BT_EXPORT
const struct bt_value *bt_field_class_variant_option_borrow_user_attributes_const(
		const struct bt_field_class_variant_option *option)
{
	BT_ASSERT_PRE_VAR_FC_OPT_NON_NULL(option);
	return bt_named_field_class_borrow_user_attributes_const(
		(const void *) option);
}

BT_EXPORT
struct bt_value *bt_field_class_variant_option_borrow_user_attributes(
		struct bt_field_class_variant_option *option)
{
	BT_ASSERT_PRE_VAR_FC_OPT_NON_NULL(option);
	return (void *) bt_named_field_class_borrow_user_attributes_const(
		(void *) option);
}

BT_EXPORT
void bt_field_class_variant_option_set_user_attributes(
		struct bt_field_class_variant_option *option,
		const struct bt_value *user_attributes)
{
	BT_ASSERT_PRE_VAR_FC_OPT_NON_NULL(option);
	BT_ASSERT_PRE_DEV_HOT("variant-field-class-option",
		(struct bt_named_field_class *) option,
		"Variant field class option", ".");
	set_named_field_class_user_attributes((void *) option,
		user_attributes, __func__);
}

BT_EXPORT
bt_field_class_blob_set_media_type_status bt_field_class_blob_set_media_type(
		struct bt_field_class *fc, const char *media_type)
{
	struct bt_field_class_blob *fc_blob = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_BLOB("field-class", fc, "Field class");
	BT_ASSERT_PRE_DEV_FC_HOT(fc);
	BT_ASSERT_PRE_NON_NULL("media-type", media_type, "Media type");

	g_string_assign(fc_blob->media_type, media_type);

	return BT_FIELD_CLASS_BLOB_SET_MEDIA_TYPE_STATUS_OK;
}

BT_EXPORT
const char *bt_field_class_blob_get_media_type(
		const bt_field_class *fc)
{
	struct bt_field_class_blob *fc_blob = (void *) fc;

	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_IS_BLOB("field-class", fc, "Field class");

	return fc_blob->media_type->str;
}

static
int init_blob_field_class(struct bt_field_class_blob *fc,
		enum bt_field_class_type type,
		bt_object_release_func release_func,
		bt_trace_class *trace_class)
{
	int ret;

	ret = init_field_class((void *) fc, type, release_func,
		trace_class);
	if (ret) {
		goto end;
	}

	fc->media_type = g_string_new("application/octet-stream");

end:
	return ret;
}

static
void destroy_blob_field_class(struct bt_object *obj)
{
	struct bt_field_class_blob *blob = (void *) obj;

	BT_ASSERT(obj);
	g_string_free(blob->media_type, TRUE);
	finalize_field_class((void *) obj);
	g_free(obj);
}

static
void destroy_static_blob_field_class(struct bt_object *obj)
{
	struct bt_field_class_blob_static *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying static BLOB field class object: %!+F", fc);

	destroy_blob_field_class(obj);
}

BT_EXPORT
struct bt_field_class *bt_field_class_blob_static_create(
		struct bt_trace_class *trace_class, uint64_t length)
{
	struct bt_field_class_blob_static *blob_fc = NULL;
	int ret;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	BT_LOGD_STR("Creating default static BLOB field class object.");
	blob_fc = g_new0(struct bt_field_class_blob_static, 1);
	if (!blob_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one static BLOB field class.");
		goto error;
	}

	ret = init_blob_field_class((void *) blob_fc,
		BT_FIELD_CLASS_TYPE_STATIC_BLOB,
		destroy_static_blob_field_class,
		trace_class);
	if (ret) {
		goto end;
	}

	blob_fc->length = length;
	BT_LIB_LOGD("Created static BLOB field class object: %!+F", blob_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(blob_fc);

end:
	return (void *) blob_fc;
}

BT_EXPORT
uint64_t bt_field_class_blob_static_get_length(
		const bt_field_class *fc)
{
	const struct bt_field_class_blob_static *blob_fc = (const void *) fc;

	BT_ASSERT_PRE_DEV_FC_NON_NULL(fc);
	BT_ASSERT_PRE_DEV_FC_HAS_TYPE("field-class", fc,
		"static-blob-field-class", BT_FIELD_CLASS_TYPE_STATIC_BLOB,
		"Field class");

	return blob_fc->length;
}

static
void destroy_dynamic_blob_field_class(struct bt_object *obj)
{
	struct bt_field_class_blob_dynamic *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying dynamic BLOB field class object: %!+F", fc);

	bt_object_put_ref(fc->length_fl);
	fc->length_fl = NULL;

	destroy_blob_field_class(obj);
}

static
struct bt_field_class_blob_dynamic *create_dynamic_blob_field_class(
		struct bt_trace_class *trace_class, enum bt_field_class_type type)
{
	struct bt_field_class_blob_dynamic *blob_fc = NULL;
	int ret;

	BT_LOGD_STR("Creating default dynamic BLOB field class object.");
	blob_fc = g_new0(struct bt_field_class_blob_dynamic, 1);
	if (!blob_fc) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one dynamic BLOB field class.");
		goto error;
	}

	ret = init_blob_field_class((void *) blob_fc, type,
		destroy_dynamic_blob_field_class, trace_class);
	if (ret) {
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(blob_fc);

end:
	return blob_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_blob_dynamic_without_length_field_location_create(
		struct bt_trace_class *trace_class)
{
	struct bt_field_class_blob_dynamic *blob_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	blob_fc = create_dynamic_blob_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD);
	if (!blob_fc) {
		goto error;
	}

	BT_LIB_LOGD("Created dynamic BLOB field class without field location object: %!+F",
		blob_fc);

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(blob_fc);

end:
	return (void *) blob_fc;
}

BT_EXPORT
struct bt_field_class *bt_field_class_blob_dynamic_with_length_field_location_create(
		bt_trace_class *trace_class,
		const bt_field_location *length_field_location)
{
	struct bt_field_class_blob_dynamic *blob_fc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_TC_NON_NULL(trace_class);
	BT_ASSERT_PRE_FL_NON_NULL(length_field_location);
	BT_ASSERT_PRE_TC_MIP_VERSION_GE(trace_class, 1);

	blob_fc = create_dynamic_blob_field_class(trace_class,
		BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD);
	if (!blob_fc) {
		goto error;
	}

	blob_fc->length_fl = length_field_location;
	bt_object_get_ref_no_null_check(length_field_location);

	BT_LIB_LOGD("Created dynamic BLOB field class with field location object: %!+F",
		blob_fc);

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(blob_fc);

end:
	return (void *) blob_fc;
}

BT_EXPORT
const bt_field_location *
bt_field_class_blob_dynamic_with_length_field_borrow_length_field_location_const(
		const bt_field_class *fc)
{
	const struct bt_field_class_blob_dynamic *blob_fc = (const void *) fc;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_FC_NON_NULL(fc);
	BT_ASSERT_PRE_FC_HAS_TYPE("field-class", fc,
		"dynamic-blob-field-class-with-length-field",
		BT_FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD,
		"Field class");
	BT_ASSERT_PRE_FC_MIP_VERSION_GE(fc, 1);
	BT_ASSERT(blob_fc->length_fl);

	return blob_fc->length_fl;
}

BT_EXPORT
uint64_t bt_field_class_get_graph_mip_version(
		const bt_field_class *field_class)
{
	BT_ASSERT_PRE_DEV_FC_NON_NULL(field_class);
	return field_class->mip_version;
}

BT_EXPORT
void bt_field_class_get_ref(const struct bt_field_class *field_class)
{
	bt_object_get_ref(field_class);
}

BT_EXPORT
void bt_field_class_put_ref(const struct bt_field_class *field_class)
{
	bt_object_put_ref(field_class);
}
