#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

# This file tests what happens when we trim at different points in the message
# flow.

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
source "$UTILSSH"

data_dir="$BT_TESTS_DATADIR/plugins/flt.utils.trimmer"
temp_stdout_expected=$(mktemp)

plan_tests 168

function run_test
{
	local begin_time="$1"
	local end_time="$2"
	# with_stream_msgs_cs and with_packet_msgs are set to "true" or "false"
	# by the tests.
	local local_args=(
		"--plugin-path" "$data_dir"
		"-c" "src.test-trimmer.TheSourceOfAllEvil"
		"-p" "with-stream-msgs-cs=$with_stream_msgs_cs"
		"-p" "with-packet-msgs=$with_packet_msgs"
		"-c" "sink.text.details"
		"--params=compact=true,with-metadata=false"
	)

	if [ "$with_stream_msgs_cs" = "true" ]; then
		test_name="with stream message clock snapshots"
	else
		test_name="without stream message clock snapshots"
	fi

	if [ "$with_packet_msgs" = "true" ]; then
		test_name="$test_name, with packet messages"
	else
		test_name="$test_name, without packet messages"
	fi

	if [ -n "$begin_time" ]; then
		local_args+=("--begin=$begin_time")
		test_name="$test_name, with --begin=$begin_time"
	else
		test_name="$test_name, without --begin"
	fi

	if [ -n "$end_time" ]; then
		local_args+=("--end=$end_time")
		test_name="$test_name, with --end=$end_time"
	else
		test_name="$test_name, without --end"
	fi

	bt_test_cli "$test_name" --expect-stdout "$temp_stdout_expected" -- \
		"${local_args[@]}"
}

