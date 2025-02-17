/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_LIB_TRACE_IR_TRACE_CLASS_H
#define BABELTRACE_LIB_TRACE_IR_TRACE_CLASS_H

#include <babeltrace2/trace-ir/trace-class.h>
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field.h>
#include "lib/object.h"
#include <babeltrace2/value.h>
#include <babeltrace2/types.h>
#include <glib.h>
#include <sys/types.h>
#include <stdbool.h>

struct bt_trace_class {
	struct bt_object base;

	/* Effective MIP version for this trace class */
	uint64_t mip_version;

	/* Owned by this */
	struct bt_value *user_attributes;

	/* Array of `struct bt_stream_class *` */
	GPtrArray *stream_classes;

	bool assigns_automatic_stream_class_id;
	GArray *destruction_listeners;
	bool frozen;
};

void _bt_trace_class_freeze(const struct bt_trace_class *trace_class);

#ifdef BT_DEV_MODE
# define bt_trace_class_freeze		_bt_trace_class_freeze
#else
# define bt_trace_class_freeze(_tc)
#endif

#endif /* BABELTRACE_LIB_TRACE_IR_TRACE_CLASS_H */
