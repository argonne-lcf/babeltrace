/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_LIB_TRACE_IR_FIELD_CLASS_H
#define BABELTRACE_LIB_TRACE_IR_FIELD_CLASS_H

#include <babeltrace2/trace-ir/clock-class.h>
#include <babeltrace2/trace-ir/field-class.h>
#include "common/macros.h"
#include "lib/object.h"
#include <babeltrace2/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

#define BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(_fc, _index)		\
	(&bt_g_array_index(((struct bt_field_class_enumeration *) (_fc))->mappings, \
		struct bt_field_class_enumeration_mapping, (_index)))

#define BT_FIELD_CLASS_ENUM_MAPPING_RANGE_AT_INDEX(_mapping, _index)	\
	(&bt_g_array_index((_mapping)->ranges,				\
		struct bt_field_class_enumeration_mapping_range, (_index)))

enum field_xref_kind {
	/*
	 * Start at 1 to be able to differentiate a zero-initialized field from
	 * an initialized field.
	 */
	FIELD_XREF_KIND_PATH = 1,
	FIELD_XREF_KIND_LOCATION,
};

struct bt_field_class {
	struct bt_object base;
	enum bt_field_class_type type;
	bool frozen;

	/* Owned by this */
	struct bt_value *user_attributes;

	/*
	 * This flag indicates whether or not this field class is part
	 * of a trace class.
	 */
	bool part_of_trace_class;

	/* Effective MIP version for this field class */
	uint64_t mip_version;
};

struct bt_field_class_bool {
	struct bt_field_class common;
};

struct bt_field_class_bit_array_flag {
	/* Owned by this */
	gchar *label;

	/* Strong reference */
	const bt_integer_range_set_unsigned *range_set;

	/*
	 * Mask to apply to the field's value to determine if this flag is
	 * active.
	 */
	uint64_t mask;
};

struct bt_field_class_bit_array {
	struct bt_field_class common;
	uint64_t length;

	/* Array of `bt_field_class_bit_array_flag`, owned by this */
	GPtrArray *flags;

	/*
	 * This is an array of `const char *` which acts as a temporary
	 * (potentially growing) buffer for
	 * bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer().
	 *
	 * The actual strings are owned by the flags above.
	 */
	GPtrArray *label_buf;
};

struct bt_field_class_integer {
	struct bt_field_class common;

	/*
	 * Value range of fields built from this integer field class:
	 * this is an equivalent integer size in bits. More formally,
	 * `range` is `n` in:
	 *
	 * Unsigned range: [0, 2^n - 1]
	 * Signed range: [-2^(n - 1), 2^(n - 1) - 1]
	 */
	uint64_t range;

	/* Hints (bitwise OR) */
	uint64_t hints;

	enum bt_field_class_integer_preferred_display_base base;
};

struct bt_field_class_enumeration_mapping {
	GString *label;

	/* Owner by this */
	const struct bt_integer_range_set *range_set;
};

struct bt_field_class_enumeration_unsigned_mapping;
struct bt_field_class_enumeration_signed_mapping;

struct bt_field_class_enumeration {
	struct bt_field_class_integer common;

	/* Array of `struct bt_field_class_enumeration_mapping *` */
	GArray *mappings;

	/*
	 * This is an array of `const char *` which acts as a temporary
	 * (potentially growing) buffer for
	 * bt_field_class_enumeration_unsigned_get_mapping_labels_for_value()
	 * and
	 * bt_field_class_enumeration_signed_get_mapping_labels_for_value().
	 *
	 * The actual strings are owned by the mappings above.
	 */
	GPtrArray *label_buf;
};

struct bt_field_class_real {
	struct bt_field_class common;
};

struct bt_field_class_string {
	struct bt_field_class common;
};

/* A named field class is a (name, field class) pair */
struct bt_named_field_class {
	/* MIP > 0: `NULL` if not set */
	GString *name;

	/* Owned by this */
	struct bt_value *user_attributes;

	/* Owned by this */
	struct bt_field_class *fc;

	bool frozen;
};

struct bt_field_class_structure_member;
struct bt_field_class_variant_option;
struct bt_field_class_variant_with_selector_field_integer_unsigned_option;
struct bt_field_class_variant_with_selector_field_integer_signed_option;

struct bt_field_class_named_field_class_container {
	struct bt_field_class common;

