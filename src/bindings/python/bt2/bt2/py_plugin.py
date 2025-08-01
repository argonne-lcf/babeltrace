# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import sys

from bt2 import utils as bt2_utils
from bt2 import component as bt2_component
from bt2 import typing_mod

typing = typing_mod._typing_mod

# Python plugin path to `_PluginInfo` (cache)
_plugin_infos = {}


def plugin_component_class(component_class: typing.Type[bt2_component._UserComponent]):
    if not issubclass(component_class, bt2_component._UserComponent):
        raise TypeError("component class is not a subclass of a user component class")

    component_class._bt_plugin_component_class = None
    return component_class


def register_plugin(
    module_name: str,
    name: str,
    description: typing.Optional[str] = None,
    author: typing.Optional[str] = None,
    license: typing.Optional[str] = None,
    version: typing.Union[
        typing.Tuple[int, int, int], typing.Tuple[int, int, int, str], None
    ] = None,
):
    if module_name not in sys.modules:
        raise RuntimeError(
            "cannot find module '{}' in loaded modules".format(module_name)
        )

    bt2_utils._check_str(name)

    if description is not None:
        bt2_utils._check_str(description)

    if author is not None:
        bt2_utils._check_str(author)

    if license is not None:
        bt2_utils._check_str(license)

    if version is not None:
        if not _validate_version(version):
            raise ValueError(
                "wrong version: expecting a tuple: (major, minor, patch) or (major, minor, patch, extra)"
            )

    sys.modules[module_name]._bt_plugin_info = _PluginInfo(
        name, description, author, license, version
    )


def _validate_version(version):
    if version is None:
        return True

    if not isinstance(version, tuple):
        return False

    if len(version) < 3 or len(version) > 4:
        return False

    if not isinstance(version[0], int):
        return False

    if not isinstance(version[1], int):
        return False

    if not isinstance(version[2], int):
        return False

    if len(version) == 4:
        if not isinstance(version[3], str):
            return False

    return True


class _PluginInfo:
    def __init__(self, name, description, author, license, version):
        self.name = name
        self.description = description
        self.author = author
        self.license = license
        self.version = version
        self.comp_class_addrs = None


# called by the BT plugin system
def _try_load_plugin_module(path):
    if path in _plugin_infos:
        # do not load module and create plugin info twice for this path
        return _plugin_infos[path]

    import hashlib
    import inspect
    import importlib.machinery

    if path is None:
        raise TypeError("missing path")

    # In order to load the module uniquely from its path, even from
    # different files which have the same basename, we hash the path
    # and prefix with `bt_plugin_`. This is its key in sys.modules.
    h = hashlib.sha256()
    h.update(path.encode())
    module_name = "bt_plugin_{}".format(h.hexdigest())
    assert module_name not in sys.modules

    # try loading the module: any raised exception is caught by the caller
    if sys.version_info < (3, 5):
        mod = importlib.machinery.SourceFileLoader(module_name, path).load_module()
    else:
        import importlib.util

        loader = importlib.machinery.SourceFileLoader(module_name, path)
        spec = importlib.util.spec_from_file_location(module_name, path, loader=loader)
        mod = importlib.util.module_from_spec(spec)
        sys.modules[mod.__name__] = mod
        loader.exec_module(mod)

    # we have the module: look for its plugin info first
    if not hasattr(mod, "_bt_plugin_info"):
        raise RuntimeError("missing '_bt_plugin_info' module attribute")

    plugin_info = mod._bt_plugin_info

    # search for user component classes
    def is_user_comp_class(obj):
        if not inspect.isclass(obj):
            return False

        if not hasattr(obj, "_bt_plugin_component_class"):
            return False

        return True

    comp_class_entries = inspect.getmembers(mod, is_user_comp_class)
    plugin_info.comp_class_addrs = [entry[1].addr for entry in comp_class_entries]
    _plugin_infos[path] = plugin_info
    return plugin_info
