#!/bin/bash
#
# SPDX-FileCopyrightText: 2024 EfficiOS Inc.
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

plan_tests 3

bt_test_cli "show basic bit array fields with flags" \
	--expect-stdout "$BT_TESTS_DATADIR/plugins/sink.text.pretty/fl-bm-ctf2.expect" \
	-- \
	"$BT_TESTS_DATADIR/ctf-traces/2/succeed/fl-bm"
