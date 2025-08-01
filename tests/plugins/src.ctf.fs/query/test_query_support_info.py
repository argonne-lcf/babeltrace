# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#

import os
import unittest

import bt2
from test_all_ctf_versions import test_all_ctf_versions

query_object = "babeltrace.support-info"


@test_all_ctf_versions
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

    def test_support_info_with_uuid(self):
        # Test that the right group is reported for each trace.

        session_rotation_trace_path = os.path.join(
            os.environ["BT_CTF_TRACES_PATH"],
            str(self._ctf_version),
            "succeed",
            "session-rotation",
        )

        trace_10352_1 = os.path.join(
            session_rotation_trace_path,
            "a",
            "1",
            "ust",
            "pid",
            "10352",
        )
        trace_10353_1 = os.path.join(
            session_rotation_trace_path,
            "a",
            "1",
            "ust",
            "pid",
            "10353",
        )
        trace_10352_2 = os.path.join(
            session_rotation_trace_path,
            "a",
            "2",
            "ust",
            "pid",
            "10352",
        )
        trace_10353_2 = os.path.join(
            session_rotation_trace_path,
            "a",
            "2",
            "ust",
            "pid",
            "10353",
        )
        trace_10352_3 = os.path.join(
            session_rotation_trace_path,
            "3",
            "ust",
            "pid",
            "10352",
        )
        trace_10353_3 = os.path.join(
            session_rotation_trace_path,
            "3",
            "ust",
            "pid",
            "10353",
        )

        def do_one_query(input, expected_group):
            qe = bt2.QueryExecutor(
                self._fs,
                query_object,
                {"input": input, "type": "directory"},
            )

            result = qe.query()
            self.assertEqual(result["group"], expected_group)

        do_one_query(
            trace_10352_1,
            "namespace: lttng.org,2009, name: , uid: 21cdfa5e-9a64-490a-832c-53aca6c101ba",
        )
        do_one_query(
            trace_10352_2,
            "namespace: lttng.org,2009, name: , uid: 21cdfa5e-9a64-490a-832c-53aca6c101ba",
        )
        do_one_query(
            trace_10352_3,
            "namespace: lttng.org,2009, name: , uid: 21cdfa5e-9a64-490a-832c-53aca6c101ba",
        )
        do_one_query(
            trace_10353_1,
            "namespace: lttng.org,2009, name: , uid: 83656eb1-b131-40e7-9666-c04ae279b58c",
        )
        do_one_query(
            trace_10353_2,
            "namespace: lttng.org,2009, name: , uid: 83656eb1-b131-40e7-9666-c04ae279b58c",
        )
        do_one_query(
            trace_10353_3,
            "namespace: lttng.org,2009, name: , uid: 83656eb1-b131-40e7-9666-c04ae279b58c",
        )


if __name__ == "__main__":
    unittest.main()
