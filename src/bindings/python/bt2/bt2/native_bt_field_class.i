/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 */

/* Parameter names seem to be required for multi-argument typemaps to match. */
%typemap(in, numinputs=0)
	(bt_field_class_enumeration_mapping_label_array *labels, uint64_t *count)
	(bt_field_class_enumeration_mapping_label_array temp_array, uint64_t temp_label_count = 0) {
	$1 = &temp_array;
	$2 = &temp_label_count;
}

%typemap(argout)
	(bt_field_class_enumeration_mapping_label_array *labels, uint64_t *count) {
	if (result == __BT_FUNC_STATUS_OK) {
		PyObject *py_label_list = PyList_New(*$2);
		uint64_t i;

		for (i = 0; i < *$2; i++) {
			PyList_SET_ITEM(py_label_list, i, PyUnicode_FromString((*$1)[i]));
		}

		$result = SWIG_AppendOutput($result, py_label_list);
	} else {
		Py_INCREF(Py_None);
		$result = SWIG_AppendOutput($result, Py_None);
	}
}

%typemap(in, numinputs=0)
	(bt_field_class_bit_array_flag_label_array *labels, uint64_t *count)
	(bt_field_class_bit_array_flag_label_array temp_array, uint64_t temp_label_count = 0) {
	$1 = &temp_array;
	$2 = &temp_label_count;
}

%typemap(argout)
	(bt_field_class_bit_array_flag_label_array *labels, uint64_t *count) {
	if (result == __BT_FUNC_STATUS_OK) {
		PyObject *py_label_list = PyList_New(*$2);
		uint64_t i;

		for (i = 0; i < *$2; i++) {
			PyList_SET_ITEM(py_label_list, i, PyUnicode_FromString((*$1)[i]));
		}

		$result = SWIG_AppendOutput($result, py_label_list);
	} else {
		Py_INCREF(Py_None);
		$result = SWIG_AppendOutput($result, Py_None);
	}
}

/*
 * The default typemap for enums uses `SWIG_From_int()`, which causes
 * values to be truncated to `int`. For `bt_field_class_type`, we need
 * more than 32 bits.  Use this custom typemap to avoid truncation.
 */
%typemap(out) bt_field_class_type {
	$result = PyLong_FromLongLong($1);
}

%include <babeltrace2/trace-ir/field-class.h>
