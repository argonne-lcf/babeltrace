/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/CLOCK-CLASS"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include "common/uuid.h"
#include <babeltrace2/trace-ir/clock-class.h>
#include "clock-class.h"
#include "clock-snapshot.h"
#include "utils.h"
#include "compat/compiler.h"
#include <babeltrace2/types.h>
#include <inttypes.h>
#include <stdbool.h>
#include "lib/object.h"
#include "common/assert.h"
#include "lib/func-status.h"
#include "lib/value.h"
#include "lib/graph/component.h"
#include "lib/graph/graph.h"

#define BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(_cc)				\
	BT_ASSERT_PRE_DEV_HOT("clock-class", (_cc), "Clock class",	\
		": %!+K", (_cc))

static
void free_clock_class_custom_origin(struct bt_clock_class *clock_class)
{
	g_free(clock_class->custom_origin.ns);
	clock_class->custom_origin.ns = NULL;
	g_free(clock_class->custom_origin.name);
	clock_class->custom_origin.name = NULL;
	g_free(clock_class->custom_origin.uid);
	clock_class->custom_origin.uid = NULL;
}

static
void destroy_clock_class(struct bt_object *obj)
{
	struct bt_clock_class *clock_class = (void *) obj;

	BT_LIB_LOGD("Destroying clock class: %!+K", clock_class);
	BT_OBJECT_PUT_REF_AND_RESET(clock_class->user_attributes);

	g_free(clock_class->ns);
	g_free(clock_class->name);
	g_free(clock_class->uid);
	g_free(clock_class->description);
	free_clock_class_custom_origin(clock_class);
	bt_object_pool_finalize(&clock_class->cs_pool);
	g_free(clock_class);
}

static
void free_clock_snapshot(struct bt_clock_snapshot *clock_snapshot,
		struct bt_clock_class *clock_class __attribute__((unused)))
{
	bt_clock_snapshot_destroy(clock_snapshot);
}

static inline
void set_base_offset(struct bt_clock_class *clock_class)
{
	clock_class->base_offset.overflows = bt_util_get_base_offset_ns(
		clock_class->offset_seconds, clock_class->offset_cycles,
		clock_class->frequency, &clock_class->base_offset.value_ns);
}

static void
set_origin_unix_epoch(struct bt_clock_class *clock_class)
{
	free_clock_class_custom_origin(clock_class);
	clock_class->origin_kind = CLOCK_ORIGIN_KIND_UNIX_EPOCH;
	BT_LIB_LOGD("Set clock class's origin to Unix epoch: %!+K",
		clock_class);
}

static void
set_origin_unknown(struct bt_clock_class *clock_class)
{
	free_clock_class_custom_origin(clock_class);
	clock_class->origin_kind = CLOCK_ORIGIN_KIND_UNKNOWN;
	BT_LIB_LOGD("Set clock class's origin to unknown: %!+K",
		clock_class);
}

BT_EXPORT
struct bt_clock_class *bt_clock_class_create(bt_self_component *self_comp)
{
	int ret;
	struct bt_clock_class *clock_class = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_COMP_NON_NULL(self_comp);
	BT_LOGD_STR("Creating default clock class object");

	clock_class = g_new0(struct bt_clock_class, 1);
	if (!clock_class) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one clock class.");
		goto error;
	}

	bt_object_init_shared(&clock_class->base, destroy_clock_class);

	clock_class->mip_version =
		bt_component_borrow_graph((void *) self_comp)->mip_version;

	clock_class->user_attributes = bt_value_map_create();
	if (!clock_class->user_attributes) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create a map value object.");
		goto error;
	}

	clock_class->frequency = UINT64_C(1000000000);

	if (clock_class->mip_version == 0) {
		clock_class->precision.base.avail =
			BT_PROPERTY_AVAILABILITY_AVAILABLE;
	}

	set_origin_unix_epoch(clock_class);
	set_base_offset(clock_class);
	ret = bt_object_pool_initialize(&clock_class->cs_pool,
		(bt_object_pool_new_object_func) bt_clock_snapshot_new,
		(bt_object_pool_destroy_object_func)
			free_clock_snapshot,
		clock_class);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to initialize clock snapshot pool: ret=%d",
			ret);
		goto error;
	}

	BT_LIB_LOGD("Created clock class object: %!+K", clock_class);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(clock_class);

