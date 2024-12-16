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

clean_tmp() {
	rm -rf "${out_path}" "${text_output1}" "${text_output2_intermediary}" "${text_output2}" "${stderr_file}"
}

plan_tests 163

for ctf_version in 1 2; do
	for path in "${BT_CTF_TRACES_PATH}/$ctf_version/succeed/"*; do
		out_path="$(mktemp -d)"
		text_output1="$(mktemp)"
		text_output2_intermediary="$(mktemp)"
		text_output2="$(mktemp)"
		stderr_file="$(mktemp)"
		trace="$(basename "${path}")"
		sort_cmd="cat" # by default do not sort the trace

		bt_cli "${text_output1}" "/dev/null" --no-delta "${path}"
		ret=$?
		cnt="$(wc -l < "${text_output1}")"
		if test "$ret" == 0 && test "${cnt// /}" == 0; then
			pass "Empty trace ${trace}, nothing to copy (CTF $ctf_version)"
			clean_tmp
			continue
		fi

		# If the trace has a timestamp (starts with [), check if there are
		# duplicate timestamps in the output.
		# If there are, we have to sort the text output to make sure it is
		# always the same.
		head -1 "${text_output1}" | bt_grep "^\[" >/dev/null
		if test $? = 0; then
			# shellcheck disable=SC2016
			uniq_ts_cnt="$("${BT_TESTS_AWK_BIN}" '{ print $1 }' < "${text_output1}" | sort | uniq | wc -l)"
			# Extract only the timestamp columns and compare the number of
			# unique lines with the total number of lines to see if there
			# are duplicate timestamps.
			if test "${cnt// /}" != "${uniq_ts_cnt// /}"; then
				diag "Trace with non unique timestamps, sorting the output (CTF $ctf_version)"
				sort_cmd="sort"
				tmp="$(mktemp)"
				sort "${text_output1}" > "$tmp"
				rm "${text_output1}"
				text_output1="$tmp"
			fi
		fi

		bt_cli "/dev/null" "${stderr_file}" "${path}" --component sink.ctf.fs "--params=path=\"${out_path}\",ctf-version=\"$ctf_version\""
		if ! ok $? "Copy trace ${trace} with ctf-fs sink (CTF $ctf_version)"; then
			diag "stderr:"
			diag_file "${stderr_file}"
		fi

		bt_cli "/dev/null" "${stderr_file}" "${out_path}"
		if ! ok $? "Read the new trace in ${out_path} (CTF $ctf_version)"; then
			diag "stderr:"
			diag_file "${stderr_file}"
		fi

		if ! bt_cli "${text_output2_intermediary}" "${stderr_file}" --no-delta "${out_path}"; then
			diag "stderr:"
			diag_file "${stderr_file}"
		fi

		$sort_cmd "${text_output2_intermediary}" > "${text_output2}"
		bt_diff "${text_output1}" "${text_output2}"
		ok $? "Exact same content between the two traces (CTF $ctf_version)"

		clean_tmp
	done
done