function test_with_stream_msg_cs_with_packets {
	with_stream_msgs_cs="true"
	with_packet_msgs="true"

	# Baseline (without trimming)
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test "" ""

	# Trim begin at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 50 ""

	# Trim begin before stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 10050 ""

	# Trim begin before packet beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[150 10,150,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 10150 ""

	# Trim begin before first event
	cat <<- 'END' > "$temp_stdout_expected"
	[250 10,250,000,000,000] {0 0 0} Stream beginning
	[250 10,250,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 10250 ""

	# Trim begin before second event
	cat <<- 'END' > "$temp_stdout_expected"
	[350 10,350,000,000,000] {0 0 0} Stream beginning
	[350 10,350,000,000,000] {0 0 0} Packet beginning
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END

	run_test 10350 ""

	# Trim begin before packet end
	cat <<- 'END' > "$temp_stdout_expected"
	[850 10,850,000,000,000] {0 0 0} Stream beginning
	[850 10,850,000,000,000] {0 0 0} Packet beginning
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END

	run_test 10850 ""

	# Trim begin after everything
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test 11050 ""

	# Trim end after stream end
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END

	run_test "" 11050

	# Trim end after packet end
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[950 10,950,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10950

	# Trim end after second event
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[450 10,450,000,000,000] {0 0 0} Packet end
	[450 10,450,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10450

	# Trim end after first event
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[350 10,350,000,000,000] {0 0 0} Packet end
	[350 10,350,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10350

	# Trim end after packet beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[250 10,250,000,000,000] {0 0 0} Packet end
	[250 10,250,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10250

	# Trim end after stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[150 10,150,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10150

	# Trim end before everything
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test "" 10050

	# Trim end at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test "" 50
}

function test_without_stream_msg_cs_with_packets {
	with_stream_msgs_cs="false"
	with_packet_msgs="true"

	# Baseline (without trimming)
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END
	run_test "" ""

	# Trim begin at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END
	run_test 50 ""

	# Trim begin before stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END
	run_test 10050 ""

	# Trim begin before packet beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END
	run_test 10150 ""

	# Trim begin before first event
	cat <<- 'END' > "$temp_stdout_expected"
	[250 10,250,000,000,000] {0 0 0} Stream beginning
	[250 10,250,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END
	run_test 10250 ""

	# Trim begin before second event
	cat <<- 'END' > "$temp_stdout_expected"
	[350 10,350,000,000,000] {0 0 0} Stream beginning
	[350 10,350,000,000,000] {0 0 0} Packet beginning
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END

	run_test 10350 ""

	# Trim begin before packet end
	cat <<- 'END' > "$temp_stdout_expected"
	[850 10,850,000,000,000] {0 0 0} Stream beginning
	[850 10,850,000,000,000] {0 0 0} Packet beginning
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END

	run_test 10850 ""

	# Trim begin after everything
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test 11050 ""

	# Trim end after stream end
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 11050

	# Trim end after packet end
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[900 10,900,000,000,000] {0 0 0} Packet end
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 10950

	# Trim end after second event
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[450 10,450,000,000,000] {0 0 0} Packet end
	[450 10,450,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10450

	# Trim end after first event
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[350 10,350,000,000,000] {0 0 0} Packet end
	[350 10,350,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10350

	# Trim end after packet beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[200 10,200,000,000,000] {0 0 0} Packet beginning
	[250 10,250,000,000,000] {0 0 0} Packet end
	[250 10,250,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10250

	# Trim end after stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 10150

	# Trim end before everything
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 10050

	# Trim end at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 50
}

function test_with_stream_msg_cs_without_packets {
	with_stream_msgs_cs="true"
	with_packet_msgs="false"

	# Baseline (without trimming)
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test "" ""

	# Trim begin at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 50 ""

	# Trim begin before stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 10050 ""

	# Trim begin before first event
	cat <<- 'END' > "$temp_stdout_expected"
	[250 10,250,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END
	run_test 10250 ""

	# Trim begin before second event
	cat <<- 'END' > "$temp_stdout_expected"
	[350 10,350,000,000,000] {0 0 0} Stream beginning
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END

	run_test 10350 ""

	# Trim begin before packet end
	cat <<- 'END' > "$temp_stdout_expected"
	[850 10,850,000,000,000] {0 0 0} Stream beginning
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END

	run_test 10850 ""

	# Trim begin after everything
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test 11050 ""

	# Trim end after stream end
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[1000 11,000,000,000,000] {0 0 0} Stream end
	END

	run_test "" 11050

	# Trim end after packet end
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[950 10,950,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10950

	# Trim end after second event
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[450 10,450,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10450

	# Trim end after first event
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[350 10,350,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10350

	# Trim end after packet beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[250 10,250,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10250

	# Trim end after stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[100 10,100,000,000,000] {0 0 0} Stream beginning
	[150 10,150,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10150

	# Trim end before everything
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test "" 10050

	# Trim end at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test "" 50
}

function test_without_stream_msg_cs_without_packets {
	with_stream_msgs_cs="false"
	with_packet_msgs="false"

	# Baseline (without trimming)
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[Unknown] {0 0 0} Stream end
	END
	run_test "" ""

	# Trim begin at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[Unknown] {0 0 0} Stream end
	END
	run_test 50 ""

	# Trim begin before stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[Unknown] {0 0 0} Stream end
	END
	run_test 10050 ""

	# Trim begin before second event
	cat <<- 'END' > "$temp_stdout_expected"
	[350 10,350,000,000,000] {0 0 0} Stream beginning
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[Unknown] {0 0 0} Stream end
	END

	run_test 10350 ""

	# Trim begin after everything
	cat <<- 'END' > "$temp_stdout_expected"
	END

	run_test 11050 ""

	# Trim end after stream end
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[400 10,400,000,000,000] {0 0 0} Event `event 2` (1)
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 11050

	# Trim end after first event
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[300 10,300,000,000,000] {0 0 0} Event `event 1` (0)
	[350 10,350,000,000,000] {0 0 0} Stream end
	END

	run_test "" 10350

	# Trim end after stream beginning
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 10150

	# Trim end at a time before what the clock class can represent
	cat <<- 'END' > "$temp_stdout_expected"
	[Unknown] {0 0 0} Stream beginning
	[Unknown] {0 0 0} Stream end
	END

	run_test "" 50
}

test_with_stream_msg_cs_with_packets
test_without_stream_msg_cs_with_packets
test_with_stream_msg_cs_without_packets
test_without_stream_msg_cs_without_packets

rm -f "$temp_stdout_expected"
