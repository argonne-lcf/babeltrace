# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import enum
import collections.abc

from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import field_path as bt2_field_path
from bt2 import typing_mod
from bt2 import user_attributes as bt2_user_attrs
from bt2 import integer_range_set as bt2_integer_range_set

typing = typing_mod._typing_mod


def _obj_type_from_field_class_ptr_template(type_map, ptr):
    return type_map[native_bt.field_class_get_type(ptr)]


def _obj_type_from_field_class_ptr(ptr) -> "_FieldClass":
    return _obj_type_from_field_class_ptr_template(_FIELD_CLASS_TYPE_TO_OBJ, ptr)


def _create_field_class_from_ptr(ptr):
    return _obj_type_from_field_class_ptr(ptr)._create_from_ptr(ptr)


def _create_field_class_from_ptr_and_get_ref_template(type_map, ptr):
    return _obj_type_from_field_class_ptr_template(
        type_map, ptr
    )._create_from_ptr_and_get_ref(ptr)


def _create_field_class_from_ptr_and_get_ref(ptr):
    return _create_field_class_from_ptr_and_get_ref_template(
        _FIELD_CLASS_TYPE_TO_OBJ, ptr
    )


def _create_field_class_from_const_ptr_and_get_ref(ptr):
    return _create_field_class_from_ptr_and_get_ref_template(
        _FIELD_CLASS_TYPE_TO_CONST_OBJ, ptr
    )


class IntegerDisplayBase(enum.IntEnum):
    BINARY = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY
    OCTAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL
    DECIMAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
    HEXADECIMAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL


