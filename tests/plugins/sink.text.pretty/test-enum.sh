#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020 Geneviève Bastien <gbastien@versatic.net>
#
# This file tests pretty printing in details some event classes that are
# not all covered by the main babeltrace tests with traces.
SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

data_dir="$BT_TESTS_DATADIR/plugins/sink.text.pretty"
temp_stdout_expected_file=$(mktemp -t test-pretty-expected-stdout.XXXXXX)

plan_tests 31

function compare_enum_sorted
{
	local expected_file="$1"
	local actual_file="$2"

	# The order in which enum labels are printed by a `sink.text.pretty`
	# component directly depends on the order in which mappings were added
	# to the enum field class in the source component. This order should
	# not be relied on when testing. Relying on it caused problems with
	# Python component classes because different versions of Python sort
	# data structures differently (e.g. dictionaries are insertion sorted
	# since Python 3.7).

	bt_run_in_py_env "${BT_TESTS_PYTHON_BIN}" "${BT_TESTS_SRCDIR}/utils/python/split_sort_compare.py" \
		"$(cat "$expected_file")" "$(cat "$actual_file")"
}

function run_test
{
	local test_name=$1
	local expected_to_fail="$2"
	local value="$3"
	local expected_stdout_file="$4"
	local actual_stdout_file
	local actual_stderr_file
	local ret=0
	local local_args=(
		"--plugin-path" "$data_dir"
		"-c" "src.test-pretty.TheSourceOfProblems"
		"-p" "enum-values=\"$enum_values\""
		"-p" "value=$value"
		"-p" "enum-signed=$enum_signed"
		"-c" "sink.text.pretty"
		"-p" "print-enum-flags=true"
	)

	actual_stdout_file="$(mktemp -t actual-pretty-stdout.XXXXXX)"
	actual_stderr_file="$(mktemp -t actual-pretty-stderr.XXXXXX)"

	bt_cli --stdout-file "$actual_stdout_file" --stderr-file "$actual_stderr_file" -- "${local_args[@]}"

	compare_enum_sorted "$expected_stdout_file" "$actual_stdout_file"
	ret_stdout=$?

	bt_diff /dev/null "$actual_stderr_file"
	ret_stderr=$?

	if ((ret_stdout != 0 || ret_stderr != 0)); then
		ret=1
	fi

	rm -f "$actual_stdout_file" "$actual_stderr_file"

	if [ "$expected_to_fail" = "1" ]; then
		isnt $ret 0 "$test_name signed=$enum_signed with value=$value doesn't match as expected"
	else
		ok $ret "$test_name signed=$enum_signed with value=$value matches"
	fi

}

function test_normal_enum {
	test_name="Normal enum"
	enum_signed="$1"
	enum_values="single,1,1 single2,2,2 single3,4,4 range,4,8 range2,15,20"

	# Hit a single value
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "single" : container = 1 ) }
	END
	run_test "$test_name" 0 1 "$temp_stdout_expected_file"

	# Hit a single range
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "range" : container = 7 ) }
	END
	run_test "$test_name" 0 7 "$temp_stdout_expected_file"

	# Hit a range and a value
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( { "single3", "range" } : container = 4 ) }
	END
	run_test "$test_name" 0 4 "$temp_stdout_expected_file"

	# Hit a range and a value
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( { "NOT_A_LABEL", "DOESNT_EXIST" } : container = 4 ) }
	END
	run_test "$test_name" 1 4 "$temp_stdout_expected_file"

	# Unknown
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = 21 ) }
	END
	run_test "$test_name" 0 21 "$temp_stdout_expected_file"

	# Unknown but with bits with a value, but range larger than 1 element
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = 12 ) }
	END
	run_test "$test_name" 0 12 "$temp_stdout_expected_file"

	# Unknown value of 0
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = 0 ) }
	END
	run_test "$test_name" 0 0 "$temp_stdout_expected_file"
}

