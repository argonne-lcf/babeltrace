#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2020 EfficiOS Inc.
#

# This file tests the assume-single-trace parameter of sink.ctf.fs.

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

# Directory containing the Python test source.
data_dir="$BT_TESTS_DATADIR/plugins/sink.ctf.fs/assume-single-trace"

temp_stdout=$(mktemp)
temp_expected_stdout=$(mktemp)
temp_stderr=$(mktemp)
temp_output_dir=$(mktemp -d)

trace_dir="$temp_output_dir/the-trace"

if [ "$BT_TESTS_ENABLE_PYTHON_PLUGINS" != "1" ]; then
	plan_skip_all "This test requires the Python plugin provider"
	exit
fi

plan_tests 9

bt_cli --stdout-file "$temp_stdout" --stderr-file "$temp_stderr" -- \
	"--plugin-path=${data_dir}" \
	-c src.foo.TheSource \
	-c sink.ctf.fs -p "path=\"${trace_dir}\"" -p 'assume-single-trace=true' -p 'ctf-version="1"'
ok "$?" "run sink.ctf.fs with assume-single-trace=true"

# Check stdout.
if [ "$BT_TESTS_OS_TYPE" = "mingw" ]; then
	echo "Created CTF trace \`$(cygpath -m "${trace_dir}")\`." > "$temp_expected_stdout"
else
	echo "Created CTF trace \`${trace_dir}\`." > "$temp_expected_stdout"
fi
bt_diff "$temp_expected_stdout" "$temp_stdout"
ok "$?" "expected message on stdout"

# Check stderr.
bt_diff "/dev/null" "$temp_stderr"
ok "$?" "stderr is empty"

# Verify only the expected files exist.
files=("$trace_dir"/*)
num_files=${#files[@]}
is "$num_files" "2" "expected number of files in output directory"

test -f "$trace_dir/metadata"
ok "$?" "metadata file exists"

test -f "$trace_dir/the-stream"
ok "$?" "the-stream file exists"

# Read back the output trace to make sure it's properly formed.
echo "the-event: " > "$temp_expected_stdout"
bt_test_cli "read back output trace" --expect-stdout "$temp_expected_stdout" -- \
	"$trace_dir"

rm -f "$temp_stdout"
rm -f "$temp_stderr"
rm -f "$temp_expected_stdout"
rm -f "$trace_dir/metadata"
rm -f "$trace_dir/the-stream"
rmdir "$trace_dir"
rmdir "$temp_output_dir"
