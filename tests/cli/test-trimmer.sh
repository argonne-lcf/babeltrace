#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2017 Julien Desfossez <jdesfossez@efficios.com>
#

SH_TAP=1

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../utils/utils.sh"
fi

# shellcheck source=../utils/utils.sh
source "$UTILSSH"

TRACE_PATH="${BT_CTF_TRACES_PATH}/1/succeed/wk-heartbeat-u/"

NUM_TESTS=118

plan_tests $NUM_TESTS

tmp_out=$(mktemp)
tmp_err=$(mktemp)


# Run Babeltrace with some command line arguments, verify exit status and
# number of output events (i.e. number of output lines)
#
# Arguments:
#
#   $1: expected number of events
#   $2: test description
#   remaining arguments: command-line arguments to pass to Babeltrace

function expect_success()
{
	local expected_num_events="$1"
	local msg="$2"
	shift 2

	bt_cli --stdout-file "${tmp_out}" -- "${TRACE_PATH}" "$@"
	ok $? "trimmer: ${msg}: exit status"

	num_events=$(wc -l < "${tmp_out}")
	# Use bash parameter expansion to strip spaces added by BSD 'wc' on macOs and Solaris
	is "${num_events// /}" "${expected_num_events}" "trimmer: ${msg}: number of events (${expected_num_events})"
}

# Run Babeltrace with some command line arguments, verify that the exit status
# is not 0 and that the error message contains a given string.
#
# Arguments:
#
#   $1: a string expected to be found in the error message
#   $2: test description
#   remaining arguments: command-line arguments to pass to Babeltrace

function expect_failure()
{
	local expected_err_string="$1"
	local msg="$2"
	shift 2

	# We check the error message logged by the trimmer plugin, set the env
	# var necessary for it to log errors.
	BABELTRACE_FLT_UTILS_TRIMMER_LOG_LEVEL=E bt_cli --stdout-file "${tmp_out}" --stderr-file "${tmp_err}" -- "${TRACE_PATH}" "$@"
	isnt $? 0 "trimmer: ${msg}: exit status"

	num_events=$(wc -l < "${tmp_out}")
	# Use bash parameter expansion to strip spaces added by BSD 'wc' on macOs and Solaris
	is "${num_events// /}" 0 "trimmer: ${msg}: number of events (0)"

	stderr="$(cat "${tmp_err}")"
	# "like" doesn't like when the passed text is empty.
	if [ -n "${stderr}" ]; then
		like "${stderr}" "${expected_err_string}" "trimmer: ${msg}: error message"
	else
		fail "trimmer: ${msg}: error message"
		diag "Nothing was output on stderr".
	fi

}

expect_success 18 "--begin, GMT relative timestamps" \
	--clock-gmt --begin 17:48:17.587029529
expect_success 9 "--end, GMT relative timestamps" \
	--clock-gmt --end 17:48:17.588680018
expect_success 7 "--begin and --end, GMT relative timestamps" \
	--clock-gmt --begin 17:48:17.587029529 --end 17:48:17.588680018
expect_success 0 "--begin, out of range, GMT relative timestamps" \
	--clock-gmt --begin 18:48:17.587029529
expect_success 0 "--end, out of range, GMT relative timestamps" \
	--clock-gmt --end 16:48:17.588680018

expect_success 18 "--begin, GMT absolute timestamps" \
	--clock-gmt --begin "2012-10-29 17:48:17.587029529"
expect_success 9 "--end, GMT absolute timestamps" \
	--clock-gmt --end "2012-10-29 17:48:17.588680018"
expect_success 7 "--begin and --end, GMT absolute timestamps" \
	--clock-gmt --begin "2012-10-29 17:48:17.587029529" --end "2012-10-29 17:48:17.588680018"
expect_success 0 "--begin, out of range, GMT absolute timestamps" \
	--clock-gmt --begin "2012-10-29 18:48:17.587029529"
expect_success 0 "--begin, out of range, GMT absolute timestamps" \
	--clock-gmt --end "2012-10-29 16:48:17.588680018"

# Note here that the POSIX notation is a bit weird.
# The libc documentation shed some light on this:
#  The offset specifies the time value you must add to the local time to get a
#  Coordinated Universal Time value. It has syntax like [+|-]hh[:mm[:ss]]. This
#  is positive if the local time zone is west of the Prime Meridian and negative
#  if it is east. The hour must be between 0 and 24, and the minute and seconds
#  between 0 and 59. [1]
#
# This is why we use EST5 to simulate an effective UTC-5:00 time.
#
# [1] https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
export TZ=EST5