end:
	return clock_class;
}

BT_EXPORT
const char *bt_clock_class_get_namespace(
		const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	return clock_class->ns;
}

BT_EXPORT
enum bt_clock_class_set_namespace_status bt_clock_class_set_namespace(
		struct bt_clock_class *clock_class, const char *ns)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_NAME_NON_NULL(ns);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	g_free(clock_class->ns);
	clock_class->ns = g_strdup(ns);
	BT_LIB_LOGD("Set clock class's namespace: %!+K", clock_class);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
const char *bt_clock_class_get_name(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->name;
}

BT_EXPORT
enum bt_clock_class_set_name_status bt_clock_class_set_name(
		struct bt_clock_class *clock_class, const char *name)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_NAME_NON_NULL(name);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	g_free(clock_class->name);
	clock_class->name = g_strdup(name);
	BT_LIB_LOGD("Set clock class's name: %!+K", clock_class);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
const char *bt_clock_class_get_uid(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	return clock_class->uid;
}

BT_EXPORT
enum bt_clock_class_set_uid_status bt_clock_class_set_uid(
		struct bt_clock_class *clock_class, const char *uid)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_NAME_NON_NULL(uid);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	g_free(clock_class->uid);
	clock_class->uid = g_strdup(uid);
	BT_LIB_LOGD("Set clock class's UID: %!+K", clock_class);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
const char *bt_clock_class_get_description(
		const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->description;
}

BT_EXPORT
enum bt_clock_class_set_description_status bt_clock_class_set_description(
		struct bt_clock_class *clock_class, const char *descr)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DESCR_NON_NULL(descr);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	g_free(clock_class->description);
	clock_class->description = g_strdup(descr);
	BT_LIB_LOGD("Set clock class's description: %!+K",
		clock_class);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
uint64_t bt_clock_class_get_frequency(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->frequency;
}

BT_EXPORT
void bt_clock_class_set_frequency(struct bt_clock_class *clock_class,
		uint64_t frequency)
{
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE("valid-frequency",
		frequency != UINT64_C(-1) && frequency != 0,
		"Invalid frequency: %![cc-]+K, new-freq=%" PRIu64,
		clock_class, frequency);
	BT_ASSERT_PRE("offset-cycles-lt-frequency",
		clock_class->offset_cycles < frequency,
		"Offset (cycles) is greater than clock class's frequency: "
		"%![cc-]+K, new-freq=%" PRIu64, clock_class, frequency);
	clock_class->frequency = frequency;
	set_base_offset(clock_class);
	BT_LIB_LOGD("Set clock class's frequency: %!+K", clock_class);
}

BT_EXPORT
uint64_t bt_clock_class_get_precision(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_EQ(clock_class, 0);
	BT_ASSERT_DBG(clock_class->precision.base.avail ==
		BT_PROPERTY_AVAILABILITY_AVAILABLE);
	return clock_class->precision.value;
}

BT_EXPORT
bt_property_availability bt_clock_class_get_opt_precision(
		const struct bt_clock_class *clock_class, uint64_t *precision)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_NON_NULL("precision-output", precision,
		"Precision (output)");

	*precision = clock_class->precision.value;
	return clock_class->precision.base.avail;
}

BT_EXPORT
void bt_clock_class_set_precision(struct bt_clock_class *clock_class,
		uint64_t precision)
{
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE("valid-precision", precision != UINT64_C(-1),
		"Invalid precision: %![cc-]+K, new-precision=%" PRIu64,
		clock_class, precision);
	clock_class->precision.value = precision;
	clock_class->precision.base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	BT_LIB_LOGD("Set clock class's precision: %!+K", clock_class);
}

BT_EXPORT
void bt_clock_class_set_accuracy(bt_clock_class *clock_class,
		uint64_t accuracy)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	BT_ASSERT_PRE("valid-accuracy", accuracy != UINT64_C(-1),
		"Invalid accuracy: %![cc-]+K, new-accuracy=%" PRIu64,
		clock_class, accuracy);
	clock_class->accuracy.value = accuracy;
	clock_class->accuracy.base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	BT_LIB_LOGD("Set clock class's accuracy: %!+K", clock_class);
}

