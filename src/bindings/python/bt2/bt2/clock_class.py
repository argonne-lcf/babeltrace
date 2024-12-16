# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import uuid as uuidp

from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt, typing_mod
from bt2 import user_attributes as bt2_user_attrs

typing = typing_mod._typing_mod


class ClockOffset:
    def __init__(self, seconds: int = 0, cycles: int = 0):
        bt2_utils._check_int64(seconds)
        bt2_utils._check_int64(cycles)
        self._seconds = seconds
        self._cycles = cycles

    @property
    def seconds(self) -> int:
        return self._seconds

    @property
    def cycles(self) -> int:
        return self._cycles

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, self.__class__):
            # not comparing apples to apples
            return False

        return (self.seconds, self.cycles) == (other.seconds, other.cycles)


ClockClassOffset = ClockOffset


class _ClockClassConst(bt2_object._SharedObject, bt2_user_attrs._WithUserAttrsConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.clock_class_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.clock_class_put_ref(ptr)

    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.clock_class_borrow_user_attributes_const(ptr)

    @property
    def name(self) -> typing.Optional[str]:
        return native_bt.clock_class_get_name(self._ptr)

    @property
    def description(self) -> typing.Optional[str]:
        return native_bt.clock_class_get_description(self._ptr)

    @property
    def frequency(self) -> int:
        return native_bt.clock_class_get_frequency(self._ptr)

    @property
    def precision(self) -> int:
        return native_bt.clock_class_get_precision(self._ptr)

    @property
    def offset(self) -> ClockOffset:
        offset_s, offset_cycles = native_bt.clock_class_get_offset(self._ptr)
        return ClockOffset(offset_s, offset_cycles)

    @property
    def origin_is_unix_epoch(self) -> bool:
        return native_bt.clock_class_origin_is_unix_epoch(self._ptr)

    @property
    def uuid(self) -> typing.Optional[uuidp.UUID]:
        uuid_bytes = native_bt.clock_class_get_uuid(self._ptr)

        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    def cycles_to_ns_from_origin(self, cycles: int) -> int:
        bt2_utils._check_uint64(cycles)
        status, ns = native_bt.clock_class_cycles_to_ns_from_origin(self._ptr, cycles)
        bt2_utils._handle_func_status(
            status,
            "cannot convert clock value to nanoseconds from origin for given clock class",
        )
        return ns


class _ClockClass(bt2_user_attrs._WithUserAttrs, _ClockClassConst):
    @staticmethod
    def _borrow_user_attributes_ptr(ptr):
        return native_bt.clock_class_borrow_user_attributes(ptr)

    @staticmethod
    def _set_user_attributes_ptr(obj_ptr, value_ptr):
        native_bt.clock_class_set_user_attributes(obj_ptr, value_ptr)

    def _set_name(self, name):
        bt2_utils._check_str(name)
        bt2_utils._handle_func_status(
            native_bt.clock_class_set_name(self._ptr, name),
            "cannot set clock class object's name",
        )

    def _set_description(self, description):
        bt2_utils._check_str(description)
        bt2_utils._handle_func_status(
            native_bt.clock_class_set_description(self._ptr, description),
            "cannot set clock class object's description",
        )

    def _set_frequency(self, frequency):
        bt2_utils._check_uint64(frequency)
        native_bt.clock_class_set_frequency(self._ptr, frequency)

    def _set_precision(self, precision):
        bt2_utils._check_uint64(precision)
        native_bt.clock_class_set_precision(self._ptr, precision)

    def _set_offset(self, offset):
        bt2_utils._check_type(offset, ClockOffset)
        native_bt.clock_class_set_offset(self._ptr, offset.seconds, offset.cycles)

    def _set_origin_is_unix_epoch(self, origin_is_unix_epoch):
        bt2_utils._check_bool(origin_is_unix_epoch)
        native_bt.clock_class_set_origin_is_unix_epoch(
            self._ptr, int(origin_is_unix_epoch)
        )

    def _set_uuid(self, uuid):
        bt2_utils._check_type(uuid, uuidp.UUID)
        native_bt.clock_class_set_uuid(self._ptr, uuid.bytes)