expect_success 18 "--begin, EST relative timestamps" \
	--begin "12:48:17.587029529"
expect_success 9 "--end, EST relative timestamps" \
	--end "12:48:17.588680018"
expect_success 7 "--begin and --end, EST relative timestamps" \
	--begin "12:48:17.587029529" --end "12:48:17.588680018"
expect_success 0 "--begin, out of range, EST relative timestamps" \
	--begin "13:48:17.587029529"
expect_success 0 "--end, out of range, EST relative timestamps" \
	--end "11:48:17.588680018"

expect_success 18 "--begin, EST absolute timestamps" \
	--begin "2012-10-29 12:48:17.587029529"
expect_success 9 "--end, EST absolute timestamps" \
	--end "12:48:17.588680018"
expect_success 7 "--begin and --end, EST absolute timestamps" \
	--begin "2012-10-29 12:48:17.587029529" --end "2012-10-29 12:48:17.588680018"
expect_success 0 "--begin, out of range, EST absolute timestamps" \
	--begin "2012-10-29 13:48:17.587029529"
expect_success 0 "--end, out of range, EST absolute timestamps" \
	--end "2012-10-29 11:48:17.588680018"

# Check various formats.
#
# We sometimes apply a clock offset to make the events of the trace span two
# different seconds or minutes.

expect_success 13 "date time format: partial nanosecond precision" \
	--begin="2012-10-29 12:48:17.588"
expect_success 11 "date time format: second precision" \
	--clock-offset-ns=411282268 --begin="2012-10-29 12:48:18"
expect_success 11 "date time format: minute precision" \
	--clock-offset=42 --clock-offset-ns=411282268 --begin="2012-10-29 12:49"

expect_success 11 "seconds from origin format: nanosecond precision" \
	--begin="1351532897.588717732"
expect_success 11 "seconds from origin format: partial nanosecond precision" \
	--begin="1351532897.58871773"
expect_success 11 "seconds from origin format: second precision" \
	--clock-offset-ns=411282268 --begin="1351532898"

expect_failure "Invalid date/time format" "date time format: too many nanosecond digits" \
	--begin="2012-10-29 12:48:17.1231231231"
expect_failure "Invalid date/time format" "date time format: missing nanoseconds" \
	--begin="2012-10-29 12:48:17."
expect_failure "Invalid date/time format" "date time format: seconds with too many digit" \
	--begin="2012-10-29 12:48:123"
expect_failure "Invalid date/time format" "date time format: seconds with missing digit" \
	--begin="2012-10-29 12:48:1"
expect_failure "Invalid date/time format" "date time format: minutes with too many digit" \
	--begin="2012-10-29 12:489:17"
expect_failure "Invalid date/time format" "date time format: minutes with missing digit" \
	--begin="2012-10-29 12:4:17"
expect_failure "Invalid date/time format" "date time format: hours with too many digit" \
	--begin="2012-10-29 123:48:17"
expect_failure "Invalid date/time format" "date time format: hours with missing digit" \
	--begin="2012-10-29 2:48:17"
expect_failure "Invalid date/time format" "date time format: missing seconds" \
	--begin="2012-10-29 12:48:"
expect_failure "Invalid date/time format" "date time format: missing minutes 1" \
	--begin="2012-10-29 12:"
expect_failure "Invalid date/time format" "date time format: missing minutes 2" \
	--begin="2012-10-29 12"
expect_failure "Invalid date/time format" "date time format: missing time" \
	--begin="2012-10-29 "
expect_failure "Invalid date/time format" "date time format: day with too many digit" \
	--begin="2012-10-291"
expect_failure "Invalid date/time format" "date time format: day with missing digit" \
	--begin="2012-10-2"
expect_failure "Invalid date/time format" "date time format: month with too many digit" \
	--begin="2012-101-29"
expect_failure "Invalid date/time format" "date time format: month with missing digit" \
	--begin="2012-1-29"
expect_failure "Invalid date/time format" "date time format: year with too many digits" \
	--begin="20121-10-29"
expect_failure "Invalid date/time format" "date time format: year with missing digits" \
	--begin="12-10-29"
expect_failure "Invalid date/time format" "date time format: missing day 1" \
	--begin="2012-10-"
expect_failure "Invalid date/time format" "date time format: missing day 2" \
	--begin="2012-10"

expect_failure "Invalid date/time format" "seconds from origin format: too many nanosecond digits" \
	--begin="1351532898.1231231231"
expect_failure "Invalid date/time format" "seconds from origin format: missing nanseconds" \
	--begin="1351532898."

rm "${tmp_out}" "${tmp_err}"
