# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import unittest

import utils
from bt2 import field as bt2_field
from bt2 import stream as bt2_stream
from utils import run_in_component_init


class PacketTestCase(unittest.TestCase):
    @staticmethod
    def _create_packet(with_pc):
        def create_tc_cc(comp_self):
            cc = comp_self._create_clock_class(frequency=1000, name="my_cc")
            tc = comp_self._create_trace_class()
            return cc, tc

        clock_class, tc = run_in_component_init(0, create_tc_cc)

        # packet context
        pc = (
            tc.create_structure_field_class(
                members=(
                    ("something", tc.create_signed_integer_field_class(8)),
                    ("something_else", tc.create_double_precision_real_field_class()),
                    ("events_discarded", tc.create_unsigned_integer_field_class(64)),
                    ("packet_seq_num", tc.create_unsigned_integer_field_class(64)),
                )
            )
            if with_pc
            else None
        )

        # stream
        stream = tc().create_stream(
            tc.create_stream_class(
                default_clock_class=clock_class,
                event_common_context_field_class=tc.create_structure_field_class(
                    members=(
                        ("cpu_id", tc.create_signed_integer_field_class(8)),
                        ("stuff", tc.create_double_precision_real_field_class()),
                    )
                ),
                packet_context_field_class=pc,
                supports_packets=True,
            )
        )

        # packet
        return stream.create_packet(), stream, pc

    def test_attr_stream(self):
        packet, stream, _ = self._create_packet(with_pc=True)
        self.assertEqual(packet.stream.addr, stream.addr)
        self.assertIs(type(packet.stream), bt2_stream._Stream)

    def test_const_attr_stream(self):
        packet = utils.get_const_packet_beginning_message().packet
        self.assertIs(type(packet.stream), bt2_stream._StreamConst)

    def test_context_field(self):
        packet, stream, pc_fc = self._create_packet(with_pc=True)
        self.assertEqual(packet.context_field.cls.addr, pc_fc.addr)
        self.assertIs(type(packet.context_field), bt2_field._StructureField)

    def test_const_context_field(self):
        packet = utils.get_const_packet_beginning_message().packet
        self.assertIs(type(packet.context_field), bt2_field._StructureFieldConst)

    def test_no_context_field(self):
        packet, _, _ = self._create_packet(with_pc=False)
        self.assertIsNone(packet.context_field)


if __name__ == "__main__":
    unittest.main()
