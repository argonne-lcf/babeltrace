# SPDX-FileCopyrightText: 2024 EfficiOS Inc.
#
# SPDX-License-Identifier: MIT

include $(top_srcdir)/src/Makefile.common.inc

SUBDIRS = \
	utils \
	lib \
	bitfield \
	ctf-writer \
	plugins \
	param-validation

AM_CPPFLAGS += -I$(top_srcdir)/tests/utils

COMMON_TEST_LDADD = \
	$(top_builddir)/tests/utils/tap/libtap.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/logging/liblogging.la

# Directories added to EXTRA_DIST will be recursively copied to the distribution.
EXTRA_DIST = $(srcdir)/data \
	     bindings/python/bt2/.coveragerc

dist_check_SCRIPTS = \
	bindings/python/bt2/test-python-bt2.sh \
	bindings/python/bt2/test_clock_class.py \
	bindings/python/bt2/test_component_class.py \
	bindings/python/bt2/test_component.py \
	bindings/python/bt2/test_connection.py \
	bindings/python/bt2/test_event_class.py \
	bindings/python/bt2/test_event.py \
	bindings/python/bt2/test_field_class.py \
	bindings/python/bt2/test_field.py \
	bindings/python/bt2/test_graph.py \
	bindings/python/bt2/test_integer_range_set.py \
	bindings/python/bt2/test_interrupter.py \
	bindings/python/bt2/test_message_iterator.py \
	bindings/python/bt2/test_message.py \
	bindings/python/bt2/test_mip1.py \
	bindings/python/bt2/test_package.py \
	bindings/python/bt2/test_packet.py \
	bindings/python/bt2/test_plugin.py \
	bindings/python/bt2/test_port.py \
	bindings/python/bt2/test_query_executor.py \
	bindings/python/bt2/test_stream_class.py \
	bindings/python/bt2/test_stream.py \
	bindings/python/bt2/test_trace_collection_message_iterator.py \
	bindings/python/bt2/test_trace.py \
	bindings/python/bt2/test_value.py \
	bindings/python/bt2/utils.py \
	cli/convert/test-auto-source-discovery-grouping.sh \
	cli/convert/test-auto-source-discovery-params.sh \
	cli/convert/test-auto-source-discovery-log-level.sh \
	cli/convert/test-convert-args.sh \
	cli/list-plugins/test-list-plugins.sh \
	cli/params/test-params.sh \
	cli/query/test-query.sh \
	cli/test-exit-status.sh \
	cli/test-help.sh \
	cli/test-intersection.sh \
	cli/test-output-ctf-metadata.sh \
	cli/test-output-path-ctf-non-lttng-trace.sh \
	cli/test-packet-seq-num.sh \
	cli/test-trace-copy.sh \
	cli/test-trace-read.sh \
	cli/test-trimmer.sh \
	plugins/sink.text.details/succeed/test-succeed.sh \
	plugins/sink.text.pretty/test-enum.sh \
	plugins/sink.text.pretty/test_pretty.py \
	plugins/sink.text.pretty/test-pretty.sh \
	plugins/sink.text.pretty/test-pretty-python.sh \
	plugins/src.ctf.lttng-live/test-live.sh \
	plugins/src.ctf.lttng-live/test-query.sh \
	plugins/src.ctf.lttng-live/test_query.py \
	python-plugin-provider/bt_plugin_test_python_plugin_provider.py \
	python-plugin-provider/test-python-plugin-provider.sh \
	python-plugin-provider/test_python_plugin_provider.py

noinst_PROGRAMS =

TESTS_BINDINGS =

if ENABLE_PYTHON_BINDINGS
TESTS_BINDINGS += bindings/python/bt2/test-python-bt2.sh
endif