class _FieldClassConst(bt2_object._SharedObject, bt2_user_attrs._WithUserAttrsConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.field_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.field_class_put_ref(ptr)

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.field_class_borrow_user_attributes_const(ptr)


class _FieldClass(bt2_user_attrs._WithUserAttrs, _FieldClassConst):
    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.field_class_borrow_user_attributes(ptr)

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.field_class_set_user_attributes(obj_ptr, value_ptr)


class _BoolFieldClassConst(_FieldClassConst):
    _NAME = "Const boolean"


class _BoolFieldClass(_BoolFieldClassConst, _FieldClass):
    _NAME = "Boolean"


class _BitArrayFieldClassConst(_FieldClassConst):
    _NAME = "Const bit array"

    @property
    def length(self) -> int:
        return native_bt.field_class_bit_array_get_length(self._ptr)


class _BitArrayFieldClass(_BitArrayFieldClassConst, _FieldClass):
    _NAME = "Bit array"


class _IntegerFieldClassConst(_FieldClassConst):
    @property
    def field_value_range(self) -> int:
        return native_bt.field_class_integer_get_field_value_range(self._ptr)

    @property
    def preferred_display_base(self) -> IntegerDisplayBase:
        return IntegerDisplayBase(
            native_bt.field_class_integer_get_preferred_display_base(self._ptr)
        )


class _IntegerFieldClass(_FieldClass, _IntegerFieldClassConst):
    def _set_field_value_range(self, size):
        if size < 1 or size > 64:
            raise ValueError("Value is outside valid range [1, 64] ({})".format(size))

        native_bt.field_class_integer_set_field_value_range(self._ptr, size)

    def _set_preferred_display_base(self, base: IntegerDisplayBase):
        if isinstance(base, int):
            base = IntegerDisplayBase(base)

        bt2_utils._check_type(base, IntegerDisplayBase)
        native_bt.field_class_integer_set_preferred_display_base(self._ptr, base.value)


class _UnsignedIntegerFieldClassConst(_IntegerFieldClassConst, _FieldClassConst):
    _NAME = "Const unsigned integer"


class _UnsignedIntegerFieldClass(
    _UnsignedIntegerFieldClassConst, _IntegerFieldClass, _FieldClass
):
    _NAME = "Unsigned integer"


class _SignedIntegerFieldClassConst(_IntegerFieldClassConst, _FieldClassConst):
    _NAME = "Const signed integer"


class _SignedIntegerFieldClass(
    _SignedIntegerFieldClassConst, _IntegerFieldClass, _FieldClass
):
    _NAME = "Signed integer"


class _RealFieldClassConst(_FieldClassConst):
    pass


class _SinglePrecisionRealFieldClassConst(_RealFieldClassConst):
    _NAME = "Const single-precision real"


class _DoublePrecisionRealFieldClassConst(_RealFieldClassConst):
    _NAME = "Const double-precision real"


class _RealFieldClass(_FieldClass, _RealFieldClassConst):
    pass


class _SinglePrecisionRealFieldClass(_RealFieldClass):
    _NAME = "Single-precision real"


class _DoublePrecisionRealFieldClass(_RealFieldClass):
    _NAME = "Double-precision real"


# an enumeration field class mapping does not have a reference count, so
# we copy the properties here to avoid eventual memory access errors.
class _EnumerationFieldClassMapping:
    def __init__(self, mapping_ptr):
        self._label = native_bt.field_class_enumeration_mapping_get_label(
            self._as_enumeration_field_class_mapping_ptr(mapping_ptr)
        )
        self._ranges = self._range_set_pycls._create_from_ptr_and_get_ref(
            self._mapping_borrow_ranges_ptr(mapping_ptr)
        )

    @property
    def label(self) -> str:
        return self._label

    @property
    def ranges(self) -> bt2_integer_range_set._IntegerRangeSetConst:
        return self._ranges


class _UnsignedEnumerationFieldClassMappingConst(_EnumerationFieldClassMapping):
    _range_set_pycls = bt2_integer_range_set._UnsignedIntegerRangeSetConst
    _as_enumeration_field_class_mapping_ptr = staticmethod(
        native_bt.field_class_enumeration_unsigned_mapping_as_mapping_const
    )
    _mapping_borrow_ranges_ptr = staticmethod(
        native_bt.field_class_enumeration_unsigned_mapping_borrow_ranges_const
    )


class _SignedEnumerationFieldClassMappingConst(_EnumerationFieldClassMapping):
    _range_set_pycls = bt2_integer_range_set._SignedIntegerRangeSetConst
    _as_enumeration_field_class_mapping_ptr = staticmethod(
        native_bt.field_class_enumeration_signed_mapping_as_mapping_const
    )
    _mapping_borrow_ranges_ptr = staticmethod(
        native_bt.field_class_enumeration_signed_mapping_borrow_ranges_const
    )


class _EnumerationFieldClassConst(
    _IntegerFieldClassConst, typing.Mapping[str, _EnumerationFieldClassMapping]
):
    def __len__(self) -> int:
        return native_bt.field_class_enumeration_get_mapping_count(self._ptr)

    def mappings_for_value(
        self, value: int
    ) -> typing.List[_EnumerationFieldClassMapping]:
        self._check_int_type(value)

        status, labels = self._get_mapping_labels_for_value(self._ptr, value)
        bt2_utils._handle_func_status(
            status, "cannot get mapping labels for value {}".format(value)
        )
        return [self[label] for label in labels]

    def __iter__(self) -> typing.Iterator[str]:
        for idx in range(len(self)):
            yield self._mapping_pycls(
                self._borrow_mapping_ptr_by_index(self._ptr, idx)
            ).label

    def __getitem__(self, label: str) -> _EnumerationFieldClassMapping:
        bt2_utils._check_str(label)
        mapping_ptr = self._borrow_mapping_ptr_by_label(self._ptr, label)

        if mapping_ptr is None:
            raise KeyError(label)

        return self._mapping_pycls(mapping_ptr)


class _EnumerationFieldClass(_EnumerationFieldClassConst, _IntegerFieldClass):
    def _add_mapping(
        self, label: str, ranges: bt2_integer_range_set._IntegerRangeSetConst
    ):
        bt2_utils._check_str(label)
        bt2_utils._check_type(ranges, self._range_set_pycls)

        if label in self:
            raise ValueError("duplicate mapping label '{}'".format(label))

        bt2_utils._handle_func_status(
            self._add_mapping_ptr(self._ptr, label, ranges._ptr),
            "cannot add mapping to enumeration field class object",
        )

    def _iadd(
        self,
        mappings: typing.Iterable[
            typing.Tuple[str, bt2_integer_range_set._IntegerRangeSetConst]
        ],
    ) -> "_EnumerationFieldClass":
        for label, ranges in mappings:
            self.add_mapping(label, ranges)

        return self


class _UnsignedEnumerationFieldClassConst(
    _EnumerationFieldClassConst, _UnsignedIntegerFieldClassConst
):
    _NAME = "Const unsigned enumeration"
    _borrow_mapping_ptr_by_label = staticmethod(
        native_bt.field_class_enumeration_unsigned_borrow_mapping_by_label_const
    )
    _borrow_mapping_ptr_by_index = staticmethod(
        native_bt.field_class_enumeration_unsigned_borrow_mapping_by_index_const
    )
    _mapping_pycls = property(lambda _: _UnsignedEnumerationFieldClassMappingConst)
    _get_mapping_labels_for_value = staticmethod(
        native_bt.field_class_enumeration_unsigned_get_mapping_labels_for_value
    )
    _check_int_type = staticmethod(bt2_utils._check_uint64)


class _UnsignedEnumerationFieldClass(
    _UnsignedEnumerationFieldClassConst,
    _EnumerationFieldClass,
    _UnsignedIntegerFieldClass,
):
    _NAME = "Unsigned enumeration"
    _range_set_pycls = bt2_integer_range_set.UnsignedIntegerRangeSet
    _add_mapping_ptr = staticmethod(
        native_bt.field_class_enumeration_unsigned_add_mapping
    )

    def add_mapping(
        self, label: str, ranges: bt2_integer_range_set._UnsignedIntegerRangeSetConst
    ):
        self._add_mapping(label, ranges)

    def __iadd__(
        self,
        mappings: typing.Iterable[
            typing.Tuple[str, bt2_integer_range_set._UnsignedIntegerRangeSetConst]
        ],
    ) -> "_UnsignedEnumerationFieldClass":
        for label, ranges in mappings:
            self.add_mapping(label, ranges)

        return self


class _SignedEnumerationFieldClassConst(
    _EnumerationFieldClassConst, _SignedIntegerFieldClassConst
):
    _NAME = "Const signed enumeration"
    _borrow_mapping_ptr_by_label = staticmethod(
        native_bt.field_class_enumeration_signed_borrow_mapping_by_label_const
    )
    _borrow_mapping_ptr_by_index = staticmethod(
        native_bt.field_class_enumeration_signed_borrow_mapping_by_index_const
    )
    _mapping_pycls = property(lambda _: _SignedEnumerationFieldClassMappingConst)
    _get_mapping_labels_for_value = staticmethod(
        native_bt.field_class_enumeration_signed_get_mapping_labels_for_value
    )
    _check_int_type = staticmethod(bt2_utils._check_int64)


class _SignedEnumerationFieldClass(
    _SignedEnumerationFieldClassConst, _EnumerationFieldClass, _SignedIntegerFieldClass
):
    _NAME = "Signed enumeration"
    _range_set_pycls = bt2_integer_range_set.SignedIntegerRangeSet
    _add_mapping_ptr = staticmethod(
        native_bt.field_class_enumeration_signed_add_mapping
    )

    def add_mapping(
        self, label: str, ranges: bt2_integer_range_set._SignedIntegerRangeSetConst
    ):
        self._add_mapping(label, ranges)

    def __iadd__(
        self,
        mappings: typing.Iterable[
            typing.Tuple[str, bt2_integer_range_set._SignedIntegerRangeSetConst]
        ],
    ) -> "_SignedEnumerationFieldClass":
        for label, ranges in mappings:
            self.add_mapping(label, ranges)

        return self


class _StringFieldClassConst(_FieldClassConst):
    _NAME = "Const string"


class _StringFieldClass(_StringFieldClassConst, _FieldClass):
    _NAME = "String"


class _StructureFieldClassMemberConst(bt2_user_attrs._WithUserAttrsConst):
    @property
    def _ptr(self):
        return self._member_ptr

    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_const_ptr_and_get_ref
    )
    _borrow_field_class_ptr = staticmethod(
        native_bt.field_class_structure_member_borrow_field_class_const
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.field_class_structure_member_borrow_user_attributes_const(ptr)

    def __init__(self, owning_struct_fc, member_ptr):
        # this field class owns the member; keeping it here maintains
        # the member alive as members are not shared objects
        self._owning_struct_fc = owning_struct_fc
        self._member_ptr = member_ptr

    @property
    def name(self) -> str:
        return native_bt.field_class_structure_member_get_name(self._member_ptr)

    @property
    def field_class(self) -> _FieldClassConst:
        return self._create_field_class_from_ptr_and_get_ref(
            self._borrow_field_class_ptr(self._member_ptr)
        )


class _StructureFieldClassMember(
    bt2_user_attrs._WithUserAttrs, _StructureFieldClassMemberConst
):
    _borrow_field_class_ptr = staticmethod(
        native_bt.field_class_structure_member_borrow_field_class
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.field_class_structure_member_borrow_user_attributes(ptr)

    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_ptr_and_get_ref
    )

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.field_class_structure_member_set_user_attributes(obj_ptr, value_ptr)


class _StructureFieldClassConst(_FieldClassConst, collections.abc.Mapping):
    _NAME = "Const structure"
    _borrow_member_ptr_by_index = staticmethod(
        native_bt.field_class_structure_borrow_member_by_index_const
    )
    _borrow_member_ptr_by_name = staticmethod(
        native_bt.field_class_structure_borrow_member_by_name_const
    )
    _structure_member_field_class_pycls = property(
        lambda _: _StructureFieldClassMemberConst
    )

    def __len__(self) -> int:
        return native_bt.field_class_structure_get_member_count(self._ptr)

    def __getitem__(self, key: str) -> _StructureFieldClassMemberConst:
        if not isinstance(key, str):
            raise TypeError(
                "key must be a 'str' object, got '{}'".format(key.__class__.__name__)
            )

        member_ptr = self._borrow_member_ptr_by_name(self._ptr, key)

        if member_ptr is None:
            raise KeyError(key)

        return self._structure_member_field_class_pycls(self, member_ptr)

    def __iter__(self) -> typing.Iterator[str]:
        for idx in range(len(self)):
            yield native_bt.field_class_structure_member_get_name(
                self._borrow_member_ptr_by_index(self._ptr, idx)
            )

    def member_at_index(self, index: int) -> _StructureFieldClassMemberConst:
        bt2_utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        return self._structure_member_field_class_pycls(
            self, self._borrow_member_ptr_by_index(self._ptr, index)
        )


class _StructureFieldClass(_StructureFieldClassConst, _FieldClass):
    _NAME = "Structure"
    _borrow_member_by_index = staticmethod(
        native_bt.field_class_structure_borrow_member_by_index
    )
    _borrow_member_ptr_by_name = staticmethod(
        native_bt.field_class_structure_borrow_member_by_name
    )
    _structure_member_field_class_pycls = property(lambda _: _StructureFieldClassMember)

    def append_member(
        self,
        name: str,
        field_class: _FieldClass,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ):
        bt2_utils._check_str(name)
        bt2_utils._check_type(field_class, _FieldClass)

        if name in self:
            raise ValueError("duplicate member name '{}'".format(name))

        # check now that user attributes are valid
        user_attributes_value = bt2_value.create_value(user_attributes)

        bt2_utils._handle_func_status(
            native_bt.field_class_structure_append_member(
                self._ptr, name, field_class._ptr
            ),
            "cannot append member to structure field class object",
        )

        if user_attributes is not None:
            self[name]._set_user_attributes(user_attributes_value)

    def __iadd__(
        self,
        members: typing.Iterable[
            typing.Union[
                typing.Tuple[str, _FieldClass],
                typing.Tuple[
                    str,
                    _FieldClass,
                    typing.Optional[bt2_value._MapValueConst],
                ],
            ]
        ],
    ) -> "_StructureFieldClass":
        for member in members:
            if len(member) == 2:
                name, field_class = member
                user_attributes = None
            elif len(member) == 3:
                name, field_class, user_attributes = member
            else:
                raise ValueError("invalid member tuple length: {}".format(len(member)))

            self.append_member(name, field_class, user_attributes)

        return self


class _OptionFieldClassConst(_FieldClassConst):
    _NAME = "Const option"
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_const_ptr_and_get_ref
    )
    _borrow_field_class_ptr = staticmethod(
        native_bt.field_class_option_borrow_field_class_const
    )

    @property
    def field_class(self) -> _FieldClassConst:
        elem_fc_ptr = self._borrow_field_class_ptr(self._ptr)
        return self._create_field_class_from_ptr_and_get_ref(elem_fc_ptr)


