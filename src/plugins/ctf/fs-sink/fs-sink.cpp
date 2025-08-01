/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#include <glib.h>
#include <stdio.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/wrap.hpp"
#include "cpp-common/bt2c/logging.hpp"
#include "cpp-common/vendor/fmt/format.h"
#include "ctfser/ctfser.h"

#include "plugins/common/param-validation/param-validation.h"

#include "fs-sink-ctf-meta.hpp"
#include "fs-sink-stream.hpp"
#include "fs-sink-trace.hpp"
#include "fs-sink.hpp"
#include "translate-trace-ir-to-ctf-ir.hpp"

static const char * const in_port_name = "in";

static bt_component_class_initialize_method_status
ensure_output_dir_exists(struct fs_sink_comp *fs_sink)
{
    bt_component_class_initialize_method_status status =
        BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
    int ret;

    ret = g_mkdir_with_parents(fs_sink->output_dir_path->str, 0755);
    if (ret) {
        BT_CPPLOGE_ERRNO_APPEND_CAUSE_SPEC(
            fs_sink->logger, "Cannot create directories for output directory",
            ": output-dir-path=\"{}\"", fs_sink->output_dir_path->str);
        status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        goto end;
    }

end:
    return status;
}

constexpr const char *ctfVersionParamName = "ctf-version";

static bt_param_validation_map_value_entry_descr fs_sink_params_descr[] = {
    {"path", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
     bt_param_validation_value_descr::makeString()},
    {"assume-single-trace", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"ignore-discarded-events", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"ignore-discarded-packets", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"quiet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {ctfVersionParamName, BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

static int ctfVersionFromParams(const bt_value *params, const bt2c::Logger& logger)
{
    const auto wParams = bt2::wrap(params).asMap();

    if (wParams.hasEntry(ctfVersionParamName)) {
        const auto ctfVersionVal = wParams[ctfVersionParamName]->asString();

        if (ctfVersionVal.value() == "1" || ctfVersionVal.value() == "1.8") {
            return 1;
        } else if (ctfVersionVal.value() != "2" && ctfVersionVal.value() != "2.0") {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                logger,
                "Unexpected `{}` parameter value `{}`: expecting `1`, `1.8`, `2`, or `2.0`.",
                ctfVersionParamName, ctfVersionVal.value());
            return -1;
        }
    }

    /* Default */
    return 2;
}

static bt_component_class_initialize_method_status
configure_component(bt_self_component_sink *self_comp_sink, struct fs_sink_comp *fs_sink,
                    const bt_value *params)
{
    bt_component_class_initialize_method_status status;
    const bt_value *value;
    enum bt_param_validation_status validation_status;
    gchar *validation_error;

    validation_status =
        bt_param_validation_validate(params, fs_sink_params_descr, &validation_error);
    if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
        status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "{}", validation_error);
        goto end;
    } else if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
        status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
        goto end;
    }

    value = bt_value_map_borrow_entry_value_const(params, "path");
    g_string_assign(fs_sink->output_dir_path, bt_value_string_get(value));

    value = bt_value_map_borrow_entry_value_const(params, "assume-single-trace");
    if (value) {
        fs_sink->assume_single_trace = (bool) bt_value_bool_get(value);
    }

    value = bt_value_map_borrow_entry_value_const(params, "ignore-discarded-events");
    if (value) {
        fs_sink->ignore_discarded_events = (bool) bt_value_bool_get(value);
    }

    value = bt_value_map_borrow_entry_value_const(params, "ignore-discarded-packets");
    if (value) {
        fs_sink->ignore_discarded_packets = (bool) bt_value_bool_get(value);
    }

    value = bt_value_map_borrow_entry_value_const(params, "quiet");
    if (value) {
        fs_sink->quiet = (bool) bt_value_bool_get(value);
    }

    value = bt_value_map_borrow_entry_value_const(params, "ctf-version");
    if (value) {
        const auto ctfVersion = ctfVersionFromParams(params, fs_sink->logger);

        if (ctfVersion < 0) {
            status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
            goto end;
        }

        fs_sink->ctf_version = static_cast<unsigned int>(ctfVersion);
    }

    {
        const auto mipVersion = bt2::wrap(self_comp_sink).graphMipVersion();

        if ((fs_sink->ctf_version == 1 && mipVersion == 1) ||
            (fs_sink->ctf_version == 2 && mipVersion == 0)) {
            status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger, "Invalid CTF version ({}) and MIP version combination ({}).",
                fs_sink->ctf_version, mipVersion);
            goto end;
        }
    }

    status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;

