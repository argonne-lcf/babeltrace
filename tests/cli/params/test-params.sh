#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

# Test how parameters are applied to sources auto-discovered by the convert
# command.

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
SH_TAP=1 source "$UTILSSH"

NUM_TESTS=27

plan_tests $NUM_TESTS

data_dir="${BT_TESTS_DATADIR}/cli/params"
plugin_dir="${data_dir}"

expected_file=$(mktemp -t expected.XXXXXX)

function expect_success
{
	local test_name="$1"
	local params_str="$2"
	local expected_str="$3"

	echo "$expected_str" > "$expected_file"

	bt_test_cli "$test_name" --expect-stdout "$expected_file" -- \
		--plugin-path "$plugin_dir" -c "src.text.dmesg" \
		-c "sink.params.SinkThatPrintsParams" --params "$params_str"
}

expect_success 'null' 'a=null,b=nul,c=NULL' \
	'{a=None, b=None, c=None}'
expect_success 'bool' 'a=true,b=TRUE,c=yes,d=YES,e=false,f=FALSE,g=no,h=NO' \
	'{a=True, b=True, c=True, d=True, e=False, f=False, g=False, h=False}'
expect_success 'signed integer' 'a=0b110, b=022, c=22, d=0x22' \
	'{a=6, b=18, c=22, d=34}'
expect_success 'unsigned integer' 'a=+0b110, b=+022, c=+22, d=+0x22' \
	'{a=6u, b=18u, c=22u, d=34u}'
expect_success 'string' 'a="avril lavigne", b=patata, c="This\"is\\escaped"' \
	'{a=avril lavigne, b=patata, c=This"is\escaped}'
expect_success 'float' 'a=1.234, b=17., c=.28, d=-18.28' \
	'{a=1.2340000, b=17.0000000, c=0.2800000, d=-18.2800000}'
expect_success 'float scientific notation' 'a=10.5e6, b=10.5E6, c=10.5e-6, d=10.5E-6' \
	'{a=10500000.0000000, b=10500000.0000000, c=0.0000105, d=0.0000105}'
expect_success 'array' 'a=[1, [["hi",]]]' \
	'{a=[1, [[hi]]]}'
expect_success 'map' 'a=4,a={},b={salut="la gang",comment="ca va",oh={x=2}}' \
	'{a={}, b={comment=ca va, oh={x=2}, salut=la gang}}'

rm -f "$expected_file"