BT_EXPORT
bt_property_availability bt_clock_class_get_accuracy(
		const struct bt_clock_class *clock_class, uint64_t *accuracy)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	BT_ASSERT_PRE_DEV_NON_NULL("accuracy-output", accuracy,
		"Accuracy (output)");

	*accuracy = clock_class->accuracy.value;
	return clock_class->accuracy.base.avail;
}

BT_EXPORT
void bt_clock_class_get_offset(const struct bt_clock_class *clock_class,
		int64_t *seconds, uint64_t *cycles)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_NON_NULL("seconds-output", seconds,
		"Seconds (output)");
	BT_ASSERT_PRE_DEV_NON_NULL("cycles-output", cycles, "Cycles (output)");
	*seconds = clock_class->offset_seconds;
	*cycles = clock_class->offset_cycles;
}

BT_EXPORT
void bt_clock_class_set_offset(struct bt_clock_class *clock_class,
		int64_t seconds, uint64_t cycles)
{
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE("offset-cycles-lt-frequency",
		cycles < clock_class->frequency,
		"Offset (cycles) is greater than clock class's frequency: "
		"%![cc-]+K, new-offset-cycles=%" PRIu64, clock_class, cycles);
	clock_class->offset_seconds = seconds;
	clock_class->offset_cycles = cycles;
	set_base_offset(clock_class);
	BT_LIB_LOGD("Set clock class's offset: %!+K", clock_class);
}

#define BT_ASSERT_PRE_DEV_CC_ORIGIN_IS_CUSTOM(_cc)				\
	BT_ASSERT_PRE_DEV("clock-class-origin-is-custom",			\
		(_cc)->origin_kind == CLOCK_ORIGIN_KIND_CUSTOM,	\
		"Clock class's origin is not custom: %!+K", (_cc))

BT_EXPORT
const char *bt_clock_class_get_origin_namespace(
		const bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	BT_ASSERT_PRE_DEV_CC_ORIGIN_IS_CUSTOM(clock_class);
	return clock_class->custom_origin.ns;
}

BT_EXPORT
const char *bt_clock_class_get_origin_name(
		const bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	BT_ASSERT_PRE_DEV_CC_ORIGIN_IS_CUSTOM(clock_class);
	return clock_class->custom_origin.name;
}

BT_EXPORT
const char *bt_clock_class_get_origin_uid(
		const bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	BT_ASSERT_PRE_DEV_CC_ORIGIN_IS_CUSTOM(clock_class);
	return clock_class->custom_origin.uid;
}

BT_EXPORT
bt_bool bt_clock_class_origin_is_unix_epoch(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->origin_kind == CLOCK_ORIGIN_KIND_UNIX_EPOCH;
}

BT_EXPORT
bt_bool bt_clock_class_origin_is_known(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->origin_kind != CLOCK_ORIGIN_KIND_UNKNOWN;
}

BT_EXPORT
void bt_clock_class_set_origin_is_unix_epoch(struct bt_clock_class *clock_class,
		bt_bool origin_is_unix_epoch)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);

	if (origin_is_unix_epoch) {
		set_origin_unix_epoch(clock_class);
	} else {
		set_origin_unknown(clock_class);
	}
}

BT_EXPORT
void bt_clock_class_set_origin_unknown(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	set_origin_unknown(clock_class);
}

BT_EXPORT
enum bt_clock_class_set_origin_status bt_clock_class_set_origin(
		struct bt_clock_class *clock_class, const char *ns,
		const char *name, const char *uid)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(clock_class, 1);
	BT_ASSERT_PRE_NAME_NON_NULL(name);
	BT_ASSERT_PRE_UID_NON_NULL(uid);
	free_clock_class_custom_origin(clock_class);
	clock_class->origin_kind = CLOCK_ORIGIN_KIND_CUSTOM;
	clock_class->custom_origin.ns = g_strdup(ns);
	clock_class->custom_origin.name = g_strdup(name);
	clock_class->custom_origin.uid = g_strdup(uid);
	BT_LIB_LOGD("Set clock class's origin to custom: %!+K", clock_class);
	return BT_FUNC_STATUS_OK;
}

BT_EXPORT
void bt_clock_class_set_origin_unix_epoch(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	set_origin_unix_epoch(clock_class);
}

