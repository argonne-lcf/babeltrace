# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2025 EfficiOS, Inc.
#

# pyright: strict, reportPrivateUsage=false

import unittest

import bt2


class QueryTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin("ctf")
        assert ctf
        self._live = ctf.source_component_classes["lttng-live"]
        assert self._live

    def test_sessions_no_params(self):
        with self.assertRaisesRegex(
            bt2._Error, "Error validating parameters: top-level is not a map value"
        ):
            bt2.QueryExecutor(self._live, "sessions").query()

    def test_support_info_no_params(self):
        with self.assertRaisesRegex(
            bt2._Error, "Error validating parameters: top-level is not a map value"
        ):
            bt2.QueryExecutor(self._live, "babeltrace.support-info").query()


if __name__ == "__main__":
    unittest.main()
