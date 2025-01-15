# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#


import unittest

import bt2

query_object = "metadata-info"


class QuerySupportInfoTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin("ctf")
        assert ctf
        self._fs = ctf.source_component_classes["fs"]

    def test_non_map_params(self):
        with self.assertRaisesRegex(
            bt2._Error, "Error validating parameters: top-level is not a map value"
        ):
            bt2.QueryExecutor(self._fs, query_object).query()


if __name__ == "__main__":
    unittest.main()
