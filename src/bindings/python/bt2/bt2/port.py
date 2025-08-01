# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

from bt2 import object as bt2_object
from bt2 import native_bt, typing_mod

typing = typing_mod._typing_mod

if typing.TYPE_CHECKING:
    from bt2 import connection as bt2_connection


def _bt2_connection():
    from bt2 import connection as bt2_connection

    return bt2_connection


def _create_from_const_ptr_and_get_ref(ptr, port_type):
    cls = _PORT_TYPE_TO_PYCLS.get(port_type, None)

    if cls is None:
        raise TypeError("unknown port type: {}".format(port_type))

    return cls._create_from_ptr_and_get_ref(ptr)


def _create_self_from_ptr_and_get_ref(ptr, port_type):
    cls = _PORT_TYPE_TO_USER_PYCLS.get(port_type, None)

    if cls is None:
        raise TypeError("unknown port type: {}".format(port_type))

    return cls._create_from_ptr_and_get_ref(ptr)


class _PortConst(bt2_object._SharedObject):
    @classmethod
    def _get_ref(cls, ptr):
        return native_bt.port_get_ref(cls._as_port_ptr(ptr))

    @classmethod
    def _put_ref(cls, ptr):
        return native_bt.port_put_ref(cls._as_port_ptr(ptr))

    @property
    def name(self) -> str:
        return native_bt.port_get_name(self._as_port_ptr(self._ptr))

    @property
    def connection(self) -> typing.Optional["bt2_connection._ConnectionConst"]:
        conn_ptr = native_bt.port_borrow_connection_const(self._as_port_ptr(self._ptr))

        if conn_ptr is None:
            return

        return _bt2_connection()._ConnectionConst._create_from_ptr_and_get_ref(conn_ptr)

    @property
    def is_connected(self) -> bool:
        return self.connection is not None


class _InputPortConst(_PortConst):
    _as_port_ptr = staticmethod(native_bt.port_input_as_port_const)


class _OutputPortConst(_PortConst):
    _as_port_ptr = staticmethod(native_bt.port_output_as_port_const)


class _UserComponentPort(_PortConst):
    @classmethod
    def _as_port_ptr(cls, ptr):
        return native_bt.self_component_port_as_port(cls._as_self_port_ptr(ptr))

    @property
    def connection(self) -> typing.Optional["bt2_connection._ConnectionConst"]:
        conn_ptr = native_bt.port_borrow_connection_const(self._as_port_ptr(self._ptr))

        if conn_ptr is None:
            return

        return _bt2_connection()._ConnectionConst._create_from_ptr_and_get_ref(conn_ptr)

    @property
    def user_data(self) -> object:
        return native_bt.self_component_port_get_data(self._as_self_port_ptr(self._ptr))


class _UserComponentInputPort(_UserComponentPort, _InputPortConst):
    _as_self_port_ptr = staticmethod(
        native_bt.self_component_port_input_as_self_component_port
    )


class _UserComponentOutputPort(_UserComponentPort, _OutputPortConst):
    _as_self_port_ptr = staticmethod(
        native_bt.self_component_port_output_as_self_component_port
    )


_PORT_TYPE_TO_PYCLS = {
    native_bt.PORT_TYPE_INPUT: _InputPortConst,
    native_bt.PORT_TYPE_OUTPUT: _OutputPortConst,
}


_PORT_TYPE_TO_USER_PYCLS = {
    native_bt.PORT_TYPE_INPUT: _UserComponentInputPort,
    native_bt.PORT_TYPE_OUTPUT: _UserComponentOutputPort,
}