	/*
	 * Key: `const char *`, not owned by this (owned by named field
	 * class objects contained in `named_fcs` below).
	 *
	 * MIP > 0: the size of this hash table may be less than the
	 * size of `named_fcs` below because, for a variant field class,
	 * its option names are optional.
	 */
	GHashTable *name_to_index;

	/* Array of `struct bt_named_field_class *` */
	GPtrArray *named_fcs;
};

struct bt_field_class_structure {
	struct bt_field_class_named_field_class_container common;
};

struct bt_field_class_array {
	struct bt_field_class common;

	/* Owned by this */
	struct bt_field_class *element_fc;
};

struct bt_field_class_array_static {
	struct bt_field_class_array common;
	uint64_t length;
};

struct bt_field_class_array_dynamic {
	struct bt_field_class_array common;

	/* Used with BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD */
	struct {
		enum field_xref_kind xref_kind;

		union {
			/* Used with FIELD_XREF_KIND_PATH */
			struct {
				/* Owned by this */
				struct bt_field_class *class;

				/* Owned by this */
				struct bt_field_path *path;
			} path;

			/* Used with FIELD_XREF_KIND_LOCATION, owned by this */
			const struct bt_field_location *location;
		};
	} length_field;
};

struct bt_field_class_option {
	struct bt_field_class common;

	/* Owned by this */
	struct bt_field_class *content_fc;
};

struct bt_field_class_option_with_selector_field {
	struct bt_field_class_option common;

	enum field_xref_kind selector_field_xref_kind;

	union {
		/* Used with FIELD_XREF_KIND_PATH */
		struct {
			/* Owned by this */
			struct bt_field_class *class;

			/* Owned by this */
			struct bt_field_path *path;
		} path;

		/* Used with FIELD_XREF_KIND_LOCATION, owned by this */
		const struct bt_field_location *location;
	} selector_field;
};

struct bt_field_class_option_with_selector_field_bool {
	struct bt_field_class_option_with_selector_field common;

	/* Owned by this */
	bool sel_is_reversed;
};

struct bt_field_class_option_with_selector_field_integer {
	struct bt_field_class_option_with_selector_field common;

	/* Owned by this */
	const struct bt_integer_range_set *range_set;
};

/* Variant FC (with selector) option: named field class + range set */
struct bt_field_class_variant_with_selector_field_option {
	struct bt_named_field_class common;

	/* Owned by this */
	const struct bt_integer_range_set *range_set;
};

struct bt_field_class_variant {
	/*
	 * Depending on the variant field class type, the contained
	 * named field classes are of type
	 * `struct bt_named_field_class *` if the variant field class
	 * doesn't have a selector, or
	 * `struct bt_field_class_variant_with_selector_field_option *`
	 * if it has.
	 */
	struct bt_field_class_named_field_class_container common;
};

struct bt_field_class_variant_with_selector_field {
	struct bt_field_class_variant common;

	enum field_xref_kind selector_field_xref_kind;

	union {
		/* Used with FIELD_XREF_KIND_PATH */
		struct {
			/*
			* Owned by this, but never dereferenced: only use to find it
			* elsewhere.
			*/
			const struct bt_field_class *class;

			/* Owned by this */
			struct bt_field_path *path;
		} path;

		/* Used with FIELD_XREF_KIND_LOCATION, owned by this */
		const struct bt_field_location *location;
	} selector_field;
};

struct bt_field_class_blob {
	struct bt_field_class common;

	GString *media_type;
};

struct bt_field_class_blob_static {
	struct bt_field_class_blob common;
	uint64_t length;
};

struct bt_field_class_blob_dynamic {
	struct bt_field_class_blob common;
	const struct bt_field_location *length_fl;
};

void _bt_field_class_freeze(const struct bt_field_class *field_class);

#ifdef BT_DEV_MODE
# define bt_field_class_freeze		_bt_field_class_freeze
#else
# define bt_field_class_freeze(_fc)	((void) _fc)
#endif

void _bt_named_field_class_freeze(const struct bt_named_field_class *named_fc);

#ifdef BT_DEV_MODE
# define bt_named_field_class_freeze		_bt_named_field_class_freeze
#else
# define bt_named_field_class_freeze(_named_fc)	((void) _named_fc)
#endif

/*
 * This function recursively marks `field_class` and its children as
 * being part of a trace. This is used to validate that all field classes
 * are used at a single location within trace objects even if they are
 * shared objects for other purposes.
 */
void bt_field_class_make_part_of_trace_class(
		const struct bt_field_class *field_class);

#endif /* BABELTRACE_LIB_TRACE_IR_FIELD_CLASS_H */