end:
    g_free(validation_error);
    return status;
}

static void destroy_fs_sink_comp(struct fs_sink_comp *fs_sink)
{
    if (!fs_sink) {
        goto end;
    }

    if (fs_sink->output_dir_path) {
        g_string_free(fs_sink->output_dir_path, TRUE);
        fs_sink->output_dir_path = NULL;
    }

    if (fs_sink->traces) {
        g_hash_table_destroy(fs_sink->traces);
        fs_sink->traces = NULL;
    }

    BT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(fs_sink->upstream_iter);
    delete fs_sink;

end:
    return;
}

bt_component_class_initialize_method_status ctf_fs_sink_init(bt_self_component_sink *self_comp_sink,
                                                             bt_self_component_sink_configuration *,
                                                             const bt_value *params, void *)
{
    try {
        bt_component_class_initialize_method_status status;
        bt_self_component_add_port_status add_port_status;
        struct fs_sink_comp *fs_sink = NULL;

        fs_sink = new fs_sink_comp {bt2::wrap(self_comp_sink)};
        fs_sink->output_dir_path = g_string_new(NULL);
        status = configure_component(self_comp_sink, fs_sink, params);
        if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
            /* configure_component() logs errors */
            goto end;
        }

        if (fs_sink->assume_single_trace &&
            g_file_test(fs_sink->output_dir_path->str, G_FILE_TEST_EXISTS)) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger, "Single trace mode, but output path exists: output-path=\"{}\"",
                fs_sink->output_dir_path->str);
            status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
            goto end;
        }

        status = ensure_output_dir_exists(fs_sink);
        if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
            /* ensure_output_dir_exists() logs errors */
            goto end;
        }

        fs_sink->traces = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                                (GDestroyNotify) fs_sink_trace_destroy);
        if (!fs_sink->traces) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to allocate one GHashTable.");
            status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
            goto end;
        }

        add_port_status =
            bt_self_component_sink_add_input_port(self_comp_sink, in_port_name, NULL, NULL);
        if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
            status = (bt_component_class_initialize_method_status) add_port_status;
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to add input port.");
            goto end;
        }

        bt_self_component_set_data(bt_self_component_sink_as_self_component(self_comp_sink),
                                   fs_sink);

end:
        if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
            destroy_fs_sink_comp(fs_sink);
        }

        return status;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

static inline struct fs_sink_stream *borrow_stream(struct fs_sink_comp *fs_sink,
                                                   const bt_stream *ir_stream)
{
    const bt_trace *ir_trace = bt_stream_borrow_trace_const(ir_stream);
    struct fs_sink_trace *trace;
    struct fs_sink_stream *stream = NULL;

    trace = (fs_sink_trace *) g_hash_table_lookup(fs_sink->traces, ir_trace);
    if (G_UNLIKELY(!trace)) {
        if (fs_sink->assume_single_trace && g_hash_table_size(fs_sink->traces) > 0) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                         "Single trace mode, but getting more than one trace: "
                                         "stream-name=\"{}\"",
                                         bt2c::maybeNull(bt_stream_get_name(ir_stream)));
            goto end;
        }

        trace = fs_sink_trace_create(fs_sink, ir_trace);
        if (!trace) {
            goto end;
        }
    }

    stream = (fs_sink_stream *) g_hash_table_lookup(trace->streams, ir_stream);
    if (G_UNLIKELY(!stream)) {
        stream = fs_sink_stream_create(trace, ir_stream);
        if (!stream) {
            goto end;
        }
    }

end:
    return stream;
}

