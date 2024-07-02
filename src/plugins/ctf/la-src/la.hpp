/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * BabelTrace - CTF on File System Component
 */

#ifndef BABELTRACE_PLUGINS_CTF_LA_SRC_HPP
#define BABELTRACE_PLUGINS_CTF_LA_SRC_HPP

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/bt2c/aliases.hpp"
#include "cpp-common/bt2c/fmt.hpp"
#include "cpp-common/bt2c/logging.hpp"
#include "cpp-common/bt2c/prio-heap.hpp"
#include "cpp-common/bt2s/optional.hpp"

#include "plugins/common/muxing/muxing.h"

#include "data-stream-file.hpp"
#include "plugins/ctf/common/src/metadata/ctf-ir.hpp"
#include "plugins/ctf/common/src/metadata/metadata-stream-parser-utils.hpp"
#include "plugins/ctf/common/src/msg-iter.hpp"

#define CTF_LA_METADATA_FILENAME "metadata"

extern bool ctf_la_debug;

struct ctf_la_trace
{
    using UP = std::unique_ptr<ctf_la_trace>;

    explicit ctf_la_trace(const ctf::src::ClkClsCfg& clkClsCfg,
                          const bt2::OptionalBorrowedObject<bt2::SelfComponent> selfComp,
                          const bt2c::Logger& parentLogger) :
        _mLogger {parentLogger, "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/TRACE"},
        _mClkClsCfg {clkClsCfg}, _mSelfComp {selfComp}
    {
    }

    const ctf::src::TraceCls *cls() const
    {
        BT_ASSERT(_mParseRet);
        BT_ASSERT(_mParseRet->traceCls);
        return _mParseRet->traceCls.get();
    }

    const bt2s::optional<bt2c::Uuid>& metadataStreamUuid() const noexcept
    {
        BT_ASSERT(_mParseRet);
        return _mParseRet->uuid;
    }

    void parseMetadata(const bt2c::ConstBytes buffer)
    {
        _mParseRet = ctf::src::parseMetadataStream(_mSelfComp, _mClkClsCfg, buffer, _mLogger);
    }

    bt2::Trace::Shared trace;

    std::string path;

    /* Next automatic stream ID when not provided by packet header */
    uint64_t next_stream_id = 0;

private:
    bt2c::Logger _mLogger;
    ctf::src::ClkClsCfg _mClkClsCfg;
    bt2::OptionalBorrowedObject<bt2::SelfComponent> _mSelfComp;
    bt2s::optional<ctf::src::MetadataStreamParser::ParseRet> _mParseRet;
};

struct ctf_la_component
{
    using UP = std::unique_ptr<ctf_la_component>;

    explicit ctf_la_component(const ctf::src::ClkClsCfg& clkClsCfgParam,
                              std::string sessionNameParam, const bool removeConsumedArchiveParam,
                              bt2s::optional<std::string> traceNameParam,
                              bt2s::optional<std::string> sessionFoundFilePathParam,
                              const bt2c::Logger& parentLogger) noexcept :
        logger {parentLogger, "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/COMP"},
        sessionName {std::move(sessionNameParam)},
        removeConsumedArchive {removeConsumedArchiveParam}, traceName {std::move(traceNameParam)},
        sessionFoundFilePath {sessionFoundFilePathParam},
        clkClsCfg {clkClsCfgParam}
    {
    }

    bt2c::Logger logger;

    /* Path to the output directory */
    std::string outputDirPath;

    /* Path to the `archives` directory within the output directory */
    std::string archivesPath;

    /* Recording session name (user-provided) */
    std::string sessionName;

    /* Whether or not to remove consumed trace archives */
    bool removeConsumedArchive;

    bt2s::optional<std::string> traceName;

    /*
     * Path of a file to create when the targeted recording session
     * is found.
     */
    bt2s::optional<std::string> sessionFoundFilePath;

    ctf_la_trace::UP firstArchiveTrace;

    ctf::src::ClkClsCfg clkClsCfg;
};

enum class MsgIterState
{
    /* Initial state */
    Init,

    /* Wait for the targeted recording session to exist */
    WaitForSessionToExist,

    /* Try to create the trace from the first archive */
    TryCreateFirstArchiveTrace,

    /* Do normal work */
    Work,

    /* Wait for the final trace archive (after recording session destruction) */
    WaitForNoMoreCurrentTraceChunk,