TESTS_CLI = \
	cli/convert/test-convert-args.sh \
	cli/test-help.sh \
	cli/test-intersection.sh \
	cli/test-output-ctf-metadata.sh \
	cli/test-output-path-ctf-non-lttng-trace.sh \
	cli/test-packet-seq-num.sh \
	cli/test-trace-copy.sh \
	cli/test-trace-read.sh \
	cli/test-trimmer.sh

noinst_PROGRAMS += \
	cpp-common/test-c-string-view

cpp_common_test_c_string_view_SOURCES = \
	cpp-common/test-c-string-view.cpp

cpp_common_test_c_string_view_LDADD = \
	$(COMMON_TEST_LDADD)

noinst_PROGRAMS += \
	cpp-common/test-uuid

cpp_common_test_uuid_SOURCES = \
	cpp-common/test-uuid.cpp

cpp_common_test_uuid_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la

noinst_PROGRAMS += \
	cpp-common/test-unicode-conv

cpp_common_test_unicode_conv_SOURCES = \
	cpp-common/test-unicode-conv.cpp

cpp_common_test_unicode_conv_LDADD = \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la \
	$(top_builddir)/src/cpp-common/libcpp-common.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(COMMON_TEST_LDADD)

TESTS_CPP_COMMON = \
	cpp-common/test-c-string-view \
	cpp-common/test-uuid \
	cpp-common/test-unicode-conv

TESTS_LIB = \
	lib/test-bt-uuid \
	lib/test-bt-values \
	lib/test-fields.sh \
	lib/test-graph-topo \
	lib/test-mip \
	lib/test-remove-destruction-listener-in-destruction-listener \
	lib/test-simple-sink \
	lib/test-trace-ir-ref

TESTS_BITFIELD = \
	bitfield/test-bitfield

TESTS_CTF_WRITER = \
	ctf-writer/test-ctf-writer.sh

if !ENABLE_BUILT_IN_PLUGINS
TESTS_LIB += lib/test-plugins.sh
endif

# plugins/flt.utils.muxer

noinst_PROGRAMS += plugins/flt.utils.muxer/test-clock-compatibility

plugins_flt_utils_muxer_test_clock_compatibility_SOURCES = \
	plugins/flt.utils.muxer/test-clock-compatibility.cpp

plugins_flt_utils_muxer_test_clock_compatibility_LDADD = \
	$(COMMON_TEST_LDADD) \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la

dist_check_SCRIPTS += plugins/flt.utils.muxer/test-clock-compatibility.sh

if ENABLE_BUILT_IN_PLUGINS
plugins_flt_utils_muxer_test_clock_compatibility_LDFLAGS = $(call pluginarchive,utils)
plugins_flt_utils_muxer_test_clock_compatibility_LDADD += \
	$(top_builddir)/src/plugins/common/param-validation/libparam-validation.la
endif # ENABLE_BUILT_IN_PLUGINS

TESTS_PLUGINS = \
	plugins/src.ctf.fs/fail/test-fail.sh \
	plugins/src.ctf.fs/succeed/test-succeed.sh \
	plugins/src.ctf.fs/test-deterministic-ordering.sh \
	plugins/sink.ctf.fs/succeed/test-succeed.sh \
	plugins/sink.text.details/succeed/test-succeed.sh \
	plugins/flt.utils.muxer/test-clock-compatibility.sh \
	plugins/sink.text.pretty/test-pretty.sh

if !ENABLE_BUILT_IN_PLUGINS
if ENABLE_PYTHON_BINDINGS
TESTS_PLUGINS += plugins/src.ctf.fs/query/test-query-support-info.sh
TESTS_PLUGINS += plugins/src.ctf.fs/query/test-query-trace-info.sh
TESTS_PLUGINS += plugins/src.ctf.fs/query/test-query-metadata-info.sh
TESTS_PLUGINS += plugins/src.ctf.fs/query/test-query-metadata-info-py.sh
TESTS_PLUGINS += plugins/src.ctf.lttng-live/test-query.sh
TESTS_PLUGINS += plugins/sink.ctf.fs/test-assume-single-trace.sh
TESTS_PLUGINS += plugins/sink.ctf.fs/test-stream-names.sh
endif
endif