class _OptionFieldClassWithSelectorFieldConst(_OptionFieldClassConst):
    _NAME = "Const option (with selector)"

    @property
    def selector_field_path(self) -> typing.Optional[bt2_field_path._FieldPathConst]:
        ptr = native_bt.field_class_option_with_selector_field_borrow_selector_field_path_const(
            self._ptr
        )
        if ptr is None:
            return

        return bt2_field_path._FieldPathConst._create_from_ptr_and_get_ref(ptr)


_OptionWithSelectorFieldClassConst = _OptionFieldClassWithSelectorFieldConst


class _OptionFieldClassWithBoolSelectorFieldConst(
    _OptionFieldClassWithSelectorFieldConst
):
    _NAME = "Const option (with boolean selector)"

    @property
    def selector_is_reversed(self) -> bool:
        return bool(
            native_bt.field_class_option_with_selector_field_bool_selector_is_reversed(
                self._ptr
            )
        )


_OptionWithBoolSelectorFieldClassConst = _OptionFieldClassWithBoolSelectorFieldConst


class _OptionFieldClassWithIntegerSelectorFieldConst(
    _OptionFieldClassWithSelectorFieldConst
):
    _NAME = "Const option (with integer selector)"

    @property
    def ranges(self) -> bt2_integer_range_set._IntegerRangeSetConst:
        return self._range_set_pycls._create_from_ptr_and_get_ref(
            self._borrow_selector_ranges_ptr(self._ptr)
        )


