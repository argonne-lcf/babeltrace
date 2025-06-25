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

plan_tests 21

stdout=$(mktemp -t test-help-stdout.XXXXXX)
stderr=$(mktemp -t test-help-stderr.XXXXXX)

# Return 0 if file "$1" exists and is empty, non-0 otherwise.

is_empty()
{
	[[ -f "$1" && ! -s "$1" ]]
}

# Test with a working plugin name.
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help ctf
ok $? "help ctf plugin exit status"

bt_grep_ok \
	'Description: CTF input and output' \
	"${stdout}" \
	"help ctf plugin expected output"

is_empty "${stderr}"
ok $? "help ctf plugin produces no error"

# Test with a working component class name.
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help src.ctf.fs
ok $? "help src.ctf.fs component class exit status"

bt_grep_ok \
	'Description: Read CTF traces from the file system.' \
	"${stdout}" \
	"help src.ctf.fs component class expected output"

is_empty "${stderr}"
ok $? "help src.ctf.fs component class produces no error"

# Test without parameter.
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help
isnt $? 0 "help without parameter exit status"

bt_grep_ok \
	"Missing plugin name or component class descriptor." \
	"${stderr}" \
	"help without parameter produces expected error"

is_empty "${stdout}"
ok $? "help without parameter produces no output"

# Test with too many parameters.
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help ctf fs
isnt $? 0  "help with too many parameters exit status"

bt_grep_ok \
	"Extraneous command-line argument specified to \`help\` command:" \
	"${stderr}" \
	"help with too many parameters produces expected error"

is_empty "${stdout}"
ok $? "help with too many parameters produces no output"

# Test with unknown plugin name.
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help zigotos
isnt $? 0 "help with unknown plugin name"

bt_grep_ok \
	'Cannot find plugin: plugin-name="zigotos"' \
	"${stderr}" \
	"help with unknown plugin name produces expected error"

is_empty "${stdout}"
ok $? "help with unknown plugin name produces no output"

# Test with unknown component class name (but known plugin).
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help src.ctf.bob
isnt $? 0 "help with unknown component class name"

bt_grep_ok \
	'Cannot find component class: plugin-name="ctf", comp-cls-name="bob", comp-cls-type=SOURCE' \
	"${stderr}" \
	"help with unknown component class name produces expected error"

bt_grep_ok \
	'Description: CTF input and output' \
	"${stdout}" \
	"help with unknown component class name prints plugin help"

# Test with unknown component class plugin
bt_cli --stdout-file "${stdout}" --stderr-file "${stderr}" -- help src.bob.fs
isnt $? 0 "help with unknown component class plugin"

bt_grep_ok \
	'Cannot find plugin: plugin-name="bob"' \
	"${stderr}" \
	"help with unknown component class plugin produces expected error"

is_empty "${stdout}"
ok $? "help with unknown component class plugin produces no output"

rm -f "${stdout}" "${stderr}"