static inline bt_component_class_sink_consume_method_status
handle_event_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    try {
        int ret;
        bt_component_class_sink_consume_method_status status =
            BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
        const bt_event *ir_event = bt_message_event_borrow_event_const(msg);
        const bt_stream *ir_stream = bt_event_borrow_stream_const(ir_event);
        struct fs_sink_stream *stream;
        struct fs_sink_ctf_event_class *ec = NULL;
        const bt_clock_snapshot *cs = NULL;

        stream = borrow_stream(fs_sink, ir_stream);
        if (G_UNLIKELY(!stream)) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }

        ret = try_translate_event_class_trace_ir_to_ctf_ir(
            fs_sink, stream->sc, bt_event_borrow_class_const(ir_event), &ec);
        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                         "Failed to translate event class to CTF IR.");
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }

        BT_ASSERT_DBG(ec);

        if (stream->sc->default_clock_class) {
            cs = bt_message_event_borrow_default_clock_snapshot_const(msg);
        }

        /*
     * If this event's stream does not support packets, then we
     * lazily create artificial packets.
     *
     * The size of an artificial packet is arbitrarily at least
     * 4 MiB (it usually is greater because we close it when
     * comes the time to write a new event and the packet's content
     * size is >= 4 MiB), except the last one which can be smaller.
     */
        if (G_UNLIKELY(!stream->sc->has_packets)) {
            if (stream->packet_state.is_open &&
                bt_ctfser_get_offset_in_current_packet_bits(&stream->ctfser) / 8 >=
                    4 * 1024 * 1024) {
                /*
             * Stream's current packet is larger than 4 MiB:
             * close it. A new packet will be opened just
             * below.
             */
                ret = fs_sink_stream_close_packet(stream, NULL);
                if (ret) {
                    BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to close packet.");
                    status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
                    goto end;
                }
            }

            if (!stream->packet_state.is_open) {
                /* Stream's packet is not currently opened: open it */
                ret = fs_sink_stream_open_packet(stream, NULL, NULL);
                if (ret) {
                    BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to open packet.");
                    status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
                    goto end;
                }
            }
        }

        BT_ASSERT_DBG(stream->packet_state.is_open);
        ret = fs_sink_stream_write_event(stream, cs, ir_event, ec);
        if (G_UNLIKELY(ret)) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to write event.");
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }

end:
        return status;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
    }
}

