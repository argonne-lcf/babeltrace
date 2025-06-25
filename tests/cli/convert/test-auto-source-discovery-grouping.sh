#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

# Test the auto source disovery mechanism of the CLI.

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
SH_TAP=1 source "$UTILSSH"

NUM_TESTS=3

plan_tests $NUM_TESTS

data_dir="${BT_TESTS_DATADIR}/auto-source-discovery/grouping"
plugin_dir="${data_dir}"
trace_dir="${data_dir}/traces"

stdout_expected_file="${BT_TESTS_DATADIR}/cli/convert/auto-source-discovery-grouping.expect"
stdout_actual_file=$(mktemp -t stdout-actual.XXXXXX)
stderr_actual_file=$(mktemp -t actual-stderr.XXXXXX)

bt_cli --stdout-file "$stdout_actual_file" --stderr-file "$stderr_actual_file" -- \
	--plugin-path "${plugin_dir}" convert "ABCDE" "${trace_dir}" some_other_non_opt \
	-c sink.text.details --params='with-metadata=false'
ok "$?" "CLI runs successfully"

# Check components and their inputs.
bt_diff "$stdout_expected_file" "$stdout_actual_file"
ok "$?" "expected components are instantiated with expected inputs"

# Check that expected warning is printed.
# shellcheck disable=SC2016
bt_grep_ok \
	'No trace was found based on input `some_other_non_opt`' \
	"$stderr_actual_file" \
	"warning is printed"

rm -f "$stdout_actual_file"
rm -f "$stderr_actual_file"
