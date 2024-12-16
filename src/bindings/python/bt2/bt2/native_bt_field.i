/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 */

/* For label type mappings. */
%include "native_bt_field_class.i"

%typemap(out) const uint8_t * {
	/*
	 * Note that using `arg1` relies on the convention used by SWIG
	 * in its generated code, which is probably not guaranteed to
	 * stay stable.
	 */
	$result = PyMemoryView_FromMemory(reinterpret_cast<char *>($1),
		bt_field_blob_get_length(arg1), PyBUF_READ);
}

%typemap(out) uint8_t * {
	/*
	 * Note that using `arg1` relies on the convention used by SWIG
	 * in its generated code, which is probably not guaranteed to
	 * stay stable.
	 */
	$result = PyMemoryView_FromMemory(reinterpret_cast<char *>($1),
		bt_field_blob_get_length(arg1), PyBUF_WRITE);
}

%include <babeltrace2/trace-ir/field.h>
