# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

# pyright: strict, reportPrivateUsage=false

import math
import typing

import bt2

bt2.register_plugin(__name__, "trace-ir-test")

_FieldType = typing.TypeVar("_FieldType", bound=bt2._Field)


def c(field: bt2._Field, expected_type: typing.Type[_FieldType]) -> _FieldType:
    assert type(field) is expected_type
    return field


class AllFieldsIter(bt2._UserMessageIterator):
    def __init__(
        self,
        config: bt2._MessageIteratorConfiguration,
        output_port: bt2._UserComponentOutputPort,
    ):
        ec = typing.cast(bt2._EventClass, output_port.user_data)
        sc = ec.stream_class
        tc = sc.trace_class
        stream = tc().create_stream(sc)
        ev = self._create_event_message(ec, stream)

        payload = ev.event.payload_field
        assert payload

        payload["bool"] = False
        payload["real_single"] = 2.0
        payload["real_double"] = math.pi
        payload["int32"] = 121
        payload["int3"] = -1
        payload["int9_hex"] = -92
        payload["uint32"] = 121
        payload["uint61"] = 299792458
        payload["uint5_oct"] = 29

        struct = c(payload["struct"], bt2._StructureField)
        struct["str"] = "Rotisserie St-Hubert"
        struct["option_real"] = math.pi

        payload["string"] = "🎉"
        payload["dyn_array"] = [1.2, 2 / 3, 42.3, math.pi]
        payload["dyn_array_len"] = 4
        payload["dyn_array_with_len"] = [5.2, 5 / 3, 42.5, math.pi * 12]
        payload["sta_array"] = ["🕰", "🦴", " 🎍"]
        payload["option_none"]
        payload["option_some"] = "NORMANDIN"
        payload["option_bool_selector"] = True
        payload["option_bool"] = "Mike's"
        payload["option_int_selector"] = 1
        payload["option_int"] = "Barbies resto bar grill"

        variant = c(payload["variant"], bt2._VariantField)
        variant.selected_option_index = 0
        variant.value = "Couche-Tard"

        self._msgs = [
            self._create_stream_beginning_message(stream),
            ev,
            self._create_stream_end_message(stream),
        ]

    def __next__(self):
        if len(self._msgs) > 0:
            return self._msgs.pop(0)
        else:
            raise StopIteration


@bt2.plugin_component_class
class AllFields(bt2._UserSourceComponent, message_iterator_class=AllFieldsIter):
    def __init__(
        self,
        config: bt2._UserSourceComponentConfiguration,
        params: bt2._MapValueConst,
        obj: object,
    ):
        tc = self._create_trace_class()

        dyn_array_with_len_fc = tc.create_unsigned_integer_field_class(19)
        option_bool_selector_fc = tc.create_bool_field_class()
        option_int_selector_fc = tc.create_unsigned_integer_field_class(8)

        ec = tc.create_stream_class().create_event_class(
            name="my-event",
            payload_field_class=tc.create_structure_field_class(
                members=(
                    ("bool", tc.create_bool_field_class()),
                    ("real_single", tc.create_single_precision_real_field_class()),
                    ("real_double", tc.create_double_precision_real_field_class()),
                    ("int32", tc.create_signed_integer_field_class(32)),
                    ("int3", tc.create_signed_integer_field_class(3)),
                    (
                        "int9_hex",
                        tc.create_signed_integer_field_class(
                            9,
                            preferred_display_base=bt2.IntegerDisplayBase.HEXADECIMAL,
                        ),
                    ),
                    ("uint32", tc.create_unsigned_integer_field_class(32)),
                    ("uint61", tc.create_unsigned_integer_field_class(61)),
                    (
                        "uint5_oct",
                        tc.create_unsigned_integer_field_class(
                            5, preferred_display_base=bt2.IntegerDisplayBase.OCTAL
                        ),
                    ),
                    (
                        "struct",
                        tc.create_structure_field_class(
                            members=(
                                ("str", tc.create_string_field_class()),
                                (
                                    "option_real",
                                    tc.create_option_field_class_without_selector_field(
                                        tc.create_double_precision_real_field_class()
                                    ),
                                ),
                            )
                        ),
                    ),
                    ("string", tc.create_string_field_class()),
                    (
                        "dyn_array",
                        tc.create_dynamic_array_field_class(
                            tc.create_double_precision_real_field_class()
                        ),
                    ),
                    ("dyn_array_len", dyn_array_with_len_fc),
                    (
                        "dyn_array_with_len",
                        tc.create_dynamic_array_field_class(
                            tc.create_double_precision_real_field_class(),
                            length_fc=dyn_array_with_len_fc,
                        ),
                    ),
                    (
                        "sta_array",
                        tc.create_static_array_field_class(
                            tc.create_string_field_class(), 3
                        ),
                    ),
                    (
                        "option_none",
                        tc.create_option_field_class_without_selector_field(
                            tc.create_double_precision_real_field_class()
                        ),
                    ),
                    (
                        "option_some",
                        tc.create_option_field_class_without_selector_field(
                            tc.create_string_field_class()
                        ),
                    ),
                    ("option_bool_selector", option_bool_selector_fc),
                    (
                        "option_bool",
                        tc.create_option_field_class_with_bool_selector_field(
                            tc.create_string_field_class(), option_bool_selector_fc
                        ),
                    ),
                    (
                        "option_bool_reversed",
                        tc.create_option_field_class_with_bool_selector_field(
                            tc.create_string_field_class(),
                            option_bool_selector_fc,
                            selector_is_reversed=True,
                        ),
                    ),
                    ("option_int_selector", option_int_selector_fc),
                    (
                        "option_int",
                        tc.create_option_field_class_with_integer_selector_field(
                            tc.create_string_field_class(),
                            option_int_selector_fc,
                            bt2.UnsignedIntegerRangeSet([(1, 3), (18, 44)]),
                        ),
                    ),
                    (
                        "variant",
                        tc.create_variant_field_class(
                            options=(("var_str", tc.create_string_field_class()),)
                        ),
                    ),
                )
            ),
        )

        self._add_output_port("out", ec)
