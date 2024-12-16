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
        mip = self._component._graph_mip_version
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

        # Bit array (with flags if MIP > 0)
        c(payload["bit-array"], bt2._BitArrayField).value_as_integer = 0b1000000

        payload["int32"] = 121
        payload["int3"] = -1
        payload["int9_hex"] = -92
        payload["uint32"] = 121
        payload["uint61"] = 299792458
        payload["uint5_oct"] = 29

        # Struct
        struct = c(payload["struct"], bt2._StructureField)
        struct["str"] = "Rotisserie St-Hubert"
        struct["option_real"] = math.pi

        payload["string"] = "🎉"

        # Static array
        payload["static-array"] = ["🕰", "🦴", " 🎍"]

        # Dynamic array without length field
        payload["dynamic-array-without-length-field"] = [1.2, 2 / 3, 42.3, math.pi]

        # Dynamic array with length field
        payload["dynamic-array-with-length-field-length"] = 4
        payload["dynamic-array-with-length-field"] = [5.2, 5 / 3, 42.5, math.pi * 12]

        # Option without selector field
        payload["option-without-selector-field-with-value"] = "NORMANDIN"

        # Options with bool selector field
        payload["option-with-bool-selector-field-selector-true"] = True
        payload["option-with-bool-selector-field-selector-false"] = False
        payload["option-with-bool-selector-field-with-value"] = "Mike's"
        payload["option-with-reversed-bool-selector-field-with-value"] = "Boustan"

        # Option with unsigned integer selector field
        payload["option-with-unsigned-integer-selector-field-selector"] = 1
        payload[
            "option-with-unsigned-integer-selector-field"
        ] = "Barbies resto bar grill"

        # Option with signed integer selector field
        payload["option-with-signed-integer-selector-field-selector"] = 1
        payload["option-with-signed-integer-selector-field"] = "Pacini"

        # Variant without selector field
        variant = c(payload["variant-without-selector-field"], bt2._VariantField)
        variant.selected_option_index = 1
        variant.value = "Couche-Tard"

        # Variant with unsigned integer selector field
        c(
            payload["variant-with-unsigned-integer-selector-field-selector"],
            bt2._UnsignedIntegerField,
        ).value = 5
        variant_with_unsigned_integer_selector_field = c(
            payload["variant-with-unsigned-integer-selector-field"],
            bt2._VariantFieldWithUnsignedIntegerSelectorField,
        )
        variant_with_unsigned_integer_selector_field.selected_option_index = 0
        variant_with_unsigned_integer_selector_field.value = "Marco Smith"

        # Variant with signed integer selector field
        c(
            payload["variant-with-signed-integer-selector-field-selector"],
            bt2._SignedIntegerField,
        ).value = 5
        variant_with_signed_integer_selector_field = c(
            payload["variant-with-signed-integer-selector-field"],
            bt2._VariantFieldWithSignedIntegerSelectorField,
        )
        variant_with_signed_integer_selector_field.selected_option_index = 0
        variant_with_signed_integer_selector_field.value = "Pascal Lefebvre"

        if mip > 0:
            # Static blob
            c(payload["static-blob"], bt2._StaticBlobField).data = b"Masbourian"

            # Dynamic blob without length field
            dynamic_blob_without_length_field = c(
                payload["dynamic-blob-without-length-field"], bt2._DynamicBlobField
            )
            dynamic_blob_without_length_field.length = 3
            dynamic_blob_without_length_field.data = b"mdr"

            # Dynamic blob with length field
            c(
                payload["dynamic-blob-with-length-field-length"],
                bt2._UnsignedIntegerField,
            ).value = 5
            dynamic_blob_with_length_field = c(
                payload["dynamic-blob-with-length-field"],
                bt2._DynamicBlobFieldWithLengthField,
            )
            dynamic_blob_with_length_field.length = 5
            dynamic_blob_with_length_field.data = b"buick"

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
    @staticmethod
    def _user_get_supported_mip_versions(
        params: bt2._MapValueConst, obj: object, log_level: bt2.LoggingLevel
    ):
        return [0, 1]

    def __init__(
        self,
        config: bt2._UserSourceComponentConfiguration,
        params: bt2._MapValueConst,
        obj: object,
    ):
        mip = self._graph_mip_version
        tc = self._create_trace_class()

        # Dynamic array with length field
        dynamic_array_with_length_field_length = tc.create_unsigned_integer_field_class(
            19
        )

        # Options with bool selector field
        option_with_bool_selector_field_selector_true = tc.create_bool_field_class()
        option_with_bool_selector_field_selector_false = tc.create_bool_field_class()
        option_with_bool_selector_field_selector_true_fl = (
            tc.create_field_location(
                bt2.FieldLocationScope.EVENT_PAYLOAD,
                ["option-with-bool-selector-field-selector-true"],
            )
            if mip > 0
            else None
        )
        option_with_bool_selector_field_selector_false_fl = (
            tc.create_field_location(
                bt2.FieldLocationScope.EVENT_PAYLOAD,
                ["option-with-bool-selector-field-selector-false"],
            )
            if mip > 0
            else None
        )

        # Options with integer selector field
        option_with_unsigned_integer_selector_field_selector = (
            tc.create_unsigned_integer_field_class(8)
        )
        option_with_unsigned_integer_selector_field_ranges = (
            bt2.UnsignedIntegerRangeSet([(1, 3), (18, 44)])
        )
        option_with_signed_integer_selector_field_selector = (
            tc.create_signed_integer_field_class(8)
        )
        option_with_signed_integer_selector_field_ranges = bt2.SignedIntegerRangeSet(
            [(1, 3), (18, 44)]
        )

        # Variants with integer selector field
        variant_with_unsigned_selector_field_selector = (
            tc.create_unsigned_integer_field_class(8)
        )
        variant_with_unsigned_selector_field_options = (
            (
                "var_str",
                tc.create_string_field_class(),
                bt2.UnsignedIntegerRangeSet([(5, 5)]),
            ),
        )
        variant_with_signed_selector_field_selector = (
            tc.create_signed_integer_field_class(8)
        )
        variant_with_signed_selector_field_options = (
            (
                "var_str",
                tc.create_string_field_class(),
                bt2.SignedIntegerRangeSet([(5, 5)]),
            ),
        )

        payload = tc.create_structure_field_class(
            members=(
                ("bool", tc.create_bool_field_class()),
                ("real_single", tc.create_single_precision_real_field_class()),
                ("real_double", tc.create_double_precision_real_field_class()),
                (
                    "bit-array",
                    tc.create_bit_array_field_class(
                        16,
                        flags=(
                            (
                                (
                                    "flag1",
                                    bt2.UnsignedIntegerRangeSet(((1, 3), (6, 9))),
                                ),
                                ("flag2", bt2.UnsignedIntegerRangeSet([(4, 6)])),
                            )
                            if mip > 0
                            else None
                        ),
                    ),
                ),
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
                    "dynamic-array-without-length-field",
                    tc.create_dynamic_array_field_class(
                        tc.create_double_precision_real_field_class()
                    ),
                ),
                (
                    "dynamic-array-with-length-field-length",
                    dynamic_array_with_length_field_length,
                ),
                (
                    "dynamic-array-with-length-field",
                    (
                        tc.create_dynamic_array_field_class(
                            tc.create_double_precision_real_field_class(),
                            length_fc=dynamic_array_with_length_field_length,
                        )
                        if mip == 0
                        else tc.create_dynamic_array_field_class(
                            tc.create_double_precision_real_field_class(),
                            length_field_location=tc.create_field_location(
                                bt2.FieldLocationScope.EVENT_PAYLOAD, ["dyn_array_len"]
                            ),
                        )
                    ),
                ),
                (
                    "static-array",
                    tc.create_static_array_field_class(
                        tc.create_string_field_class(), 3
                    ),
                ),
                (
                    "option-without-selector-field-without-value",
                    tc.create_option_field_class_without_selector_field(
                        tc.create_double_precision_real_field_class()
                    ),
                ),
                (
                    "option-without-selector-field-with-value",
                    tc.create_option_field_class_without_selector_field(
                        tc.create_string_field_class()
                    ),
                ),
                (
                    "option-with-bool-selector-field-selector-true",
                    option_with_bool_selector_field_selector_true,
                ),
                (
                    "option-with-bool-selector-field-selector-false",
                    option_with_bool_selector_field_selector_false,
                ),
                (
                    "option-with-bool-selector-field-with-value",
                    (
                        (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_fc=option_with_bool_selector_field_selector_true,
                            )
                        )
                        if mip == 0
                        else (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_field_location=option_with_bool_selector_field_selector_true_fl,
                            )
                        )
                    ),
                ),
                (
                    "option-with-bool-selector-field-without-value",
                    (
                        (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_fc=option_with_bool_selector_field_selector_false,
                            )
                        )
                        if mip == 0
                        else (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_field_location=option_with_bool_selector_field_selector_false_fl,
                            )
                        )
                    ),
                ),
                (
                    "option-with-reversed-bool-selector-field-with-value",
                    (
                        (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_fc=option_with_bool_selector_field_selector_false,
                                selector_is_reversed=True,
                            )
                        )
                        if mip == 0
                        else (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_field_location=option_with_bool_selector_field_selector_false_fl,
                                selector_is_reversed=True,
                            )
                        )
                    ),
                ),
                (
                    "option-with-reversed-bool-selector-field-without-value",
                    (
                        (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_fc=option_with_bool_selector_field_selector_true,
                                selector_is_reversed=True,
                            )
                        )
                        if mip == 0
                        else (
                            tc.create_option_field_class_with_bool_selector_field(
                                tc.create_string_field_class(),
                                selector_field_location=option_with_bool_selector_field_selector_true_fl,
                                selector_is_reversed=True,
                            )
                        )
                    ),
                ),
                (
                    "option-with-unsigned-integer-selector-field-selector",
                    option_with_unsigned_integer_selector_field_selector,
                ),
                (
                    "option-with-unsigned-integer-selector-field",
                    (
                        (
                            tc.create_option_field_class_with_integer_selector_field(
                                tc.create_string_field_class(),
                                selector_fc=option_with_unsigned_integer_selector_field_selector,
                                ranges=option_with_unsigned_integer_selector_field_ranges,
                            )
                        )
                        if mip == 0
                        else (
                            tc.create_option_field_class_with_unsigned_integer_selector_field(
                                tc.create_string_field_class(),
                                selector_field_location=tc.create_field_location(
                                    bt2.FieldLocationScope.EVENT_PAYLOAD,
                                    [
                                        "option-with-unsigned-integer-selector-field-selector"
                                    ],
                                ),
                                ranges=option_with_unsigned_integer_selector_field_ranges,
                            )
                        )
                    ),
                ),
                (
                    "option-with-signed-integer-selector-field-selector",
                    option_with_signed_integer_selector_field_selector,
                ),
                (
                    "option-with-signed-integer-selector-field",
                    (
                        (
                            tc.create_option_field_class_with_integer_selector_field(
                                tc.create_string_field_class(),
                                selector_fc=option_with_signed_integer_selector_field_selector,
                                ranges=option_with_signed_integer_selector_field_ranges,
                            )
                        )
                        if mip == 0
                        else (
                            tc.create_option_field_class_with_signed_integer_selector_field(
                                tc.create_string_field_class(),
                                selector_field_location=tc.create_field_location(
                                    bt2.FieldLocationScope.EVENT_PAYLOAD,
                                    [
                                        "option-with-signed-integer-selector-field-selector"
                                    ],
                                ),
                                ranges=option_with_signed_integer_selector_field_ranges,
                            )
                        )
                    ),
                ),
                (
                    "variant-without-selector-field",
                    (
                        tc.create_variant_field_class
                        if mip == 0
                        else tc.create_variant_field_class_without_selector_field
                    )(
                        options=(
                            ("opt1", tc.create_string_field_class()),
                            (
                                "opt2" if mip == 0 else None,
                                tc.create_string_field_class(),
                            ),
                        )
                    ),
                ),
                (
                    "variant-with-unsigned-integer-selector-field-selector",
                    variant_with_unsigned_selector_field_selector,
                ),
                (
                    "variant-with-unsigned-integer-selector-field",
                    (
                        tc.create_variant_field_class(
                            selector_fc=variant_with_unsigned_selector_field_selector,
                            options=variant_with_unsigned_selector_field_options,
                        )
                        if mip == 0
                        else (
                            tc.create_variant_field_class_with_unsigned_integer_selector_field(
                                selector_field_location=tc.create_field_location(
                                    bt2.FieldLocationScope.EVENT_PAYLOAD,
                                    [
                                        "variant-with-unsigned-integer-selector-field-selector"
                                    ],
                                ),
                                options=variant_with_unsigned_selector_field_options,
                            )
                        )
                    ),
                ),
                (
                    "variant-with-signed-integer-selector-field-selector",
                    variant_with_signed_selector_field_selector,
                ),
                (
                    "variant-with-signed-integer-selector-field",
                    (
                        tc.create_variant_field_class(
                            selector_fc=variant_with_signed_selector_field_selector,
                            options=variant_with_signed_selector_field_options,
                        )
                        if mip == 0
                        else (
                            tc.create_variant_field_class_with_signed_integer_selector_field(
                                selector_field_location=tc.create_field_location(
                                    bt2.FieldLocationScope.EVENT_PAYLOAD,
                                    [
                                        "variant-with-signed-integer-selector-field-selector"
                                    ],
                                ),
                                options=variant_with_signed_selector_field_options,
                            )
                        )
                    ),
                ),
            )
        )

        if mip > 0:
            payload += (
                (
                    "static-blob",
                    tc.create_static_blob_field_class(
                        10, media_type="application/x-gameboy-rom"
                    ),
                ),
                (
                    "dynamic-blob-without-length-field",
                    tc.create_dynamic_blob_field_class(
                        media_type="application/x-shockwave-flash"
                    ),
                ),
                (
                    "dynamic-blob-with-length-field-length",
                    tc.create_unsigned_integer_field_class(8),
                ),
                (
                    "dynamic-blob-with-length-field",
                    tc.create_dynamic_blob_field_class(
                        length_field_location=tc.create_field_location(
                            bt2.FieldLocationScope.EVENT_PAYLOAD,
                            ["dynamic-blob-with-length-field-length"],
                        ),
                        media_type="application/vnd.wordperfect",
                    ),
                ),
            )

        self._add_output_port(
            "out",
            tc.create_stream_class().create_event_class(payload_field_class=payload),
        )