_OptionWithIntegerSelectorFieldClassConst = (
    _OptionFieldClassWithIntegerSelectorFieldConst
)


class _OptionFieldClassWithUnsignedIntegerSelectorFieldConst(
    _OptionFieldClassWithIntegerSelectorFieldConst
):
    _NAME = "Const option (with unsigned integer selector)"
    _range_set_pycls = bt2_integer_range_set._UnsignedIntegerRangeSetConst
    _borrow_selector_ranges_ptr = staticmethod(
        native_bt.field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const
    )


_OptionWithUnsignedIntegerSelectorFieldClassConst = (
    _OptionFieldClassWithIntegerSelectorFieldConst
)


class _OptionFieldClassWithSignedIntegerSelectorFieldConst(
    _OptionFieldClassWithIntegerSelectorFieldConst
):
    _NAME = "Const option (with signed integer selector)"
    _range_set_pycls = bt2_integer_range_set._SignedIntegerRangeSetConst
    _borrow_selector_ranges_ptr = staticmethod(
        native_bt.field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const
    )


_OptionWithSignedIntegerSelectorFieldClassConst = (
    _OptionFieldClassWithSignedIntegerSelectorFieldConst
)


class _OptionFieldClass(_OptionFieldClassConst, _FieldClass):
    _NAME = "Option"
    _borrow_field_class_ptr = staticmethod(
        native_bt.field_class_option_borrow_field_class
    )
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_ptr_and_get_ref
    )


class _OptionFieldClassWithSelectorField(
    _OptionFieldClassWithSelectorFieldConst, _OptionFieldClass
):
    _NAME = "Option (with selector)"


_OptionWithSelectorFieldClass = _OptionFieldClassWithSelectorField


class _OptionFieldClassWithBoolSelectorField(
    _OptionFieldClassWithBoolSelectorFieldConst, _OptionFieldClassWithSelectorField
):
    _NAME = "Option (with boolean selector)"

    def _set_selector_is_reversed(self, selector_is_reversed):
        bt2_utils._check_bool(selector_is_reversed)
        native_bt.field_class_option_with_selector_field_bool_set_selector_is_reversed(
            self._ptr, selector_is_reversed
        )


_OptionWithBoolSelectorFieldClass = _OptionFieldClassWithBoolSelectorField


