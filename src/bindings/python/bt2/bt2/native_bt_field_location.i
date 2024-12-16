/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 EfficiOS Inc.
 */

%{
#include "py-common/py-common.hpp"
%}

%typemap(in) (const char *const *items, uint64_t item_count) (std::vector<const char *> strings) {
	strings = btPyStrListToVector($input);
	$1 = const_cast<char **>(strings.data());
	$2 = strings.size();
}

%include <babeltrace2/trace-ir/field-location.h>
