# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
# Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>

import uuid as uuidp
import functools
import collections.abc

from bt2 import error as bt2_error
from bt2 import trace as bt2_trace
from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import clock_class as bt2_clock_class
from bt2 import field_class as bt2_field_class
from bt2 import stream_class as bt2_stream_class
from bt2 import user_attributes as bt2_user_attrs
from bt2 import integer_range_set as bt2_integer_range_set

typing = bt2_utils._typing_mod


def _trace_class_destruction_listener_from_native(
    user_listener, handle, trace_class_ptr
):
    user_listener(_TraceClassConst._create_from_ptr_and_get_ref(trace_class_ptr))
    handle._invalidate()


class _TraceClassConst(
    bt2_object._SharedObject,
    bt2_user_attrs._WithUserAttrsConst,
    collections.abc.Mapping,
):
    @staticmethod
    def _get_ref(ptr):
        native_bt.trace_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.trace_class_put_ref(ptr)

    _borrow_stream_class_ptr_by_index = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_index_const
    )
    _borrow_stream_class_ptr_by_id = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_id_const
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.trace_class_borrow_user_attributes_const(ptr)

    _stream_class_pycls = bt2_stream_class._StreamClassConst

    # Number of stream classes in this trace class.

    def __len__(self):
        return native_bt.trace_class_get_stream_class_count(self._ptr)

    # Get a stream class by stream id.

    def __getitem__(self, key):
        bt2_utils._check_uint64(key)

        sc_ptr = self._borrow_stream_class_ptr_by_id(self._ptr, key)
        if sc_ptr is None:
            raise KeyError(key)

        return self._stream_class_pycls._create_from_ptr_and_get_ref(sc_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            yield native_bt.stream_class_get_id(
                self._borrow_stream_class_ptr_by_index(self._ptr, idx)
            )

    @property
    def assigns_automatic_stream_class_id(self):
        return native_bt.trace_class_assigns_automatic_stream_class_id(self._ptr)

    # Add a listener to be called when the trace class is destroyed.

    def add_destruction_listener(self, listener):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        handle = bt2_utils._ListenerHandle(self.addr)

        status, listener_id = native_bt.bt2_trace_class_add_destruction_listener(
            self._ptr,
            functools.partial(
                _trace_class_destruction_listener_from_native, listener, handle
            ),
        )
        bt2_utils._handle_func_status(
            status, "cannot add destruction listener to trace class object"
        )

        handle._set_listener_id(listener_id)

        return handle

    def remove_destruction_listener(self, listener_handle):
        bt2_utils._check_type(listener_handle, bt2_utils._ListenerHandle)

        if listener_handle._addr != self.addr:
            raise ValueError(
                "This trace class destruction listener does not match the trace class object."
            )

        if listener_handle._listener_id is None:
            raise ValueError(
                "This trace class destruction listener was already removed."
            )

        bt2_utils._handle_func_status(
            native_bt.trace_class_remove_destruction_listener(
                self._ptr, listener_handle._listener_id
            )
        )
        listener_handle._invalidate()


class _TraceClass(bt2_user_attrs._WithUserAttrs, _TraceClassConst):
    _borrow_stream_class_ptr_by_index = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_index
    )
    _borrow_stream_class_ptr_by_id = staticmethod(
        native_bt.trace_class_borrow_stream_class_by_id
    )

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.trace_class_borrow_user_attributes(ptr)

    _stream_class_pycls = bt2_stream_class._StreamClass

    # Instantiate a trace of this class.

    def __call__(
        self,
        name: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
        uuid: typing.Optional[uuidp.UUID] = None,
        environment: typing.Optional[
            typing.Mapping[str, typing.Union[str, int]]
        ] = None,
    ) -> bt2_trace._Trace:
        trace_ptr = native_bt.trace_create(self._ptr)

        if trace_ptr is None:
            raise bt2_error._MemoryError("cannot create trace class object")

        trace = bt2_trace._Trace._create_from_ptr(trace_ptr)

        if name is not None:
            trace._set_name(name)

        if user_attributes is not None:
            trace._set_user_attributes(user_attributes)

        if uuid is not None:
            trace._set_uuid(uuid)

        if environment is not None:
            for key, value in environment.items():
                trace.environment[key] = value

        return trace

    def create_stream_class(
        self,
        id: typing.Optional[int] = None,
        name: typing.Optional[str] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
        packet_context_field_class: typing.Optional[
            bt2_field_class._StructureFieldClass
        ] = None,
        event_common_context_field_class: typing.Optional[
            bt2_field_class._StructureFieldClass
        ] = None,
        default_clock_class: typing.Optional[bt2_clock_class._ClockClass] = None,
        assigns_automatic_event_class_id: bool = True,
        assigns_automatic_stream_id: bool = True,
        supports_packets: bool = False,
        packets_have_beginning_default_clock_snapshot: bool = False,
        packets_have_end_default_clock_snapshot: bool = False,
        supports_discarded_events: bool = False,
        discarded_events_have_default_clock_snapshots: bool = False,
        supports_discarded_packets: bool = False,
        discarded_packets_have_default_clock_snapshots: bool = False,
    ) -> bt2_stream_class._StreamClass:
        # Validate parameters before we create the object.
        bt2_stream_class._StreamClass._validate_create_params(
            name,
            user_attributes,
            packet_context_field_class,
            event_common_context_field_class,
            default_clock_class,
            assigns_automatic_event_class_id,
            assigns_automatic_stream_id,
            supports_packets,
            packets_have_beginning_default_clock_snapshot,
            packets_have_end_default_clock_snapshot,
            supports_discarded_events,
            discarded_events_have_default_clock_snapshots,
            supports_discarded_packets,
            discarded_packets_have_default_clock_snapshots,
        )

        if self.assigns_automatic_stream_class_id:
            if id is not None:
                raise ValueError(
                    "id provided, but trace class assigns automatic stream class ids"
                )

            sc_ptr = native_bt.stream_class_create(self._ptr)
        else:
            if id is None:
                raise ValueError(
                    "id not provided, but trace class does not assign automatic stream class ids"
                )

            bt2_utils._check_uint64(id)
            sc_ptr = native_bt.stream_class_create_with_id(self._ptr, id)

        sc = bt2_stream_class._StreamClass._create_from_ptr(sc_ptr)

        if name is not None:
            sc._set_name(name)

        if user_attributes is not None:
            sc._set_user_attributes(user_attributes)

        if event_common_context_field_class is not None:
            sc._set_event_common_context_field_class(event_common_context_field_class)

        if default_clock_class is not None:
            sc._set_default_clock_class(default_clock_class)

        # call after `sc._default_clock_class` because, if
        # `packets_have_beginning_default_clock_snapshot` or
        # `packets_have_end_default_clock_snapshot` is true, then this
        # stream class needs a default clock class already.
        sc._set_supports_packets(
            supports_packets,
            packets_have_beginning_default_clock_snapshot,
            packets_have_end_default_clock_snapshot,
        )

        # call after sc._set_supports_packets() because, if
        # `packet_context_field_class` is not `None`, then this stream
        # class needs to support packets already.
        if packet_context_field_class is not None:
            sc._set_packet_context_field_class(packet_context_field_class)

        sc._set_assigns_automatic_event_class_id(assigns_automatic_event_class_id)
        sc._set_assigns_automatic_stream_id(assigns_automatic_stream_id)
        sc._set_supports_discarded_events(
            supports_discarded_events, discarded_events_have_default_clock_snapshots
        )
        sc._set_supports_discarded_packets(
            supports_discarded_packets, discarded_packets_have_default_clock_snapshots
        )
        return sc

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.trace_class_set_user_attributes(obj_ptr, value_ptr)

    def _set_assigns_automatic_stream_class_id(self, auto_id):
        bt2_utils._check_bool(auto_id)
        return native_bt.trace_class_set_assigns_automatic_stream_class_id(
            self._ptr, auto_id
        )

    # Field class creation methods.

    @staticmethod
    def _check_and_wrap_field_class(
        ptr,
        type_name: str,
        user_attributes: typing.Optional[bt2_value._MapValueConst],
    ):
        if ptr is None:
            raise bt2_error._MemoryError(
                "cannot create {} field class".format(type_name)
            )

        fc = bt2_field_class._create_field_class_from_ptr(ptr)

        if user_attributes is not None:
            fc._set_user_attributes(user_attributes)

        return fc

    def create_bool_field_class(
        self, user_attributes: typing.Optional[bt2_value._MapValueConst] = None
    ) -> bt2_field_class._BoolFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_bool_create(self._ptr), "boolean", user_attributes
        )

    def create_bit_array_field_class(
        self,
        length: int,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._BitArrayFieldClass:
        bt2_utils._check_uint64(length)

        if length < 1 or length > 64:
            raise ValueError(
                "invalid length {}: expecting a value in the [1, 64] range".format(
                    length
                )
            )

        return self._check_and_wrap_field_class(
            native_bt.field_class_bit_array_create(self._ptr, length),
            "bit array",
            user_attributes,
        )

    def _create_integer_field_class(
        self,
        create_func,
        type_name,
        field_value_range,
        preferred_display_base,
        user_attributes,
    ):
        fc = self._check_and_wrap_field_class(
            create_func(self._ptr), type_name, user_attributes
        )

        if field_value_range is not None:
            fc._set_field_value_range(field_value_range)

        if preferred_display_base is not None:
            fc._set_preferred_display_base(preferred_display_base)

        return fc

    def create_signed_integer_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._SignedIntegerFieldClass:
        return self._create_integer_field_class(
            native_bt.field_class_integer_signed_create,
            "signed integer",
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_unsigned_integer_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._UnsignedIntegerFieldClass:
        return self._create_integer_field_class(
            native_bt.field_class_integer_unsigned_create,
            "unsigned integer",
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_signed_enumeration_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._SignedEnumerationFieldClass:
        return self._create_integer_field_class(
            native_bt.field_class_enumeration_signed_create,
            "signed enumeration",
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_unsigned_enumeration_field_class(
        self,
        field_value_range: typing.Optional[int] = None,
        preferred_display_base: typing.Optional[
            bt2_field_class.IntegerDisplayBase
        ] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._UnsignedEnumerationFieldClass:
        return self._create_integer_field_class(
            native_bt.field_class_enumeration_unsigned_create,
            "unsigned enumeration",
            field_value_range,
            preferred_display_base,
            user_attributes,
        )

    def create_single_precision_real_field_class(
        self, user_attributes: typing.Optional[bt2_value._MapValueConst] = None
    ) -> bt2_field_class._SinglePrecisionRealFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_real_single_precision_create(self._ptr),
            "single-precision real",
            user_attributes,
        )

    def create_double_precision_real_field_class(
        self, user_attributes: typing.Optional[bt2_value._MapValueConst] = None
    ) -> bt2_field_class._DoublePrecisionRealFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_real_double_precision_create(self._ptr),
            "double-precision real",
            user_attributes,
        )

    def create_structure_field_class(
        self, user_attributes: typing.Optional[bt2_value._MapValueConst] = None
    ) -> bt2_field_class._StructureFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_structure_create(self._ptr),
            "structure",
            user_attributes,
        )

    def create_string_field_class(
        self, user_attributes: typing.Optional[bt2_value._MapValueConst] = None
    ) -> bt2_field_class._StringFieldClass:
        return self._check_and_wrap_field_class(
            native_bt.field_class_string_create(self._ptr), "string", user_attributes
        )

    def create_static_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length: int,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._StaticArrayFieldClass:
        bt2_utils._check_type(elem_fc, bt2_field_class._FieldClass)
        bt2_utils._check_uint64(length)
        return self._check_and_wrap_field_class(
            native_bt.field_class_array_static_create(self._ptr, elem_fc._ptr, length),
            "static array",
            user_attributes,
        )

    def create_dynamic_array_field_class(
        self,
        elem_fc: bt2_field_class._FieldClass,
        length_fc: typing.Optional[bt2_field_class._FieldClass] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._DynamicArrayFieldClass:
        bt2_utils._check_type(elem_fc, bt2_field_class._FieldClass)
        length_fc_ptr = None

        if length_fc is not None:
            bt2_utils._check_type(length_fc, bt2_field_class._UnsignedIntegerFieldClass)
            length_fc_ptr = length_fc._ptr

        return self._check_and_wrap_field_class(
            native_bt.field_class_array_dynamic_create(
                self._ptr, elem_fc._ptr, length_fc_ptr
            ),
            "dynamic array",
            user_attributes,
        )

    def create_option_without_selector_field_class(
        self,
        content_fc: bt2_field_class._FieldClass,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._OptionFieldClass:
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)

        return self._check_and_wrap_field_class(
            native_bt.field_class_option_without_selector_create(
                self._ptr, content_fc._ptr
            ),
            "option",
            user_attributes,
        )

    def create_option_with_bool_selector_field_class(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_fc: bt2_field_class._FieldClass,
        selector_is_reversed: bool = False,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._OptionWithBoolSelectorFieldClass:
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        bt2_utils._check_bool(selector_is_reversed)
        bt2_utils._check_type(selector_fc, bt2_field_class._BoolFieldClass)
        fc = self._check_and_wrap_field_class(
            native_bt.field_class_option_with_selector_field_bool_create(
                self._ptr, content_fc._ptr, selector_fc._ptr
            ),
            "option",
            user_attributes,
        )
        fc._set_selector_is_reversed(selector_is_reversed)
        return fc

    def create_option_with_integer_selector_field_class(
        self,
        content_fc: bt2_field_class._FieldClass,
        selector_fc: bt2_field_class._FieldClass,
        ranges: bt2_integer_range_set._IntegerRangeSetConst,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._OptionWithIntegerSelectorFieldClass:
        bt2_utils._check_type(content_fc, bt2_field_class._FieldClass)
        bt2_utils._check_type(selector_fc, bt2_field_class._IntegerFieldClass)

        if len(ranges) == 0:
            raise ValueError("integer range set is empty")

        if isinstance(selector_fc, bt2_field_class._UnsignedIntegerFieldClass):
            bt2_utils._check_type(ranges, bt2_integer_range_set.UnsignedIntegerRangeSet)
            ptr = native_bt.field_class_option_with_selector_field_integer_unsigned_create(
                self._ptr, content_fc._ptr, selector_fc._ptr, ranges._ptr
            )
        else:
            bt2_utils._check_type(ranges, bt2_integer_range_set.SignedIntegerRangeSet)
            ptr = (
                native_bt.field_class_option_with_selector_field_integer_signed_create(
                    self._ptr, content_fc._ptr, selector_fc._ptr, ranges._ptr
                )
            )

        return self._check_and_wrap_field_class(ptr, "option", user_attributes)

    def create_variant_field_class(
        self,
        selector_fc: typing.Optional[bt2_field_class._FieldClass] = None,
        user_attributes: typing.Optional[bt2_value._MapValueConst] = None,
    ) -> bt2_field_class._VariantFieldClass:
        selector_fc_ptr = None

        if selector_fc is not None:
            bt2_utils._check_type(selector_fc, bt2_field_class._IntegerFieldClass)
            selector_fc_ptr = selector_fc._ptr

        return self._check_and_wrap_field_class(
            native_bt.field_class_variant_create(self._ptr, selector_fc_ptr),
            "variant",
            user_attributes,
        )