class _OptionFieldClassWithIntegerSelectorField(
    _OptionFieldClassWithIntegerSelectorFieldConst, _OptionFieldClassWithSelectorField
):
    _NAME = "Option (with integer selector)"


_OptionWithIntegerSelectorFieldClass = _OptionFieldClassWithIntegerSelectorField


class _OptionFieldClassWithUnsignedIntegerSelectorField(
    _OptionFieldClassWithUnsignedIntegerSelectorFieldConst,
    _OptionFieldClassWithIntegerSelectorField,
):
    _NAME = "Option (with unsigned integer selector)"


_OptionWithUnsignedIntegerSelectorFieldClass = (
    _OptionFieldClassWithUnsignedIntegerSelectorField
)


class _OptionFieldClassWithSignedIntegerSelectorField(
    _OptionFieldClassWithSignedIntegerSelectorFieldConst,
    _OptionFieldClassWithIntegerSelectorField,
):
    _NAME = "Option (with signed integer selector)"


_OptionWithSignedIntegerSelectorFieldClass = (
    _OptionFieldClassWithSignedIntegerSelectorField
)


class _VariantFieldClassOptionConst(bt2_user_attrs._WithUserAttrsConst):
    @property
    def _ptr(self):
        return self._opt_ptr

    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_const_ptr_and_get_ref
    )
    _borrow_field_class_ptr = staticmethod(
        native_bt.field_class_variant_option_borrow_field_class_const
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.field_class_variant_option_borrow_user_attributes_const(ptr)

    def __init__(self, owning_var_fc, option_ptr):
        # this field class owns the option; keeping it here maintains
        # the option alive as options are not shared objects
        self._owning_var_fc = owning_var_fc
        self._opt_ptr = option_ptr

    @property
    def name(self) -> str:
        return native_bt.field_class_variant_option_get_name(self._opt_ptr)

    @property
    def field_class(self) -> _FieldClassConst:
        return self._create_field_class_from_ptr_and_get_ref(
            self._borrow_field_class_ptr(self._opt_ptr)
        )


class _VariantFieldClassOption(
    bt2_user_attrs._WithUserAttrs, _VariantFieldClassOptionConst
):
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_ptr_and_get_ref
    )
    _borrow_field_class_ptr = staticmethod(
        native_bt.field_class_variant_option_borrow_field_class
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.field_class_variant_option_borrow_user_attributes(ptr)

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.field_class_variant_option_set_user_attributes(obj_ptr, value_ptr)


class _VariantFieldClassWithIntegerSelectorFieldOptionConst(
    _VariantFieldClassOptionConst
):
    def __init__(self, owning_var_fc, spec_opt_ptr):
        self._spec_ptr = spec_opt_ptr
        super().__init__(owning_var_fc, self._as_option_ptr(spec_opt_ptr))

    @property
    def ranges(self) -> bt2_integer_range_set._IntegerRangeSetConst:
        return self._range_set_pycls._create_from_ptr_and_get_ref(
            self._borrow_ranges_ptr(self._spec_ptr)
        )


class _VariantFieldClassWithIntegerSelectorFieldOption(
    _VariantFieldClassWithIntegerSelectorFieldOptionConst, _VariantFieldClassOption
):
    pass


class _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst(
    _VariantFieldClassWithIntegerSelectorFieldOptionConst
):
    _as_option_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_signed_option_as_option_const
    )
    _borrow_ranges_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const
    )
    _range_set_pycls = bt2_integer_range_set._SignedIntegerRangeSetConst


class _VariantFieldClassWithSignedIntegerSelectorFieldOption(
    _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst,
    _VariantFieldClassWithIntegerSelectorFieldOption,
):
    pass


class _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst(
    _VariantFieldClassWithIntegerSelectorFieldOptionConst
):
    _as_option_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_unsigned_option_as_option_const
    )
    _borrow_ranges_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const
    )
    _range_set_pycls = bt2_integer_range_set._UnsignedIntegerRangeSetConst


class _VariantFieldClassWithUnsignedIntegerSelectorFieldOption(
    _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst,
    _VariantFieldClassWithIntegerSelectorFieldOption,
):
    pass


class _VariantFieldClassConst(_FieldClassConst, collections.abc.Mapping):
    _NAME = "Const variant"
    _borrow_option_ptr_by_name = staticmethod(
        native_bt.field_class_variant_borrow_option_by_name_const
    )
    _borrow_option_ptr_by_index = staticmethod(
        native_bt.field_class_variant_borrow_option_by_index_const
    )

    @staticmethod
    def _as_option_ptr(opt_ptr):
        return opt_ptr

    def __len__(self) -> int:
        return native_bt.field_class_variant_get_option_count(self._ptr)

    def _getitem(self, key: str):
        if not isinstance(key, str):
            raise TypeError(
                "key must be a 'str' object, got '{}'".format(key.__class__.__name__)
            )

        opt_ptr = self._borrow_option_ptr_by_name(self._ptr, key)

        if opt_ptr is None:
            raise KeyError(key)

        return opt_ptr

    def __getitem__(self, key: str) -> _VariantFieldClassOptionConst:
        return _VariantFieldClassOptionConst(self, self._getitem(key))

    def __iter__(self) -> typing.Iterator[str]:
        for idx in range(len(self)):
            yield native_bt.field_class_variant_option_get_name(
                self._as_option_ptr(self._borrow_option_ptr_by_index(self._ptr, idx))
            )

    def _option_at_index(self, index: int):
        bt2_utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        return self._borrow_option_ptr_by_index(self._ptr, index)

    def option_at_index(self, index: int) -> _VariantFieldClassOptionConst:
        return _VariantFieldClassOptionConst(
            self, self._borrow_option_ptr_by_index(self._ptr, index)
        )