static inline bt_component_class_sink_consume_method_status
handle_packet_beginning_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    int ret;
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    const bt_packet *ir_packet = bt_message_packet_beginning_borrow_packet_const(msg);
    const bt_stream *ir_stream = bt_packet_borrow_stream_const(ir_packet);
    struct fs_sink_stream *stream;
    const bt_clock_snapshot *cs = NULL;

    stream = borrow_stream(fs_sink, ir_stream);
    if (G_UNLIKELY(!stream)) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    if (stream->sc->packets_have_ts_begin) {
        cs = bt_message_packet_beginning_borrow_default_clock_snapshot_const(msg);
        BT_ASSERT(cs);
    }

    /*
     * If we previously received a discarded events message with
     * a time range, make sure that its beginning time matches what's
     * expected for CTF 1.8, that is:
     *
     * * Its beginning time is the previous packet's end
     *   time (or the current packet's beginning time if
     *   this is the first packet).
     *
     * We check this here instead of in handle_packet_end_msg()
     * because we want to catch any incompatible message as early as
     * possible to report the error.
     *
     * Validation of the discarded events message's end time is
     * performed in handle_packet_end_msg().
     */
    if (stream->discarded_events_state.in_range) {
        uint64_t expected_cs;

        /*
         * `stream->discarded_events_state.in_range` is only set
         * when the stream class's discarded events have a time
         * range.
         *
         * It is required that the packet beginning and end
         * messages for this stream class have times when
         * discarded events have a time range.
         */
        BT_ASSERT(stream->sc->discarded_events_has_ts);
        BT_ASSERT(stream->sc->packets_have_ts_begin);
        BT_ASSERT(stream->sc->packets_have_ts_end);

        if (stream->prev_packet_state.end_cs == UINT64_C(-1)) {
            /* We're opening the first packet */
            expected_cs = bt_clock_snapshot_get_value(cs);
        } else {
            expected_cs = stream->prev_packet_state.end_cs;
        }

        if (stream->discarded_events_state.beginning_cs != expected_cs) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger,
                "Incompatible discarded events message: "
                "unexpected beginning time: "
                "beginning-cs-val={}, "
                "expected-beginning-cs-val={}, "
                "stream-id={}, stream-name=\"{}\", "
                "trace-name=\"{}\", path=\"{}/{}\"",
                stream->discarded_events_state.beginning_cs, expected_cs,
                bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                stream->trace->path->str, stream->file_name->str);
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }
    }

    /*
     * If we previously received a discarded packets message with a
     * time range, make sure that its beginning and end times match
     * what's expected for CTF 1.8, that is:
     *
     * * Its beginning time is the previous packet's end time.
     *
     * * Its end time is the current packet's beginning time.
     */
    if (stream->discarded_packets_state.in_range) {
        uint64_t expected_end_cs;

        /*
         * `stream->discarded_packets_state.in_range` is only
         * set when the stream class's discarded packets have a
         * time range.
         *
         * It is required that the packet beginning and end
         * messages for this stream class have times when
         * discarded packets have a time range.
         */
        BT_ASSERT(stream->sc->discarded_packets_has_ts);
        BT_ASSERT(stream->sc->packets_have_ts_begin);
        BT_ASSERT(stream->sc->packets_have_ts_end);

        /*
         * It is not supported to have a discarded packets
         * message _before_ the first packet: we cannot validate
         * that its beginning time is compatible with CTF 1.8 in
         * this case.
         */
        if (stream->prev_packet_state.end_cs == UINT64_C(-1)) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger,
                "Incompatible discarded packets message "
                "occurring before the stream's first packet: "
                "stream-id={}, stream-name=\"{}\", "
                "trace-name=\"{}\", path=\"{}/{}\"",
                bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                stream->trace->path->str, stream->file_name->str);
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }

        if (stream->discarded_packets_state.beginning_cs != stream->prev_packet_state.end_cs) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger,
                "Incompatible discarded packets message: "
                "unexpected beginning time: "
                "beginning-cs-val={}, "
                "expected-beginning-cs-val={}, "
                "stream-id={}, stream-name=\"{}\", "
                "trace-name=\"{}\", path=\"{}/{}\"",
                stream->discarded_packets_state.beginning_cs, stream->prev_packet_state.end_cs,
                bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                stream->trace->path->str, stream->file_name->str);
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }

        expected_end_cs = bt_clock_snapshot_get_value(cs);

        if (stream->discarded_packets_state.end_cs != expected_end_cs) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger,
                "Incompatible discarded packets message: "
                "unexpected end time: "
                "end-cs-val={}, "
                "expected-end-cs-val={}, "
                "stream-id={}, stream-name=\"{}\", "
                "trace-name=\"{}\", path=\"{}/{}\"",
                stream->discarded_packets_state.end_cs, expected_end_cs,
                bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                stream->trace->path->str, stream->file_name->str);
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }
    }

    /*
     * We're not in a discarded packets time range anymore since we
     * require that the discarded packets time ranges go from one
     * packet's end time to the next packet's beginning time, and
     * we're handling a packet beginning message here.
     */
    stream->discarded_packets_state.in_range = false;

    ret = fs_sink_stream_open_packet(stream, cs, ir_packet);
    if (ret) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to open packet.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

end:
    return status;
}

