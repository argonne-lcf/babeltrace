# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import unittest

import bt2
import utils
from bt2 import field as bt2_field
from bt2 import stream as bt2_stream
from bt2 import event_class as bt2_event_class
from bt2 import clock_snapshot as bt2_clock_snapshot
from utils import TestOutputPortMessageIterator


class EventTestCase(unittest.TestCase):
    def _create_test_const_event_message(
        self,
        packet_fields_config=None,
        event_fields_config=None,
        with_clockclass=False,
        with_cc=False,
        with_sc=False,
        with_ep=False,
        with_packet=False,
    ):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, self_output_port):
                self._at = 0
                self._msgs = [self._create_stream_beginning_message(test_obj.stream)]

                if with_packet:
                    assert test_obj.packet
                    self._msgs.append(
                        self._create_packet_beginning_message(test_obj.packet)
                    )

                default_clock_snapshot = 789 if with_clockclass else None

                if with_packet:
                    assert test_obj.packet
                    ev_parent = test_obj.packet
                else:
                    assert test_obj.stream
                    ev_parent = test_obj.stream

                msg = self._create_event_message(
                    test_obj.event_class, ev_parent, default_clock_snapshot
                )

                if event_fields_config is not None:
                    event_fields_config(msg.event)

                self._msgs.append(msg)

                if with_packet:
                    self._msgs.append(self._create_packet_end_message(test_obj.packet))

                self._msgs.append(self._create_stream_end_message(test_obj.stream))

            def __next__(self):
                if self._at == len(self._msgs):
                    raise bt2.Stop

                msg = self._msgs[self._at]
                self._at += 1
                return msg

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port("out")
                tc = self._create_trace_class()
                stream_class = tc.create_stream_class(
                    default_clock_class=(
                        self._create_clock_class(frequency=1000)
                        if with_clockclass
                        else None
                    ),
                    event_common_context_field_class=(
                        tc.create_structure_field_class(
                            members=(
                                ("cpu_id", tc.create_signed_integer_field_class(8)),
                                (
                                    "stuff",
                                    tc.create_double_precision_real_field_class(),
                                ),
                                ("gnu", tc.create_string_field_class()),
                            )
                        )
                        if with_cc
                        else None
                    ),
                    packet_context_field_class=(
                        tc.create_structure_field_class(
                            members=(
                                (
                                    "something",
                                    tc.create_unsigned_integer_field_class(8),
                                ),
                                (
                                    "something_else",
                                    tc.create_double_precision_real_field_class(),
                                ),
                            )
                        )
                        if with_packet
                        else None
                    ),
                    supports_packets=with_packet,
                )
                stream = tc().create_stream(stream_class)

                if with_packet:
                    packet = stream.create_packet()

                if packet_fields_config is not None:
                    assert packet
                    packet_fields_config(packet)

                if with_packet:
                    test_obj.packet = packet

                test_obj.stream = stream
                test_obj.event_class = stream_class.create_event_class(
                    name="garou",
                    specific_context_field_class=(
                        tc.create_structure_field_class(
                            members=(
                                ("ant", tc.create_signed_integer_field_class(16)),
                                ("msg", tc.create_string_field_class()),
                            )
                        )
                        if with_sc
                        else None
                    ),
                    payload_field_class=(
                        tc.create_structure_field_class(
                            members=(
                                ("giraffe", tc.create_signed_integer_field_class(32)),
                                ("gnu", tc.create_signed_integer_field_class(8)),
                                ("mosquito", tc.create_signed_integer_field_class(8)),
                            )
                        )
                        if with_ep
                        else None
                    ),
                )

        test_obj = self
        self._graph = bt2.Graph()
        self._src_comp = self._graph.add_component(MySrc, "my_source")
        self._msg_iter = TestOutputPortMessageIterator(
            self._graph, self._src_comp.output_ports["out"]
        )

        for msg in self._msg_iter:
            if type(msg) is bt2._EventMessageConst:
                self._event_msg = msg
                return msg

    def test_const_attr_event_class(self):
        msg = self._create_test_const_event_message()
        self.assertEqual(msg.event.cls.addr, self.event_class.addr)
        self.assertIs(type(msg.event.cls), bt2_event_class._EventClassConst)

    def test_attr_event_class(self):
        msg = utils.get_event_message()
        self.assertIs(type(msg.event.cls), bt2_event_class._EventClass)

    def test_const_attr_name(self):
        msg = self._create_test_const_event_message()
        self.assertEqual(msg.event.name, self.event_class.name)

    def test_const_attr_id(self):
        msg = self._create_test_const_event_message()
        self.assertEqual(msg.event.id, self.event_class.id)

    def test_const_get_common_context_field(self):
        def event_fields_config(event):
            event.common_context_field["cpu_id"] = 1
            event.common_context_field["stuff"] = 13.194
            event.common_context_field["gnu"] = "salut"

        msg = self._create_test_const_event_message(
            event_fields_config=event_fields_config, with_cc=True
        )

        self.assertEqual(msg.event.common_context_field["cpu_id"], 1)
        self.assertEqual(msg.event.common_context_field["stuff"], 13.194)
        self.assertEqual(msg.event.common_context_field["gnu"], "salut")
        self.assertIs(
            type(msg.event.common_context_field), bt2_field._StructureFieldConst
        )

    def test_attr_common_context_field(self):
        msg = utils.get_event_message()
        self.assertIs(type(msg.event.common_context_field), bt2_field._StructureField)

    def test_const_no_common_context_field(self):
        msg = self._create_test_const_event_message(with_cc=False)
        self.assertIsNone(msg.event.common_context_field)

    def test_const_get_specific_context_field(self):
        def event_fields_config(event):
            event.specific_context_field["ant"] = -1
            event.specific_context_field["msg"] = "hellooo"

        msg = self._create_test_const_event_message(
            event_fields_config=event_fields_config, with_sc=True
        )

        self.assertEqual(msg.event.specific_context_field["ant"], -1)
        self.assertEqual(msg.event.specific_context_field["msg"], "hellooo")
        self.assertIs(
            type(msg.event.specific_context_field), bt2_field._StructureFieldConst
        )

    def test_attr_specific_context_field(self):
        msg = utils.get_event_message()
        self.assertIs(type(msg.event.specific_context_field), bt2_field._StructureField)

    def test_const_no_specific_context_field(self):
        msg = self._create_test_const_event_message(with_sc=False)
        self.assertIsNone(msg.event.specific_context_field)

    def test_const_get_event_payload_field(self):
        def event_fields_config(event):
            event.payload_field["giraffe"] = 1
            event.payload_field["gnu"] = 23
            event.payload_field["mosquito"] = 42

        msg = self._create_test_const_event_message(
            event_fields_config=event_fields_config, with_ep=True
        )

        self.assertEqual(msg.event.payload_field["giraffe"], 1)
        self.assertEqual(msg.event.payload_field["gnu"], 23)
        self.assertEqual(msg.event.payload_field["mosquito"], 42)
        self.assertIs(type(msg.event.payload_field), bt2_field._StructureFieldConst)

    def test_attr_payload_field(self):
        msg = utils.get_event_message()
        self.assertIs(type(msg.event.payload_field), bt2_field._StructureField)

    def test_const_no_payload_field(self):
        msg = self._create_test_const_event_message(with_ep=False)
        self.assertIsNone(msg.event.payload_field)

    def test_const_clock_value(self):
        msg = self._create_test_const_event_message(with_clockclass=True)
        self.assertEqual(msg.default_clock_snapshot.value, 789)
        self.assertIs(
            type(msg.default_clock_snapshot), bt2_clock_snapshot._ClockSnapshotConst
        )

    def test_clock_value(self):
        msg = utils.get_event_message()
        self.assertEqual(msg.default_clock_snapshot.value, 789)
        self.assertIs(
            type(msg.default_clock_snapshot), bt2_clock_snapshot._ClockSnapshotConst
        )

    def test_const_no_clock_value(self):
        msg = self._create_test_const_event_message(with_clockclass=False)
        with self.assertRaisesRegex(
            ValueError, "stream class has no default clock class"
        ):
            msg.default_clock_snapshot

    def test_const_stream(self):
        msg = self._create_test_const_event_message()
        self.assertEqual(msg.event.stream.addr, self.stream.addr)
        self.assertIs(type(msg.event.stream), bt2_stream._StreamConst)

    def test_stream(self):
        msg = utils.get_event_message()
        self.assertIs(type(msg.event.stream), bt2_stream._Stream)

    @staticmethod
    def _event_payload_fields_config(event):
        event.payload_field["giraffe"] = 1
        event.payload_field["gnu"] = 23
        event.payload_field["mosquito"] = 42

    @staticmethod
    def _event_fields_config(event):
        EventTestCase._event_payload_fields_config(event)
        event.specific_context_field["ant"] = -1
        event.specific_context_field["msg"] = "hellooo"
        event.common_context_field["cpu_id"] = 1
        event.common_context_field["stuff"] = 13.194
        event.common_context_field["gnu"] = "salut"

    @staticmethod
    def _packet_fields_config(packet):
        packet.context_field["something"] = 154
        packet.context_field["something_else"] = 17.2

    def test_const_getitem(self):
        msg = self._create_test_const_event_message(
            packet_fields_config=self._packet_fields_config,
            event_fields_config=self._event_fields_config,
            with_cc=True,
            with_sc=True,
            with_ep=True,
            with_packet=True,
        )
        ev = msg.event

        # Test event fields
        self.assertEqual(ev["giraffe"], 1)
        self.assertIs(type(ev["giraffe"]), bt2_field._SignedIntegerFieldConst)
        self.assertEqual(ev["gnu"], 23)
        self.assertEqual(ev["mosquito"], 42)
        self.assertEqual(ev["ant"], -1)
        self.assertIs(type(ev["ant"]), bt2_field._SignedIntegerFieldConst)
        self.assertEqual(ev["msg"], "hellooo")
        self.assertEqual(ev["cpu_id"], 1)
        self.assertIs(type(ev["cpu_id"]), bt2_field._SignedIntegerFieldConst)
        self.assertEqual(ev["stuff"], 13.194)

        # Test packet fields
        self.assertEqual(ev["something"], 154)
        self.assertIs(type(ev["something"]), bt2_field._UnsignedIntegerFieldConst)
        self.assertEqual(ev["something_else"], 17.2)

        with self.assertRaises(KeyError):
            ev["yes"]

    def test_const_getitem_no_packet(self):
        msg = self._create_test_const_event_message(
            event_fields_config=self._event_payload_fields_config,
            with_ep=True,
        )
        ev = msg.event

        with self.assertRaises(KeyError):
            ev["yes"]

    def test_getitem(self):
        msg = utils.get_event_message()
        ev = msg.event
        self.assertEqual(ev["giraffe"], 1)
        self.assertIs(type(ev["giraffe"]), bt2_field._SignedIntegerField)
        self.assertEqual(ev["ant"], -1)
        self.assertIs(type(ev["ant"]), bt2_field._SignedIntegerField)
        self.assertEqual(ev["cpu_id"], 1)
        self.assertIs(type(ev["cpu_id"]), bt2_field._SignedIntegerField)
        self.assertEqual(ev["something"], 154)
        self.assertIs(type(ev["something"]), bt2_field._UnsignedIntegerField)

    def test_iter_full(self):
        msg = self._create_test_const_event_message(
            packet_fields_config=self._packet_fields_config,
            event_fields_config=self._event_fields_config,
            with_cc=True,
            with_sc=True,
            with_ep=True,
            with_packet=True,
        )
        expected_field_names = [
            # payload
            "giraffe",
            "gnu",
            "mosquito",
            # specific context
            "ant",
            "msg",
            # common context
            "cpu_id",
            "stuff",
            # packet context
            "something",
            "something_else",
        ]
        self.assertEqual(list(msg.event), expected_field_names)

    def test_iter_payload_only(self):
        msg = self._create_test_const_event_message(
            event_fields_config=self._event_payload_fields_config,
            with_ep=True,
        )
        expected_field_names = [
            # payload
            "giraffe",
            "gnu",
            "mosquito",
        ]
        self.assertEqual(list(msg.event), expected_field_names)

    def test_len_full(self):
        msg = self._create_test_const_event_message(
            packet_fields_config=self._packet_fields_config,
            event_fields_config=self._event_fields_config,
            with_cc=True,
            with_sc=True,
            with_ep=True,
            with_packet=True,
        )
        self.assertEqual(len(msg.event), 9)

    def test_len_payload_only(self):
        msg = self._create_test_const_event_message(
            packet_fields_config=None,
            event_fields_config=self._event_payload_fields_config,
            with_ep=True,
        )
        self.assertEqual(len(msg.event), 3)

    def test_in_full(self):
        msg = self._create_test_const_event_message(
            packet_fields_config=self._packet_fields_config,
            event_fields_config=self._event_fields_config,
            with_cc=True,
            with_sc=True,
            with_ep=True,
            with_packet=True,
        )
        field_names = [
            # payload
            "giraffe",
            "gnu",
            "mosquito",
            # specific context
            "ant",
            "msg",
            # common context
            "cpu_id",
            "stuff",
            # packet context
            "something",
            "something_else",
        ]

        for field_name in field_names:
            self.assertTrue(field_name in msg.event)

        self.assertFalse("lol" in msg.event)

    def test_in_payload_only(self):
        msg = self._create_test_const_event_message(
            packet_fields_config=None,
            event_fields_config=self._event_payload_fields_config,
            with_ep=True,
        )
        field_names = [
            "giraffe",
            "gnu",
            "mosquito",
        ]

        for field_name in field_names:
            self.assertTrue(field_name in msg.event)

        self.assertFalse("lol" in msg.event)


if __name__ == "__main__":
    unittest.main()
