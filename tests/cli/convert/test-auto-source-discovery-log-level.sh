#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

# Test how log level options are applied to sources auto-discovered by the
# convert command.

if [ -n "${BT_TESTS_SRCDIR:-}" ]; then
	UTILSSH="$BT_TESTS_SRCDIR/utils/utils.sh"
else
	UTILSSH="$(dirname "$0")/../../utils/utils.sh"
fi

# shellcheck source=../../utils/utils.sh
SH_TAP=1 source "$UTILSSH"

NUM_TESTS=12

plan_tests $NUM_TESTS

data_dir="${BT_TESTS_DATADIR}/auto-source-discovery/params-log-level"
plugin_dir="${data_dir}"
dir_a="${data_dir}/dir-a"
dir_b="${data_dir}/dir-b"
dir_ab="${data_dir}/dir-ab"

expected_file=$(mktemp -t expected.XXXXXX)

print_log_level=(--params 'what="log-level"')
details_sink=("-c" "sink.text.details" "--params=with-metadata=false")

# Apply log level to two components from one non-option argument.
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: LoggingLevel.DEBUG
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: LoggingLevel.DEBUG
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_test_cli "apply log level to two components from one non-option argument" \
	 --expect-stdout "$expected_file" -- \
	--plugin-path "${plugin_dir}" convert \
	"${dir_ab}" --log-level DEBUG "${print_log_level[@]}" \
	"${details_sink[@]}"

# Apply log level to two components from two distinct non-option arguments.
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: LoggingLevel.DEBUG
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: LoggingLevel.TRACE
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_test_cli "apply log level to two non-option arguments" \
	--expect-stdout "$expected_file" -- \
	--plugin-path "${plugin_dir}" convert \
	"${dir_a}" --log-level DEBUG "${print_log_level[@]}" "${dir_b}" --log-level TRACE "${print_log_level[@]}" \
	"${details_sink[@]}"

# Apply log level to one component coming from one non-option argument and one component coming from two non-option arguments (1).
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: LoggingLevel.TRACE
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: LoggingLevel.TRACE
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_test_cli "apply log level to one component coming from one non-option argument and one component coming from two non-option arguments (1)" \
	--expect-stdout "$expected_file" -- \
	--plugin-path "${plugin_dir}" convert \
	"${dir_a}" --log-level DEBUG "${print_log_level[@]}" "${dir_ab}" --log-level TRACE "${print_log_level[@]}" \
	"${details_sink[@]}"

# Apply log level to one component coming from one non-option argument and one component coming from two non-option arguments (2).
cat > "$expected_file" <<END
{Trace 0, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceA: LoggingLevel.TRACE
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 1, Stream class ID 0, Stream ID 0}
Stream beginning:
  Name: TestSourceB: LoggingLevel.DEBUG
  Trace:
    Stream (ID 0, Class ID 0)

{Trace 0, Stream class ID 0, Stream ID 0}
Stream end

{Trace 1, Stream class ID 0, Stream ID 0}
Stream end
END

bt_test_cli "apply log level to one component coming from one non-option argument and one component coming from two non-option arguments (2)" \
	--expect-stdout "$expected_file" -- \
	--plugin-path "${plugin_dir}" convert \
	"${dir_ab}" --log-level DEBUG "${print_log_level[@]}" "${dir_a}" --log-level TRACE "${print_log_level[@]}" \
	"${details_sink[@]}"

rm -f "$expected_file"