static inline bt_component_class_sink_consume_method_status
handle_packet_end_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    int ret;
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    const bt_packet *ir_packet = bt_message_packet_end_borrow_packet_const(msg);
    const bt_stream *ir_stream = bt_packet_borrow_stream_const(ir_packet);
    struct fs_sink_stream *stream;
    const bt_clock_snapshot *cs = NULL;

    stream = borrow_stream(fs_sink, ir_stream);
    if (G_UNLIKELY(!stream)) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    if (stream->sc->packets_have_ts_end) {
        cs = bt_message_packet_end_borrow_default_clock_snapshot_const(msg);
        BT_ASSERT(cs);
    }

    /*
     * If we previously received a discarded events message with
     * a time range, make sure that its end time matches what's
     * expected for CTF 1.8, that is:
     *
     * * Its end time is the current packet's end time.
     *
     * Validation of the discarded events message's beginning time
     * is performed in handle_packet_beginning_msg().
     */
    if (stream->discarded_events_state.in_range) {
        uint64_t expected_cs;

        /*
         * `stream->discarded_events_state.in_range` is only set
         * when the stream class's discarded events have a time
         * range.
         *
         * It is required that the packet beginning and end
         * messages for this stream class have times when
         * discarded events have a time range.
         */
        BT_ASSERT(stream->sc->discarded_events_has_ts);
        BT_ASSERT(stream->sc->packets_have_ts_begin);
        BT_ASSERT(stream->sc->packets_have_ts_end);

        expected_cs = bt_clock_snapshot_get_value(cs);

        if (stream->discarded_events_state.end_cs != expected_cs) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(
                fs_sink->logger,
                "Incompatible discarded events message: "
                "unexpected end time: "
                "end-cs-val={}, "
                "expected-end-cs-val={}, "
                "stream-id={}, stream-name=\"{}\", "
                "trace-name=\"{}\", path=\"{}/{}\"",
                stream->discarded_events_state.end_cs, expected_cs, bt_stream_get_id(ir_stream),
                bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                stream->trace->path->str, stream->file_name->str);
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }
    }

    ret = fs_sink_stream_close_packet(stream, cs);
    if (ret) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to close packet.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    /*
     * We're not in a discarded events time range anymore since we
     * require that the discarded events time ranges go from one
     * packet's end time to the next packet's end time, and we're
     * handling a packet end message here.
     */
    stream->discarded_events_state.in_range = false;

end:
    return status;
}

static inline bt_component_class_sink_consume_method_status
handle_stream_beginning_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    const bt_stream *ir_stream = bt_message_stream_beginning_borrow_stream_const(msg);
    const bt_stream_class *ir_sc = bt_stream_borrow_class_const(ir_stream);
    struct fs_sink_stream *stream;
    bool packets_have_beginning_end_cs =
        bt_stream_class_packets_have_beginning_default_clock_snapshot(ir_sc) &&
        bt_stream_class_packets_have_end_default_clock_snapshot(ir_sc);

    /*
     * Not supported: discarded events or discarded packets support
     * without packets support. Packets are the way to know where
     * discarded events/packets occurred in CTF 1.8.
     */
    if (!bt_stream_class_supports_packets(ir_sc)) {
        BT_ASSERT(!bt_stream_class_supports_discarded_packets(ir_sc));

        if (!fs_sink->ignore_discarded_events && bt_stream_class_supports_discarded_events(ir_sc)) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                         "Unsupported stream: "
                                         "stream does not support packets, "
                                         "but supports discarded events: "
                                         "stream-addr={}, "
                                         "stream-id={}, "
                                         "stream-name=\"{}\"",
                                         fmt::ptr(ir_stream), bt_stream_get_id(ir_stream),
                                         bt2c::maybeNull(bt_stream_get_name(ir_stream)));
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }
    }

    /*
     * Not supported: discarded events with default clock snapshots,
     * but packet beginning/end without default clock snapshot.
     */
    if (!fs_sink->ignore_discarded_events &&
        bt_stream_class_discarded_events_have_default_clock_snapshots(ir_sc) &&
        !packets_have_beginning_end_cs) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                     "Unsupported stream: discarded events have "
                                     "default clock snapshots, but packets have no "
                                     "beginning and/or end default clock snapshots: "
                                     "stream-addr={}, "
                                     "stream-id={}, "
                                     "stream-name=\"{}\"",
                                     fmt::ptr(ir_stream), bt_stream_get_id(ir_stream),
                                     bt2c::maybeNull(bt_stream_get_name(ir_stream)));
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    /*
     * Not supported: discarded packets with default clock
     * snapshots, but packet beginning/end without default clock
     * snapshot.
     */
    if (!fs_sink->ignore_discarded_packets &&
        bt_stream_class_discarded_packets_have_default_clock_snapshots(ir_sc) &&
        !packets_have_beginning_end_cs) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                     "Unsupported stream: discarded packets have "
                                     "default clock snapshots, but packets have no "
                                     "beginning and/or end default clock snapshots: "
                                     "stream-addr={}, "
                                     "stream-id={}, "
                                     "stream-name=\"{}\"",
                                     fmt::ptr(ir_stream), bt_stream_get_id(ir_stream),
                                     bt2c::maybeNull(bt_stream_get_name(ir_stream)));
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    stream = borrow_stream(fs_sink, ir_stream);
    if (!stream) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    BT_CPPLOGI_SPEC(fs_sink->logger,
                    "Created new, empty stream file: "
                    "stream-id={}, stream-name=\"{}\", "
                    "trace-name=\"{}\", path=\"{}/{}\"",
                    bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                    bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                    stream->trace->path->str, stream->file_name->str);