    /* End */
    End,
};

/*
 * A single entry within `ctf_la_msg_iter_data::heap`
 * and `ctf_la_msg_iter_data::heapEntries`.
 */
struct HeapEntry final
{
    HeapEntry(ctf_la_ds_file_group& dsfGroupParam) : dsfGroup {&dsfGroupParam}
    {
    }

    /*
     * Returns the timestamp of `msg`, if any.
     */
    static bt2s::optional<std::uint64_t> msgTs(const bt2::ConstMessage msg) noexcept
    {
        switch (msg.type()) {
        case bt2::MessageType::Event:
            return msg.asEvent().defaultClockSnapshot().value();
        case bt2::MessageType::PacketBeginning:
            return msg.asPacketBeginning().defaultClockSnapshot().value();
        case bt2::MessageType::PacketEnd:
            return msg.asPacketEnd().defaultClockSnapshot().value();
        case bt2::MessageType::DiscardedEvents:
            return msg.asDiscardedEvents().beginningDefaultClockSnapshot().value();
        case bt2::MessageType::DiscardedPackets:
            return msg.asDiscardedPackets().beginningDefaultClockSnapshot().value();
        case bt2::MessageType::MessageIteratorInactivity:
            return msg.asMessageIteratorInactivity().clockSnapshot();
        default:
            break;
        }

        return bt2s::nullopt;
    }

    /*
     * Sets `curMsg` to the next message, if any.
     *
     * Skips any stream end message.
     */
    void nextMsg()
    {
        BT_ASSERT(msgIter);
        curMsg = msgIter->next();

        if (curMsg && curMsg->isStreamEnd()) {
            /* Skip stream end message */
            curMsg.reset();
        }

        if (curMsg) {
            /* Set corresponding timestamp */
            curMsgTs = msgTs(*curMsg);
        } else {
            /* No timestamp */
            curMsgTs.reset();
        }
    }

    /*
     * Weak, to know the corresponding data stream class and data stream
     * ID, and also to access the queue of data stream file infos.
     */
    ctf_la_ds_file_group *dsfGroup;

    /*
     * All the following members aren't set when this heap entry
     * is sick.
     *
     * When the entry isn't sick, then it has a current
     * message (`curMsg`).
     */
    std::unique_ptr<ctf::src::MsgIter> msgIter;
    bt2::ConstMessage::Shared curMsg;
    bt2s::optional<std::uint64_t> curMsgTs;
};

/*
 * Comparator object for `ctf_la_msg_iter_data::heap`.
 */
struct HeapComparator final
{
    HeapComparator(const bt2c::Logger& loggerParam) : logger {loggerParam}
    {
    }

    /*
     * For logging.
     */
    static std::string optMsgTsStr(const bt2s::optional<std::int64_t>& ts)
    {
        if (ts) {
            return fmt::to_string(*ts);
        }

        return "none";
    }

    bool operator()(const HeapEntry * const a, const HeapEntry * const b) const noexcept
    {
        const auto msgA = *a->curMsg;
        const auto msgB = *b->curMsg;
        auto& msgTsA = a->curMsgTs;
        auto& msgTsB = b->curMsgTs;

        if (logger.wouldLogT()) {
            BT_CPPLOGT_SPEC(logger,
                            "Comparing two messages: "
                            "dsf-a-path=\"{}\", msg-a-type={}, msg-a-ts={}, "
                            "dsf-b-path=\"{}\", msg-b-type={}, msg-b-ts={}",
                            a->dsfGroup->curDsfInfo().path, msgA.type(), optMsgTsStr(msgTsA),
                            b->dsfGroup->curDsfInfo().path, msgB.type(), optMsgTsStr(msgTsB));
        }

        if (G_LIKELY(msgTsA && msgTsB)) {
            if (*msgTsA < *msgTsB) {
                BT_CPPLOGT_SPEC(
                    logger, "Timestamp of message A is less than timestamp of message B: oldest=A");
                return true;
            } else if (*msgTsA > *msgTsB) {
                BT_CPPLOGT_SPEC(
                    logger,
                    "Timestamp of message A is greater than timestamp of message B: oldest=B");
                return false;
            }
        } else if (msgTsA && !msgTsB) {
            BT_CPPLOGT_SPEC(logger, "Message A has a timestamp, but message B has none: oldest=B");
            return false;
        } else if (!msgTsA && msgTsB) {
            BT_CPPLOGT_SPEC(logger, "Message B has a timestamp, but message A has none: oldest=A");
            return true;
        }

        const auto res = common_muxing_compare_messages(msgA.libObjPtr(), msgB.libObjPtr()) < 0;

        BT_CPPLOGT_SPEC(logger,
                        "Timestamps are considered equal; comparing other properties: oldest={}",
                        res ? "A" : "B");
        return res;
    }

