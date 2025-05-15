#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

data_dir="$BT_TESTS_DATADIR/cli/exit-status"
source_name="src.test-exit-status.StatusSrc"

test_interrupted_graph() {
	local cli_args=("--plugin-path=$data_dir" "-c" "$source_name" "-p" "case=\"INTERRUPTED\"")

	bt_test_cli "interrupted graph" \
		--expect-stdout /dev/null --expect-exit-status 2 -- "${cli_args[@]}"
}

test_error_graph() {
	local cli_args=("--plugin-path=$data_dir" "-c" "$source_name" "-p" "case=\"ERROR\"")
	local actual_stdout
	local actual_stderr

	actual_stdout=$(mktemp -t test-cli-exit-status-stdout-actual.XXXXXX)
	actual_stderr=$(mktemp -t test-cli-exit-status-stderr-actual.XXXXXX)

	bt_cli --stdout-file "$actual_stdout" --stderr-file "$actual_stderr" -- "${cli_args[@]}"

	is $? 1 "Erroring graph exits with status 1"

	bt_diff /dev/null "$actual_stdout"
	ok $? "Erroring graph gives expected stdout"

	like "$(cat "${actual_stderr}")" "TypeError: Raising type error" \
		"Erroring graph gives expected error message"

	rm -f "${actual_stdout}"
	rm -f "${actual_stderr}"
}

test_stop_graph() {
	local cli_args=("--plugin-path=$data_dir" "-c" "$source_name" "-p" "case=\"STOP\"")
	local actual_stdout
	local actual_stderr

	actual_stdout=$(mktemp -t test-cli-exit-status-stdout-actual.XXXXXX)
	actual_stderr=$(mktemp -t test-cli-exit-status-stderr-actual.XXXXXX)

	bt_cli --stdout-file "$actual_stdout" --stderr-file "$actual_stderr" -- "${cli_args[@]}"

	is $? 0 "Successful graph exits with status 0"

	bt_diff /dev/null "$actual_stdout"
	ok $? "Successful graph gives no stdout"

	bt_diff /dev/null "$actual_stderr"
	ok $? "Successful graph gives no stderr"

	rm -f "${actual_stdout}"
	rm -f "${actual_stderr}"
}

plan_tests 9

test_interrupted_graph
test_error_graph
test_stop_graph