class _VariantFieldClass(_VariantFieldClassConst, _FieldClass, collections.abc.Mapping):
    _NAME = "Variant"
    _borrow_option_ptr_by_name = staticmethod(
        native_bt.field_class_variant_borrow_option_by_name
    )
    _borrow_option_ptr_by_index = staticmethod(
        native_bt.field_class_variant_borrow_option_by_index
    )
    _variant_option_pycls = _VariantFieldClassOption

    def __getitem__(self, key: str) -> _VariantFieldClassOption:
        return _VariantFieldClassOption(self, self._getitem(key))

    def option_at_index(self, index: int) -> _VariantFieldClassOption:
        return _VariantFieldClassOption(self, self._option_at_index(index))


class _VariantFieldClassWithoutSelectorFieldConst(_VariantFieldClassConst):
    _NAME = "Const variant (without selector)"


_VariantFieldClassWithoutSelectorConst = _VariantFieldClassWithoutSelectorFieldConst


class _VariantFieldClassWithoutSelectorField(
    _VariantFieldClassWithoutSelectorFieldConst, _VariantFieldClass
):
    _NAME = "Variant (without selector)"

    def append_option(
        self,
        name: str,
        field_class: _FieldClass,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ):
        bt2_utils._check_str(name)
        bt2_utils._check_type(field_class, _FieldClass)

        if name in self:
            raise ValueError("duplicate option name '{}'".format(name))

        # check now that user attributes are valid
        user_attributes_value = bt2_value.create_value(user_attributes)

        bt2_utils._handle_func_status(
            native_bt.field_class_variant_without_selector_append_option(
                self._ptr, name, field_class._ptr
            ),
            "cannot append option to variant field class object",
        )

        if user_attributes is not None:
            self[name]._set_user_attributes(user_attributes_value)

    def __iadd__(
        self,
        options: typing.Iterable[
            typing.Union[
                typing.Tuple[str, _FieldClass],
                typing.Tuple[
                    str, _FieldClass, typing.Optional[bt2_value._MapValueConst]
                ],
            ]
        ],
    ) -> "_VariantFieldClassWithoutSelectorField":
        for option in options:
            if len(option) == 2:
                name, field_class = option
                user_attributes = None
            elif len(option) == 3:
                name, field_class, user_attributes = option
            else:
                raise ValueError("invalid option tuple length: {}".format(len(option)))

            self.append_option(name, field_class, user_attributes)

        return self


_VariantFieldClassWithoutSelector = _VariantFieldClassWithoutSelectorField


class _VariantFieldClassWithIntegerSelectorFieldConst(_VariantFieldClassConst):
    _NAME = "Const variant (with selector)"

    def __getitem__(
        self, key: str
    ) -> _VariantFieldClassWithIntegerSelectorFieldOptionConst:
        return _VariantFieldClassWithIntegerSelectorFieldOptionConst(
            self, self._getitem(key)
        )

    def option_at_index(
        self, index: int
    ) -> _VariantFieldClassWithIntegerSelectorFieldOptionConst:
        return _VariantFieldClassWithIntegerSelectorFieldOptionConst(
            self, self._option_at_index(index)
        )

    @property
    def selector_field_path(self) -> typing.Optional[bt2_field_path._FieldPathConst]:
        ptr = native_bt.field_class_variant_with_selector_field_borrow_selector_field_path_const(
            self._ptr
        )

        if ptr is None:
            return

        return bt2_field_path._FieldPathConst._create_from_ptr_and_get_ref(ptr)


_VariantFieldClassWithIntegerSelectorConst = (
    _VariantFieldClassWithIntegerSelectorFieldConst
)