end:
    return status;
}

static inline bt_component_class_sink_consume_method_status
handle_stream_end_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    const bt_stream *ir_stream = bt_message_stream_end_borrow_stream_const(msg);
    struct fs_sink_stream *stream;

    stream = borrow_stream(fs_sink, ir_stream);
    if (!stream) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    if (G_UNLIKELY(!stream->sc->has_packets && stream->packet_state.is_open)) {
        /* Close stream's current artificial packet */
        int ret = fs_sink_stream_close_packet(stream, NULL);

        if (ret) {
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to close packet.");
            status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
            goto end;
        }
    }

    BT_CPPLOGI_SPEC(fs_sink->logger,
                    "Closing stream file: "
                    "stream-id={}, stream-name=\"{}\", "
                    "trace-name=\"{}\", path=\"{}/{}\"",
                    bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                    bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                    stream->trace->path->str, stream->file_name->str);

    /*
     * This destroys the stream object and frees all its resources,
     * closing the stream file.
     */
    g_hash_table_remove(stream->trace->streams, ir_stream);

end:
    return status;
}

static inline bt_component_class_sink_consume_method_status
handle_discarded_events_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    const bt_stream *ir_stream = bt_message_discarded_events_borrow_stream_const(msg);
    struct fs_sink_stream *stream;
    const bt_clock_snapshot *cs = NULL;
    bt_property_availability avail;
    uint64_t count;

    stream = borrow_stream(fs_sink, ir_stream);
    if (!stream) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    if (fs_sink->ignore_discarded_events) {
        BT_CPPLOGI_SPEC(fs_sink->logger,
                        "Ignoring discarded events message: "
                        "stream-id={}, stream-name=\"{}\", "
                        "trace-name=\"{}\", path=\"{}/{}\"",
                        bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                        bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                        stream->trace->path->str, stream->file_name->str);
        goto end;
    }

    if (stream->discarded_events_state.in_range) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(
            fs_sink->logger,
            "Unsupported contiguous discarded events message: "
            "stream-id={}, stream-name=\"{}\", "
            "trace-name=\"{}\", path=\"{}/{}\"",
            bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
            bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
            stream->trace->path->str, stream->file_name->str);
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    /*
     * If we're currently in an opened packet (got a packet
     * beginning message, but no packet end message yet), we do not
     * support having a discarded events message with a time range
     * because we require that the discarded events message's time
     * range go from a packet's end time to the next packet's end
     * time.
     */
    if (stream->packet_state.is_open && stream->sc->discarded_events_has_ts) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(
            fs_sink->logger,
            "Unsupported discarded events message with "
            "default clock snapshots occurring within a packet: "
            "stream-id={}, stream-name=\"{}\", "
            "trace-name=\"{}\", path=\"{}/{}\"",
            bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
            bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
            stream->trace->path->str, stream->file_name->str);
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    if (stream->sc->discarded_events_has_ts) {
        /*
         * Make the stream's state be in the time range of a
         * discarded events message since we have the message's
         * time range (`stream->sc->discarded_events_has_ts`).
         */
        stream->discarded_events_state.in_range = true;

        /*
         * The clock snapshot values will be validated when
         * handling the next packet beginning and end messages
         * (next calls to handle_packet_beginning_msg() and
         * handle_packet_end_msg()).
         */
        cs = bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(msg);
        BT_ASSERT(cs);
        stream->discarded_events_state.beginning_cs = bt_clock_snapshot_get_value(cs);
        cs = bt_message_discarded_events_borrow_end_default_clock_snapshot_const(msg);
        BT_ASSERT(cs);
        stream->discarded_events_state.end_cs = bt_clock_snapshot_get_value(cs);
    }

    avail = bt_message_discarded_events_get_count(msg, &count);
    if (avail != BT_PROPERTY_AVAILABILITY_AVAILABLE) {
        /*
         * There's no specific count of discarded events: set it
         * to 1 so that we know that we at least discarded
         * something.
         */
        count = 1;
    }

    stream->packet_state.discarded_events_counter += count;