function test_normal_enum_negative {
	test_name="Normal enum with negative value"
	enum_signed="true"
	enum_values="zero,0,0 single,1,1 single2,2,2 single3,4,4 range,4,8 negative,-1,-1 rangeNegative,-6,-2"

	# Hit a single negative value
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "negative" : container = -1 ) }
	END
	run_test "$test_name" 0 -1 "$temp_stdout_expected_file"

	# Hit a single negative range
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "rangeNegative" : container = -6 ) }
	END
	run_test "$test_name" 0 -6 "$temp_stdout_expected_file"

	# Unknown
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = -7 ) }
	END
	run_test "$test_name" 0 -7 "$temp_stdout_expected_file"

	# value of 0
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "zero" : container = 0 ) }
	END
	run_test "$test_name" 0 0 "$temp_stdout_expected_file"
}

function test_bit_flag_enum {
	test_name="Bit flag enum"
	enum_signed="false"
	enum_values="bit0,1,1 bit0bis,1,1 bit1,2,2 bit3,4,4 bit4,8,8 bit5,16,16 bit5,32,32"

	# Single value hit
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "bit1" : container = 2 ) }
	END
	run_test "$test_name" 0 2 "$temp_stdout_expected_file"

	# Multiple flags set
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "bit3" | "bit4" : container = 12 ) }
	END
	run_test "$test_name" 0 12 "$temp_stdout_expected_file"

	# Some unknown bit
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = 68 ) }
	END
	run_test "$test_name" 0 68 "$temp_stdout_expected_file"

	# Multiple labels for bit 0
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( { "bit0", "bit0bis" } : container = 1 ) }
	END
	run_test "$test_name" 0 1 "$temp_stdout_expected_file"

	# Two labels for bit 0 and one label for bit 1
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( { "bit0", "bit0bis" } | "bit1" : container = 3 ) }
	END
	run_test "$test_name" 0 3 "$temp_stdout_expected_file"

	# Single label for bit 0
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "bit5" | "bit5" : container = 48 ) }
	END
	run_test "$test_name" 0 48 "$temp_stdout_expected_file"

	# negative value
	enum_signed="true"
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = -1 ) }
	END
	run_test "$test_name" 0 -1 "$temp_stdout_expected_file"
}

function test_mixed_enum {
	test_name="Mixed enum bits at beginning"
	enum_signed="false"
	enum_values="bit0,1,1 bit1,2,2 bit2,4,4 bit3,8,8 bit4,16,16 range,32,44 singleValue,45,45"

	# Value with bit fields
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "bit0" | "bit1" | "bit2" | "bit3" | "bit4" : container = 31 ) }
	END
	run_test "$test_name" 0 31 "$temp_stdout_expected_file"

	# A value with some bit flags set, but within another range
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "range" : container = 36 ) }
	END
	run_test "$test_name" 0 36 "$temp_stdout_expected_file"

	# A value with some bit flags set, but corresponding to another value
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "singleValue" : container = 45 ) }
	END
	run_test "$test_name" 0 45 "$temp_stdout_expected_file"

	# Value above the ranges
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( <unknown> : container = 46 ) }
	END
	run_test "$test_name" 0 46 "$temp_stdout_expected_file"

	# Since low values are often powers of 2, they may be considered bit flags too
	test_name="Mixed enum bits at end"
	enum_signed="false"
	enum_values="val1,1,1 val2,2,2 val3,3,3 val4,4,4 val5,5,5 bit3,8,8 bit4,16,16 bit5,32,32"

	# Value with bit fields
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "bit4" : container = 16 ) }
	END
	run_test "$test_name" 0 16 "$temp_stdout_expected_file"

	# Value with a bit field set both at beginning and end
	cat <<- 'END' > "$temp_stdout_expected_file"
	with_enum: { enum_field = ( "val1" | "bit4" : container = 17 ) }
	END
	run_test "$test_name" 0 17 "$temp_stdout_expected_file"
}

# Enumerations tests
test_normal_enum "false"
test_normal_enum "true"
test_normal_enum_negative
test_bit_flag_enum
test_mixed_enum

rm -f "$temp_stdout_expected_file"