class _VariantFieldClassWithIntegerSelectorField(
    _VariantFieldClassWithIntegerSelectorFieldConst, _VariantFieldClass
):
    _NAME = "Variant (with selector)"

    def __getitem__(self, key: str) -> _VariantFieldClassWithIntegerSelectorFieldOption:
        return _VariantFieldClassWithIntegerSelectorFieldOption(
            self, self._getitem(key)
        )

    def option_at_index(
        self, index: int
    ) -> _VariantFieldClassWithIntegerSelectorFieldOption:
        return _VariantFieldClassWithIntegerSelectorFieldOption(
            self, self._option_at_index(index)
        )

    def _append_option(
        self,
        name: str,
        field_class: _FieldClass,
        ranges: bt2_integer_range_set._IntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ):
        bt2_utils._check_str(name)
        bt2_utils._check_type(field_class, _FieldClass)
        bt2_utils._check_type(ranges, self._variant_option_pycls._range_set_pycls)

        if name in self:
            raise ValueError("duplicate option name '{}'".format(name))

        if len(ranges) == 0:
            raise ValueError("range set is empty")

        # check now that user attributes are valid
        user_attributes_value = bt2_value.create_value(user_attributes)

        # TODO: check overlaps (precondition of self._append_option())

        bt2_utils._handle_func_status(
            self._append_option_ptr(self._ptr, name, field_class._ptr, ranges._ptr),
            "cannot append option to variant field class object",
        )

        if user_attributes is not None:
            self[name]._set_user_attributes(user_attributes_value)

    def _iadd(
        self,
        options: typing.Iterable[
            typing.Union[
                typing.Tuple[
                    str,
                    _FieldClass,
                    bt2_integer_range_set._IntegerRangeSetConst,
                ],
                typing.Tuple[
                    str,
                    _FieldClass,
                    bt2_integer_range_set._IntegerRangeSetConst,
                    typing.Optional[bt2_value._MapValueConst],
                ],
            ],
        ],
    ):
        for option in options:
            if len(option) == 3:
                name, field_class, ranges = option
                user_attributes = None
            elif len(option) == 4:
                name, field_class, ranges, user_attributes = option
            else:
                raise ValueError("invalid option tuple length: {}".format(len(option)))

            self.append_option(name, field_class, ranges, user_attributes)

        return self


_VariantFieldClassWithIntegerSelector = _VariantFieldClassWithIntegerSelectorField


class _VariantFieldClassWithUnsignedIntegerSelectorFieldConst(
    _VariantFieldClassWithIntegerSelectorFieldConst
):
    _NAME = "Const variant (with unsigned integer selector)"
    _borrow_option_ptr_by_name = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const
    )
    _borrow_option_ptr_by_index = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const
    )
    _as_option_ptr = staticmethod(
        _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst._as_option_ptr
    )

    def __getitem__(
        self, key: str
    ) -> _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst:
        return _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst(
            self, self._getitem(key)
        )

    def option_at_index(
        self, index: int
    ) -> _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst:
        return _VariantFieldClassWithUnsignedIntegerSelectorFieldOptionConst(
            self, self._option_at_index(index)
        )


_VariantFieldClassWithUnsignedIntegerSelectorConst = (
    _VariantFieldClassWithUnsignedIntegerSelectorFieldConst
)


class _VariantFieldClassWithUnsignedIntegerSelectorField(
    _VariantFieldClassWithUnsignedIntegerSelectorFieldConst,
    _VariantFieldClassWithIntegerSelectorField,
):
    _NAME = "Variant (with unsigned integer selector)"
    _variant_option_pycls = _VariantFieldClassWithUnsignedIntegerSelectorFieldOption
    _as_option_ptr = staticmethod(_variant_option_pycls._as_option_ptr)
    _append_option_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_unsigned_append_option
    )

    def __getitem__(
        self, key: str
    ) -> _VariantFieldClassWithUnsignedIntegerSelectorFieldOption:
        return _VariantFieldClassWithUnsignedIntegerSelectorFieldOption(
            self, self._getitem(key)
        )

    def option_at_index(
        self, index: int
    ) -> _VariantFieldClassWithUnsignedIntegerSelectorFieldOption:
        return _VariantFieldClassWithUnsignedIntegerSelectorFieldOption(
            self, self._option_at_index(index)
        )

    def append_option(
        self,
        name: typing.Optional[str],
        field_class: _FieldClass,
        ranges: bt2_integer_range_set._UnsignedIntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ):
        return self._append_option(name, field_class, ranges, user_attributes)

    def __iadd__(
        self,
        options: typing.Iterable[
            typing.Union[
                typing.Tuple[
                    str,
                    _FieldClass,
                    bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                ],
                typing.Tuple[
                    str,
                    _FieldClass,
                    bt2_integer_range_set._UnsignedIntegerRangeSetConst,
                    typing.Optional[bt2_value._MapValueConst],
                ],
            ],
        ],
    ):
        return self._iadd(options)


_VariantFieldClassWithUnsignedIntegerSelector = (
    _VariantFieldClassWithUnsignedIntegerSelectorField
)


class _VariantFieldClassWithSignedIntegerSelectorFieldConst(
    _VariantFieldClassWithIntegerSelectorFieldConst
):
    _NAME = "Const variant (with signed integer selector)"
    _borrow_option_ptr_by_name = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const
    )
    _borrow_option_ptr_by_index = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const
    )
    _as_option_ptr = staticmethod(
        _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst._as_option_ptr
    )

    def __getitem__(
        self, key: str
    ) -> _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst:
        return _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst(
            self, self._getitem(key)
        )

    def option_at_index(
        self, index: int
    ) -> _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst:
        return _VariantFieldClassWithSignedIntegerSelectorFieldOptionConst(
            self, self._option_at_index(index)
        )


_VariantFieldClassWithSignedIntegerSelectorConst = (
    _VariantFieldClassWithSignedIntegerSelectorFieldConst
)