end:
    return status;
}

static inline bt_component_class_sink_consume_method_status
handle_discarded_packets_msg(struct fs_sink_comp *fs_sink, const bt_message *msg)
{
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    const bt_stream *ir_stream = bt_message_discarded_packets_borrow_stream_const(msg);
    struct fs_sink_stream *stream;
    const bt_clock_snapshot *cs = NULL;
    bt_property_availability avail;
    uint64_t count;

    stream = borrow_stream(fs_sink, ir_stream);
    if (!stream) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to borrow stream.");
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    if (fs_sink->ignore_discarded_packets) {
        BT_CPPLOGI_SPEC(fs_sink->logger,
                        "Ignoring discarded packets message: "
                        "stream-id={}, stream-name=\"{}\", "
                        "trace-name=\"{}\", path=\"{}/{}\"",
                        bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
                        bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
                        stream->trace->path->str, stream->file_name->str);
        goto end;
    }

    if (stream->discarded_packets_state.in_range) {
        BT_CPPLOGE_APPEND_CAUSE_SPEC(
            fs_sink->logger,
            "Unsupported contiguous discarded packets message: "
            "stream-id={}, stream-name=\"{}\", "
            "trace-name=\"{}\", path=\"{}/{}\"",
            bt_stream_get_id(ir_stream), bt2c::maybeNull(bt_stream_get_name(ir_stream)),
            bt2c::maybeNull(bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream))),
            stream->trace->path->str, stream->file_name->str);
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        goto end;
    }

    /*
     * Discarded packets messages are guaranteed to occur between
     * packets.
     */
    BT_ASSERT(!stream->packet_state.is_open);

    if (stream->sc->discarded_packets_has_ts) {
        /*
         * Make the stream's state be in the time range of a
         * discarded packets message since we have the message's
         * time range (`stream->sc->discarded_packets_has_ts`).
         */
        stream->discarded_packets_state.in_range = true;

        /*
         * The clock snapshot values will be validated when
         * handling the next packet beginning message (next call
         * to handle_packet_beginning_msg()).
         */
        cs = bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(msg);
        BT_ASSERT(cs);
        stream->discarded_packets_state.beginning_cs = bt_clock_snapshot_get_value(cs);
        cs = bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(msg);
        BT_ASSERT(cs);
        stream->discarded_packets_state.end_cs = bt_clock_snapshot_get_value(cs);
    }

    avail = bt_message_discarded_packets_get_count(msg, &count);
    if (avail != BT_PROPERTY_AVAILABILITY_AVAILABLE) {
        /*
         * There's no specific count of discarded packets: set
         * it to 1 so that we know that we at least discarded
         * something.
         */
        count = 1;
    }

    stream->packet_state.seq_num += count;

end:
    return status;
}

static inline void put_messages(bt_message_array_const msgs, uint64_t count)
{
    uint64_t i;

    for (i = 0; i < count; i++) {
        BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);
    }
}

