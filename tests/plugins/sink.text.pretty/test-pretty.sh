#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

plan_tests 1

bt_diff_cli "$BT_TESTS_DATADIR/plugins/sink.text.pretty/fl-bm-ctf2.expect" /dev/null \
	"$BT_TESTS_DATADIR/ctf-traces/2/succeed/fl-bm"
ok "$?" "show basic bit array fields with flags"