class _VariantFieldClassWithSignedIntegerSelectorField(
    _VariantFieldClassWithSignedIntegerSelectorFieldConst,
    _VariantFieldClassWithIntegerSelectorField,
):
    _NAME = "Variant (with signed integer selector)"
    _variant_option_pycls = _VariantFieldClassWithSignedIntegerSelectorFieldOption
    _as_option_ptr = staticmethod(_variant_option_pycls._as_option_ptr)
    _append_option_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_field_integer_signed_append_option
    )

    def __getitem__(
        self, key: str
    ) -> _VariantFieldClassWithSignedIntegerSelectorFieldOption:
        return _VariantFieldClassWithSignedIntegerSelectorFieldOption(
            self, self._getitem(key)
        )

    def option_at_index(
        self, index: int
    ) -> _VariantFieldClassWithSignedIntegerSelectorFieldOption:
        return _VariantFieldClassWithSignedIntegerSelectorFieldOption(
            self, self._option_at_index(index)
        )

    def append_option(
        self,
        name: typing.Optional[str],
        field_class: _FieldClass,
        ranges: bt2_integer_range_set._SignedIntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._ConvertibleToMapValue] = None,
    ):
        return self._append_option(name, field_class, ranges, user_attributes)

    def __iadd__(
        self,
        options: typing.Iterable[
            typing.Union[
                typing.Tuple[
                    str,
                    _FieldClass,
                    bt2_integer_range_set._SignedIntegerRangeSetConst,
                ],
                typing.Tuple[
                    str,
                    _FieldClass,
                    bt2_integer_range_set._SignedIntegerRangeSetConst,
                    typing.Optional[bt2_value._MapValueConst],
                ],
            ],
        ],
    ):
        return self._iadd(options)


_VariantFieldClassWithSignedIntegerSelector = (
    _VariantFieldClassWithSignedIntegerSelectorField
)


class _ArrayFieldClassConst(_FieldClassConst):
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_const_ptr_and_get_ref
    )
    _borrow_element_field_class = staticmethod(
        native_bt.field_class_array_borrow_element_field_class_const
    )

    @property
    def element_field_class(self) -> _FieldClassConst:
        elem_fc_ptr = self._borrow_element_field_class(self._ptr)
        return self._create_field_class_from_ptr_and_get_ref(elem_fc_ptr)


class _ArrayFieldClass(_ArrayFieldClassConst, _FieldClass):
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        _create_field_class_from_ptr_and_get_ref
    )
    _borrow_element_field_class = staticmethod(
        native_bt.field_class_array_borrow_element_field_class
    )


class _StaticArrayFieldClassConst(_ArrayFieldClassConst):
    _NAME = "Const static array"

    @property
    def length(self) -> int:
        return native_bt.field_class_array_static_get_length(self._ptr)


class _StaticArrayFieldClass(_StaticArrayFieldClassConst, _ArrayFieldClass):
    _NAME = "Static array"


class _DynamicArrayFieldClassConst(_ArrayFieldClassConst):
    _NAME = "Const dynamic array"


class _DynamicArrayFieldClassWithLengthFieldConst(_DynamicArrayFieldClassConst):
    _NAME = "Const dynamic array (with length field)"

    @property
    def length_field_path(self) -> typing.Optional[bt2_field_path._FieldPathConst]:
        ptr = native_bt.field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
            self._ptr
        )
        if ptr is None:
            return

        return bt2_field_path._FieldPathConst._create_from_ptr_and_get_ref(ptr)


_DynamicArrayWithLengthFieldFieldClassConst = (
    _DynamicArrayFieldClassWithLengthFieldConst
)


class _DynamicArrayFieldClass(_DynamicArrayFieldClassConst, _ArrayFieldClass):
    _NAME = "Dynamic array"


class _DynamicArrayFieldClassWithLengthField(
    _DynamicArrayFieldClassWithLengthFieldConst, _DynamicArrayFieldClass
):
    _NAME = "Dynamic array (with length field)"


_DynamicArrayWithLengthFieldFieldClass = _DynamicArrayFieldClassWithLengthField

_FIELD_CLASS_TYPE_TO_CONST_OBJ = {
    native_bt.FIELD_CLASS_TYPE_BOOL: _BoolFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_BIT_ARRAY: _BitArrayFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL: _SinglePrecisionRealFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL: _DoublePrecisionRealFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD: _DynamicArrayFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD: _DynamicArrayFieldClassWithLengthFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD: _OptionFieldClassConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD: _OptionFieldClassWithBoolSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldClassWithUnsignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldClassWithSignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD: _VariantFieldClassWithoutSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldClassWithUnsignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldClassWithSignedIntegerSelectorFieldConst,
}

_FIELD_CLASS_TYPE_TO_OBJ = {
    native_bt.FIELD_CLASS_TYPE_BOOL: _BoolFieldClass,
    native_bt.FIELD_CLASS_TYPE_BIT_ARRAY: _BitArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL: _SinglePrecisionRealFieldClass,
    native_bt.FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL: _DoublePrecisionRealFieldClass,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureFieldClass,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD: _DynamicArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD: _DynamicArrayFieldClassWithLengthField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD: _OptionFieldClass,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD: _OptionFieldClassWithBoolSelectorField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldClassWithUnsignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldClassWithSignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD: _VariantFieldClassWithoutSelectorField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldClassWithUnsignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldClassWithSignedIntegerSelectorField,
}
