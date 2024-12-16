# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2024 EfficiOS, Inc.
#


# Decorator for unittest.TestCast sub-classes to run tests once for
# each existing MIP versions.
#
# Replaces all test_* methods with a test_*_mip_0 and a test_*_mip_1 variant.
#
# For instance, it transforms this:
#
#   @test_all_mip_versions
#   class MyTestCase(unittest.TestCase):
#       test_something(self):
#           pass
#
# into:
#
#   class MyTestcase(unittest.TestCase):
#       test_something_mip_0(self):
#           pass
#
#       test_something_mip_1(self):
#           pass
#
# The test methods are wrapped such that the self._mip_version attribute is
# set to either 0 or 1 during the call to each method.
def test_all_mip_versions(cls: type):
    for attr_name, attr_value in list(cls.__dict__.items()):
        if not attr_name.startswith("test_") or not callable(attr_value):
            continue

        for mip_version in 0, 1:
            # Callable that wraps and replaces test methods in order to
            # temporarily set the _mip_version attribute on the TestCase class.
            def set_mip_version_wrapper_method(self, mip_version, test_method):
                assert not hasattr(self, "_mip_version")
                self._mip_version = mip_version

                try:
                    return test_method(self)
                finally:
                    assert hasattr(self, "_mip_version")
                    del self._mip_version

            def wrap_method(wrapper_method, mip_version, original_method):
                return lambda self: wrapper_method(self, mip_version, original_method)

            setattr(
                cls,
                "{}_mip_{}".format(attr_name, mip_version),
                wrap_method(set_mip_version_wrapper_method, mip_version, attr_value),
            )

        delattr(cls, attr_name)

    return cls
