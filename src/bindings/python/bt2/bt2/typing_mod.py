# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 EfficiOS Inc.

# The purpose of this import is to make the typing module easily accessible
# elsewhere, without having to do the try-except everywhere.
try:
    import typing as _typing_mod  # noqa: F401
except ImportError:
    from bt2 import local_typing as _typing_mod  # noqa: F401