if ENABLE_DEBUG_INFO
TESTS_PLUGINS += \
	plugins/flt.lttng-utils.debug-info/test-dwarf-i386-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-dwarf-powerpc-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-dwarf-powerpc64le-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-dwarf-x86-64-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-bin-info-i386-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-bin-info-powerpc-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-bin-info-powerpc64le-linux-gnu.sh \
	plugins/flt.lttng-utils.debug-info/test-bin-info-x86-64-linux-gnu.sh
endif

if ENABLE_PYTHON_PLUGINS
if ENABLE_PYTHON_BINDINGS
TESTS_CLI += \
	cli/convert/test-auto-source-discovery-grouping.sh \
	cli/convert/test-auto-source-discovery-log-level.sh \
	cli/convert/test-auto-source-discovery-params.sh \
	cli/list-plugins/test-list-plugins.sh \
	cli/params/test-params.sh \
	cli/query/test-query.sh \
	cli/test-exit-status.sh

TESTS_PLUGINS += plugins/flt.utils.trimmer/test-trimming.sh \
	plugins/flt.utils.muxer/succeed/test-succeed.sh \
	plugins/sink.text.pretty/test-enum.sh \
	plugins/src.ctf.fs/field/test-field.sh
endif
endif

if HAVE_PYTHON
TESTS_PLUGINS += plugins/src.ctf.lttng-live/test-live.sh

if DEV_MODE
TESTS_LIB += lib/conds/test-conds.sh
endif
endif

TESTS_PYTHON_PLUGIN_PROVIDER =

if ENABLE_PYTHON_PLUGINS
TESTS_PYTHON_PLUGIN_PROVIDER += python-plugin-provider/test-python-plugin-provider.sh
TESTS_PLUGINS += plugins/sink.text.pretty/test-pretty-python.sh
if ENABLE_DEBUG_INFO
TESTS_PLUGINS += \
	plugins/flt.lttng-utils.debug-info/test-succeed.sh
endif
endif

TESTS_PARAM_VALIDATION = \
	param-validation/test-param-validation

LOG_DRIVER_FLAGS = --merge --comments
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' \
	     BT_TESTS_SRCDIR='$(abs_top_srcdir)/tests' \
	     BT_TESTS_BUILDDIR='$(abs_top_builddir)/tests' \
	     $(SHELL) $(srcdir)/utils/tap-driver.sh

TESTS_NO_BITFIELD = \
	$(TESTS_BINDINGS) \
	$(TESTS_CLI) \
	$(TESTS_CPP_COMMON) \
	$(TESTS_CTF_WRITER) \
	$(TESTS_LIB) \
	$(TESTS_PARAM_VALIDATION) \
	$(TESTS_PLUGINS) \
	$(TESTS_PYTHON_PLUGIN_PROVIDER)

TESTS = $(TESTS_NO_BITFIELD) $(TESTS_BITFIELD)

define check_target
check-$(1):
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$2" check
endef

$(eval $(call check_target,bindings,$(TESTS_BINDINGS)))
$(eval $(call check_target,bitfield,$(TESTS_BITFIELD)))
$(eval $(call check_target,cli,$(TESTS_CLI)))
$(eval $(call check_target,cpp-common,$(TESTS_CPP_COMMON)))
$(eval $(call check_target,ctf-writer,$(TESTS_CTF_WRITER)))
$(eval $(call check_target,lib,$(TESTS_LIB)))
$(eval $(call check_target,plugins,$(TESTS_PLUGINS)))
$(eval $(call check_target,python-plugin-provider,$(TESTS_PYTHON_PLUGIN_PROVIDER)))

check-no-bitfield:
	$(MAKE) $(AM_MAKEFLAGS) TESTS="$(TESTS_NO_BITFIELD)" check