bt_component_class_sink_consume_method_status ctf_fs_sink_consume(bt_self_component_sink *self_comp)
{
    bt_component_class_sink_consume_method_status status =
        BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
    struct fs_sink_comp *fs_sink;
    bt_message_iterator_next_status next_status;
    uint64_t msg_count = 0;
    bt_message_array_const msgs;

    fs_sink = (fs_sink_comp *) bt_self_component_get_data(
        bt_self_component_sink_as_self_component(self_comp));
    BT_ASSERT_DBG(fs_sink);
    BT_ASSERT_DBG(fs_sink->upstream_iter);

    /* Consume messages */
    next_status = bt_message_iterator_next(fs_sink->upstream_iter, &msgs, &msg_count);
    if (next_status < 0) {
        status = (bt_component_class_sink_consume_method_status) next_status;
        BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                     "Failed to get next message from upstream iterator.");
        goto end;
    }

    switch (next_status) {
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_OK:
    {
        uint64_t i;

        for (i = 0; i < msg_count; i++) {
            const bt_message *msg = msgs[i];

            BT_ASSERT_DBG(msg);

            switch (bt_message_get_type(msg)) {
            case BT_MESSAGE_TYPE_EVENT:
                status = handle_event_msg(fs_sink, msg);
                break;
            case BT_MESSAGE_TYPE_PACKET_BEGINNING:
                status = handle_packet_beginning_msg(fs_sink, msg);
                break;
            case BT_MESSAGE_TYPE_PACKET_END:
                status = handle_packet_end_msg(fs_sink, msg);
                break;
            case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
                /* Ignore */
                BT_CPPLOGD_SPEC(fs_sink->logger, "Ignoring message iterator inactivity message.");
                break;
            case BT_MESSAGE_TYPE_STREAM_BEGINNING:
                status = handle_stream_beginning_msg(fs_sink, msg);
                break;
            case BT_MESSAGE_TYPE_STREAM_END:
                status = handle_stream_end_msg(fs_sink, msg);
                break;
            case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
                status = handle_discarded_events_msg(fs_sink, msg);
                break;
            case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
                status = handle_discarded_packets_msg(fs_sink, msg);
                break;
            default:
                bt_common_abort();
            }

            BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);

            if (status != BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK) {
                BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger,
                                             "Failed to handle message: "
                                             "generated CTF traces could be incomplete: "
                                             "output-dir-path=\"{}\"",
                                             fs_sink->output_dir_path->str);
                goto error;
            }
        }

        break;
    }
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN:
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN;
        break;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:
        /* TODO: Finalize all traces (should already be done?) */
        status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END;
        break;
    default:
        break;
    }

    goto end;

error:
    BT_ASSERT(status != BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK);
    put_messages(msgs, msg_count);

end:
    return status;
}

bt_component_class_sink_graph_is_configured_method_status
ctf_fs_sink_graph_is_configured(bt_self_component_sink *self_comp)
{
    try {
        bt_component_class_sink_graph_is_configured_method_status status;
        bt_message_iterator_create_from_sink_component_status msg_iter_status;
        fs_sink_comp *fs_sink = (fs_sink_comp *) bt_self_component_get_data(
            bt_self_component_sink_as_self_component(self_comp));

        msg_iter_status = bt_message_iterator_create_from_sink_component(
            self_comp, bt_self_component_sink_borrow_input_port_by_name(self_comp, in_port_name),
            &fs_sink->upstream_iter);
        if (msg_iter_status != BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK) {
            status = (bt_component_class_sink_graph_is_configured_method_status) msg_iter_status;
            BT_CPPLOGE_APPEND_CAUSE_SPEC(fs_sink->logger, "Failed to create upstream iterator.");
            goto end;
        }

        status = BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK;
end:
        return status;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2c::Error&) {
        return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_ERROR;
    }
}

void ctf_fs_sink_finalize(bt_self_component_sink *self_comp)
{
    fs_sink_comp *fs_sink = (fs_sink_comp *) bt_self_component_get_data(
        bt_self_component_sink_as_self_component(self_comp));

    destroy_fs_sink_comp(fs_sink);
}

bt_component_class_get_supported_mip_versions_method_status
ctf_fs_sink_supported_mip_versions(bt_self_component_class_sink *self_component_class,
                                   const bt_value *params, void *, bt_logging_level log_level,
                                   bt_integer_range_set_unsigned *supported_versions)
{
    gchar *validation_error;
    bt_component_class_get_supported_mip_versions_method_status status;
    bt2c::Logger logger {bt2::wrap(self_component_class), static_cast<bt2::LoggingLevel>(log_level),
                         "PLUGIN/SINK.CTF.FS"};

    {
        const auto validation_status =
            bt_param_validation_validate(params, fs_sink_params_descr, &validation_error);

        if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
            status = BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR;
            BT_CPPLOGE_APPEND_CAUSE_SPEC(logger, "{}", validation_error);
            goto end;
        } else if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
            status = BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR;
            goto end;
        }
    }

    {
        const auto ctfVersion = ctfVersionFromParams(params, logger);

        if (ctfVersion < 0) {
            status = BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR;
            goto end;
        }

        const auto mipVersion = ctfVersionFromParams(params, logger) == 2U ? 1 : 0;

        bt2::wrap(supported_versions).addRange(mipVersion, mipVersion);
    }

    status = BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK;

end:
    g_free(validation_error);
    return status;
}