BT_EXPORT
bt_uuid bt_clock_class_get_uuid(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_EQ(clock_class, 0);
	return clock_class->uuid.value;
}

BT_EXPORT
void bt_clock_class_set_uuid(struct bt_clock_class *clock_class,
		bt_uuid uuid)
{
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_UUID_NON_NULL(uuid);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE_CC_MIP_VERSION_EQ(clock_class, 0);
	bt_uuid_copy(clock_class->uuid.uuid, uuid);
	clock_class->uuid.value = clock_class->uuid.uuid;
	BT_LIB_LOGD("Set clock class's UUID: %!+K", clock_class);
}

void _bt_clock_class_freeze(const struct bt_clock_class *clock_class)
{
	BT_ASSERT(clock_class);

	if (clock_class->frozen) {
		return;
	}

	BT_LIB_LOGD("Freezing clock class's user attributes: %!+v",
		clock_class->user_attributes);
	bt_value_freeze(clock_class->user_attributes);
	BT_LIB_LOGD("Freezing clock class: %!+K", clock_class);
	((struct bt_clock_class *) clock_class)->frozen = 1;
}

BT_EXPORT
enum bt_clock_class_cycles_to_ns_from_origin_status
bt_clock_class_cycles_to_ns_from_origin(
		const struct bt_clock_class *clock_class,
		uint64_t cycles, int64_t *ns)
{
	int ret;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_DEV_NON_NULL("nanoseconds-output", ns,
		"Nanoseconds (output)");
	ret = bt_util_ns_from_origin_clock_class(clock_class, cycles, ns);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot convert cycles to nanoseconds "
			"from origin for given clock class: "
			"value overflows the signed 64-bit integer range: "
			"%![cc-]+K, cycles=%" PRIu64,
			clock_class, cycles);
		ret = BT_FUNC_STATUS_OVERFLOW_ERROR;
	}

	return ret;
}

BT_EXPORT
bt_bool bt_clock_class_has_same_identity(
		const bt_clock_class *cc_a,
		const bt_clock_class *cc_b)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(cc_a);
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(cc_b);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(cc_a, 1);
	BT_ASSERT_PRE_CC_MIP_VERSION_GE(cc_b, 1);

	bt_bool is_same = BT_FALSE;

	if (g_strcmp0(cc_a->ns, cc_b->ns) != 0) {
		goto end;
	}

	if (!cc_a->name || !cc_b->name || strcmp(cc_a->name, cc_b->name) != 0) {
		goto end;
	}

	if (!cc_a->uid || !cc_b->uid || strcmp(cc_a->uid, cc_b->uid) != 0) {
		goto end;
	}

	is_same = BT_TRUE;

end:
	return is_same;
}

BT_EXPORT
const struct bt_value *bt_clock_class_borrow_user_attributes_const(
		const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->user_attributes;
}

BT_EXPORT
struct bt_value *bt_clock_class_borrow_user_attributes(
		struct bt_clock_class *clock_class)
{
	return (void *) bt_clock_class_borrow_user_attributes_const(
		(void *) clock_class);
}

BT_EXPORT
void bt_clock_class_set_user_attributes(
		struct bt_clock_class *clock_class,
		const struct bt_value *user_attributes)
{
	BT_ASSERT_PRE_CLK_CLS_NON_NULL(clock_class);
	BT_ASSERT_PRE_USER_ATTRS_NON_NULL(user_attributes);
	BT_ASSERT_PRE_USER_ATTRS_IS_MAP(user_attributes);
	BT_ASSERT_PRE_DEV_CLOCK_CLASS_HOT(clock_class);
	bt_object_put_ref_no_null_check(clock_class->user_attributes);
	clock_class->user_attributes = (void *) user_attributes;
	bt_object_get_ref_no_null_check(clock_class->user_attributes);
}

BT_EXPORT
uint64_t bt_clock_class_get_graph_mip_version(
		const bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_DEV_CLK_CLS_NON_NULL(clock_class);
	return clock_class->mip_version;
}

BT_EXPORT
void bt_clock_class_get_ref(const struct bt_clock_class *clock_class)
{
	bt_object_get_ref(clock_class);
}

BT_EXPORT
void bt_clock_class_put_ref(const struct bt_clock_class *clock_class)
{
	bt_object_put_ref(clock_class);
}