    bt2c::Logger logger;
};

struct ctf_la_msg_iter_data
{
    using UP = std::unique_ptr<ctf_la_msg_iter_data>;

    explicit ctf_la_msg_iter_data(bt_self_message_iterator *selfMsgIter) :
        self_msg_iter {selfMsgIter},
        logger {bt2::SelfMessageIterator {self_msg_iter}, "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/MSG-ITER"},
        heap {HeapComparator {logger}}
    {
    }

    /* Message iterator state */
    MsgIterState state = MsgIterState::Init;

    /*
     * Whether or not we already waited for no more current trace chunk
     * (after recording session destruction).
     */
    bool waitedForNoMoreCurrentTraceChunk = false;

    /*
     * Map of trace archive path to a number of consumed data stream
     * files within it.
     *
     * When such a count reaches `heapEntries.size()`, then we don't
     * need the corresponding archive anymore.
     */
    std::unordered_map<std::string, unsigned int> archiveConsumedDsfCount;

    /*
     * Map of data stream classes to map of data stream IDs to data
     * stream file groups.
     */
    std::unordered_map<const ctf::src::DataStreamCls *,
                       std::unordered_map<std::uint64_t, ctf_la_ds_file_group>>
        dsfGroups;

    /* Owned entries for `heap` below (which only deals with pointers) */
    std::vector<std::unique_ptr<HeapEntry>> heapEntries;

    /* Current sick heap entry to heal */
    HeapEntry *sickHeapEntry = nullptr;

    /* Weak */
    bt_self_message_iterator *self_msg_iter = nullptr;

    bt2c::Logger logger;

    /* Priority heap to sort outbound messages */
    bt2c::PrioHeap<HeapEntry *, HeapComparator> heap;
};

bt_component_class_initialize_method_status
ctf_la_init(bt_self_component_source *source, bt_self_component_source_configuration *config,
            const bt_value *params, void *init_method_data);

void ctf_la_finalize(bt_self_component_source *component);

bt_message_iterator_class_initialize_method_status
ctf_la_iterator_init(bt_self_message_iterator *self_msg_iter,
                     bt_self_message_iterator_configuration *config,
                     bt_self_component_port_output *self_port);

void ctf_la_iterator_finalize(bt_self_message_iterator *it);

bt_message_iterator_class_next_method_status
ctf_la_iterator_next(bt_self_message_iterator *iterator, bt_message_array_const msgs,
                     uint64_t capacity, uint64_t *count);

bt_message_iterator_class_seek_beginning_method_status
ctf_la_iterator_seek_beginning(bt_self_message_iterator *message_iterator);

namespace ctf {
namespace src {
namespace la {

/* `src.ctf.lttng-archive` parameters */

struct Parameters
{
    explicit Parameters(std::string sessionNameParam) noexcept :
        sessionName {std::move(sessionNameParam)}
    {
    }

    /* Name of targeted recording session */
    std::string sessionName;

    /* Whether or not to remove consumed trace archives */
    bool removeConsumedArchive = true;

    /*
     * Path of a file to create when the targeted recording session
     * is found.
     */
    bt2s::optional<std::string> sessionFoundFilePath;

    bt2s::optional<std::string> traceName;
    ClkClsCfg clkClsCfg;
};

} /* namespace la */
} /* namespace src */
} /* namespace ctf */

/*
 * Read and validate parameters taken by the src.ctf.lttng-archive plugin.
 *
 * Throw if any parameter doesn't pass validation.
 */

ctf::src::la::Parameters read_src_la_parameters(bt2::ConstMapValue params,
                                                const bt2c::Logger& logger);

/*
 * Generate the port name to be used for a given data stream file group.
 */

std::string ctf_la_make_port_name(ctf_la_ds_file_group *ds_file_group);

#endif /* BABELTRACE_PLUGINS_CTF_LA_SRC_HPP */
