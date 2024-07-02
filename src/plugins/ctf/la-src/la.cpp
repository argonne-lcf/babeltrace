/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2015-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#include <cstdint>
#include <fstream>

#include <glib.h>
#include <glib/gstdio.h>
#include <lttng/lttng.h>
#include <sys/stat.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/bt2/wrap.hpp"
#include "cpp-common/bt2c/file-utils.hpp"
#include "cpp-common/bt2c/glib-up.hpp"
#include "cpp-common/bt2c/logging.hpp"
#include "cpp-common/bt2s/make-unique.hpp"
#include "cpp-common/bt2s/optional.hpp"
#include "cpp-common/vendor/fmt/core.h"

#include "plugins/common/param-validation/param-validation.h"

#include "../common/src/metadata/ctf-ir.hpp"
#include "../common/src/metadata/tsdl/ctf-meta-configure-ir-trace.hpp"
#include "../common/src/msg-iter.hpp"
#include "../common/src/pkt-props.hpp"
#include "data-stream-file.hpp"
#include "la.hpp"
#include "metadata.hpp"

using namespace bt2c::literals::datalen;
using namespace ctf::src;
using namespace ctf;

struct tracer_info
{
    const char *name;
    int64_t major;
    int64_t minor;
    int64_t patch;
};

namespace {

/*
 * Returns the info of the LTTng recording session named
 * `comp.sessionName`, or `bt2s::nullopt` if none.
 */
bt2s::optional<lttng_session> sessionInfo(const ctf_la_component& comp)
{
    lttng_session *sessions = nullptr;
    const auto count = lttng_list_sessions(&sessions);

    if (count < 0) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(comp.logger, bt2::Error,
                                               "lttng_list_sessions() failed.");
    }

    for (auto i = 0; i < count; ++i) {
        const auto& curSession = sessions[i];

        if (comp.sessionName == curSession.name) {
            /*
             * Found it: copy object to return so as to free
             * `sessions` here.
             */
            auto session = curSession;

            std::free(sessions);
            return session;
        }
    }

    /* Nothing found */
    std::free(sessions);
    return bt2s::nullopt;
}

void set_trace_name(const bt2::Trace trace, const bt2s::optional<std::string>& traceName)
{
    std::string name;

    /*
     * Check if we have a trace environment string value named `hostname`.
     * If so, use it as the trace name's prefix.
     */
    const auto val = trace.environmentEntry("hostname");
    if (val && val->isString()) {
        name += val->asString().value();

        if (traceName) {
            name += G_DIR_SEPARATOR;
        }
    }

    if (traceName) {
        name += *traceName;
    }

    trace.name(name);
}

std::string xSlashY(const std::string& x, const std::string& y)
{
    return fmt::format("{}" G_DIR_SEPARATOR_S "{}", x, y);
}

ctf_la_trace::UP ctf_la_trace_create(const std::string& path,
                                            const bt2s::optional<std::string>& traceName,
                                            ctf::src::ClkClsCfg clkClsCfg,
                                            bt_self_component *selfComp, const bt2c::Logger& logger)
{
    auto ctf_la_trace = bt2s::make_unique<struct ctf_la_trace>(clkClsCfg, selfComp, logger);
    const auto metadataPath = xSlashY(path, CTF_LA_METADATA_FILENAME);

    ctf_la_trace->path = path;
    ctf_la_trace->parseMetadata(bt2c::dataFromFile(metadataPath, logger, true));

    BT_ASSERT(ctf_la_trace->cls());

    if (ctf_la_trace->cls()->libCls()) {
        bt2::TraceClass traceCls = *ctf_la_trace->cls()->libCls();
        ctf_la_trace->trace = traceCls.instantiate();
        ctf_trace_class_configure_ir_trace(*ctf_la_trace->cls(), *ctf_la_trace->trace,
                                           bt_self_component_get_graph_mip_version(selfComp),
                                           logger);
        set_trace_name(*ctf_la_trace->trace, traceName);
    }

    return ctf_la_trace;
}

ctf_la_trace::UP createTrace(ctf_la_component& comp, const std::string& path,
                                    bt_self_component * const selfComp)
{
    ctf_la_trace::UP ctf_la_trace =
        ctf_la_trace_create(path, comp.traceName, comp.clkClsCfg, selfComp, comp.logger);
    if (!ctf_la_trace) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(comp.logger, bt2c::Error,
                                               "Cannot create trace for `{}`.", path);
    }

    return ctf_la_trace;
}

/*
 * A file system entry: file/directory name and whether or not it's
 * a directory.
 */
struct FsEntry final
{
    std::string name;
    bool isDir;
};

/*
 * Returns an unsorted list of all the entries of the directory `path`.
 */
std::vector<FsEntry> dirEntries(const std::string& path)
{
    BT_ASSERT(g_file_test(path.c_str(), G_FILE_TEST_IS_DIR) == TRUE);

    const auto gDir = g_dir_open(path.c_str(), 0, nullptr);
    std::vector<FsEntry> entries;

    while (const auto entryName = g_dir_read_name(gDir)) {
        entries.emplace_back(FsEntry {
            entryName,
            g_file_test(xSlashY(path, entryName).c_str(), G_FILE_TEST_IS_DIR) == TRUE});
    }

    g_dir_close(gDir);
    return entries;
}

/*
 * Recursively tries to find the first true CTF trace directory within
 * the directory `dirPath`.
 *
 * Returns `bt2s::nullopt` if none.
 */
bt2s::optional<std::string> findTraceDirInDir(const std::string& dirPath)
{
    for (auto& entry : dirEntries(dirPath)) {
        if (entry.name == "metadata") {
            /* Found it! */
            return dirPath;
        }

        if (entry.isDir) {
            /* Recurse */
            if (const auto traceDirCand = findTraceDirInDir(xSlashY(dirPath, entry.name))) {
                return traceDirCand;
            }
        }
    }

    /* Nothing found */
    return bt2s::nullopt;
}

/*
 * Returns an ordered set of all the absolute archive directory paths
 * within `comp.archivesPath`.
 *
 * Lexicographically sorting archive directory paths is enough: the
 * first one is the oldest archive.
 */
std::set<std::string> archiveDirs(ctf_la_component& comp)
{
    std::set<std::string> archiveNames;

    for (auto& entry : dirEntries(comp.archivesPath)) {
        BT_ASSERT(entry.isDir);
        archiveNames.insert(xSlashY(comp.archivesPath, entry.name));
    }

    return archiveNames;
}

/*
 * Returns the size of the file `path`.
 */
bt2c::DataLen getFileSize(const std::string& path, const bt2c::Logger& logger)
{
    struct stat st;

    if (stat(path.c_str(), &st) != 0) {
        BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2::Error,
                                                     "Failed to get data stream file status", "path=\"{}\"", path);
    }

    return bt2c::DataLen::fromBytes(st.st_size);
}

/*
 * Returns an unordered list of all the data stream file paths within
 * the CTF trace directory `traceDir`.
 */
std::vector<std::string> traceDsFilePaths(ctf_la_msg_iter_data& msgIter,
                                                 const std::string& traceDir)
{
    std::vector<std::string> dsfPaths;

    for (auto& entry : dirEntries(traceDir)) {
        if (entry.name == "metadata" || entry.name[0] == '.' || entry.isDir) {
            /* Skip metadata stream, hidden file, or subdirectory */
            continue;
        }

        auto dsfPath = xSlashY(traceDir, entry.name);

        if (*getFileSize(dsfPath.c_str(), msgIter.logger) == 0) {
            /* Skip empty file */
            continue;
        }

        dsfPaths.emplace_back(std::move(dsfPath));
    }

    return dsfPaths;
}

/*
 * Tries to set `comp.outputDirPath` and `comp.archivesPath` for the
 * recording session named `comp.sessionName`.
 *
 * If the recording session is found, then this function writes the
 * recording session found file if required.
 *
 * Returns true if the recording session was found.
 */
bool waitForSessionToExist(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter)
{
    if (const auto sessInfo = sessionInfo(comp)) {
        BT_CPPLOGI_SPEC(msgIter.logger,
                        "Found target recording session: name=\"{}\", output-dir=\"{}\"",
                        comp.sessionName, sessInfo->path);

        /* Save output and archive directory paths */
        comp.outputDirPath = sessInfo->path;
        comp.archivesPath = xSlashY(sessInfo->path, "archives");

        /* Write the confirmation file if required */
        if (comp.sessionFoundFilePath) {
            std::ofstream {*comp.sessionFoundFilePath, std::ios::out | std::ios::trunc};
        }

        /* Found! */
        return true;
    }

    return false;
}

/*
 * Tries to set `comp.firstArchiveTrace` to the trace of the first
 * archive within the output directory of the recording session
 * named `comp.sessionName`.
 */
void tryCreateFirstArchiveTrace(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter,
                                       bt_self_component * const selfComp)
{
    BT_ASSERT(!comp.outputDirPath.empty());
    BT_ASSERT(!comp.archivesPath.empty());

    if (!g_file_test(comp.archivesPath.c_str(), G_FILE_TEST_IS_DIR)) {
        /* No archive directory yet */
        return;
    }

    for (const auto& archivePath : archiveDirs(comp)) {
        if (const auto firstArchiveTraceDir = findTraceDirInDir(archivePath)) {
            /* Found it! */
            BT_CPPLOGI_SPEC(msgIter.logger, "Found first trace: path=\"{}\"",
                            *firstArchiveTraceDir);
            comp.firstArchiveTrace = createTrace(comp, *firstArchiveTraceDir, selfComp);
            return;
        }
    }

    BT_CPPLOGI_SPEC(msgIter.logger, "Waiting for a recording session named `{0}`: name=\"{0}\"",
                    comp.sessionName);
}

/*
 * Returns the packet properties of the data stream file file `path`.
 */
PktProps
dsfProps(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter, const std::string& path)
{
    /* Create a temporary index with a single packet at offset 0 */
    ctf_la_ds_index tempIndex;

    tempIndex.entries.emplace_back(
        ctf_la_ds_index_entry {path, 0_bytes, getFileSize(path.c_str(), msgIter.logger)});

    /* Get the properties of the first packet */
    const auto props = readPktProps(*comp.firstArchiveTrace->cls(),
                                    bt2s::make_unique<la::Medium>(tempIndex, msgIter.logger),
                                    0_bytes, msgIter.logger);

    /* LTTng always has those */
    BT_ASSERT(props.dataStreamCls);
    BT_ASSERT(props.dataStreamId);

    /* Return properties */
    return props;
}

struct PktPropsComp final
{
    bool operator()(const PktProps& a, const PktProps& b) const
    {
        return std::make_pair(a.dataStreamCls, *a.dataStreamId) < std::make_pair(b.dataStreamCls, *b.dataStreamId);
    }
};

/*
 * Creates the data stream file groups given what's found in the trace
 * of the first archive (`comp.firstArchiveTrace`).
 *
 * We expect that there's a single data stream file per (data stream
 * class, data stream ID) pair.
 */
void createDsFileGroups(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter)
{
    /*
     * A validation set to assert that there's a single data stream file
     * per (data stream class, data stream ID) pair.
     */
    std::set<PktProps, PktPropsComp> validationSet;

    /* For each data stream file of the trace of the first archive */
    for (auto& dsfPath : traceDsFilePaths(msgIter, comp.firstArchiveTrace->path)) {
        /* Properties of the data stream file */
        const auto props = dsfProps(comp, msgIter, dsfPath);

        BT_CPPLOGI_SPEC(
            msgIter.logger,
            "Found initial data stream file: path=\"{}\", stream-cls-id={}, stream-id={}", dsfPath,
            props.dataStreamCls->id(), *props.dataStreamId);

        /*
         * Confirm that there's a single data stream file per (data
         * stream class, data stream ID) pair.
         */
        BT_ASSERT(validationSet.count(props) == 0);
        validationSet.insert(props);

        /* Add new data stream file group */
        auto it = msgIter.dsfGroups.find(props.dataStreamCls);

        if (it == msgIter.dsfGroups.end()) {
            it = msgIter.dsfGroups.emplace(std::make_pair(props.dataStreamCls, decltype(it->second) {}))
                     .first;
        }

        BT_ASSERT(it->second.count(*props.dataStreamId) == 0);

        auto& dsfGroup = it->second
                             .emplace(std::make_pair(
                                 *props.dataStreamId, ctf_la_ds_file_group {*props.dataStreamCls, *props.dataStreamId}))
                             .first->second;

        BT_ASSERT(dsfGroup.dataStreamCls->libCls());

        /* Create trace IR stream, using its full path as its name */
        dsfGroup.stream = dsfGroup.dataStreamCls->libCls()->instantiate(
            *comp.firstArchiveTrace->trace, dsfGroup.stream_id);
        dsfGroup.stream->name(dsfPath);
    }
}

/*
 * Tries to find new data stream files within `comp.archivePath`,
 * updating the queues of `msgIter.dsfGroups` accordingly.
 */
void tryAddNewDsFileInfos(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter)
{
    BT_ASSERT(!comp.archivesPath.empty());

    for (const auto& archivePath : archiveDirs(comp)) {
        if (msgIter.archiveConsumedDsfCount.count(archivePath) > 0) {
            /* Archive already processed */
            continue;
        }

        if (const auto archiveTraceDir = findTraceDirInDir(archivePath)) {
            BT_CPPLOGI_SPEC(msgIter.logger, "Found new trace archive: path=\"{}\"", archivePath);

            for (auto& dsfPath : traceDsFilePaths(msgIter, *archiveTraceDir)) {
                const auto props = dsfProps(comp, msgIter, dsfPath);

                BT_CPPLOGI_SPEC(
                    msgIter.logger,
                    "Found new data stream file to consume: path=\"{}\", stream-cls-id={}, stream-id={}",
                    dsfPath, props.dataStreamCls->id(), *props.dataStreamId);

                /* Find corresponding data stream file group (must exist at this point) */
                auto& dsfGroup = msgIter.dsfGroups.at(props.dataStreamCls).at(*props.dataStreamId);

                /* Create packet index */
                auto index = ctf_la_ds_file_build_index(dsfPath, *comp.firstArchiveTrace->cls(),
                                                        msgIter.logger);

                BT_ASSERT(index);

                /* Create corresponding data stream file info and add to queue */
                dsfGroup.dsfInfos.emplace(archivePath, dsfPath, std::move(*index), msgIter.logger);
            }

            /* Initially no consumed data stream files */
            msgIter.archiveConsumedDsfCount[archivePath] = 0;
        }
    }
}

/*
 * Creates a common message iterator for the data stream file info
 * `dsfInfo` within the group `dsfGroup`.
 */
std::unique_ptr<ctf::src::MsgIter> createMsgIter(ctf_la_component& comp,
                                                        ctf_la_msg_iter_data& msgIter,
                                                        const ctf_la_ds_file_group& dsfGroup,
                                                        const ctf_la_ds_file_info& dsfInfo)
{
    /* No quirks: assume we're working with a fixed version of LTTng */
    return bt2s::make_unique<ctf::src::MsgIter>(
        bt2::wrap(msgIter.self_msg_iter), *comp.firstArchiveTrace->cls(),
        comp.firstArchiveTrace->metadataStreamUuid(), *dsfGroup.stream, bt2s::make_unique<la::Medium>(dsfInfo.index, msgIter.logger),
        MsgIterQuirks {}, msgIter.logger);
}

/*
 * Removes the directory `path`, empty or not.
 */
void removeDir(const std::string& path)
{
    for (const auto& entry : dirEntries(path)) {
        const auto fullPath = xSlashY(path, entry.name);

        if (entry.isDir) {
            removeDir(fullPath);
        } else {
            const auto ret = g_remove(fullPath.c_str());

            BT_ASSERT(ret == 0);
        }
    }

    const auto ret = g_remove(path.c_str());

    BT_ASSERT(ret == 0);
}

/*
 * Tries to heal `entry`, that is, to make it have a data stream file
 * info, a common message iterator, and a current message.
 *
 * If `skipStreamBegin` is true, then if `entry` is healed, its current
 * message isn't a stream beginning message.
 */
void tryHealHeapEntry(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter,
                             HeapEntry& entry, const bool skipStreamBegin)
{
    /* Assumed to be sick (no current message) */
    BT_ASSERT(!entry.curMsg);
    BT_ASSERT(!entry.curMsgTs);

    if (entry.msgIter) {
        /* Get the next message */
        entry.nextMsg();

        if (!entry.curMsg) {
            /* No more messages: reset common message iterator */
            entry.msgIter.reset();

            /* Mark current data stream file info as consumed */
            const auto archivePath = entry.dsfGroup->curDsfInfo().archivePath;

            BT_CPPLOGI_SPEC(msgIter.logger, "Consumed data stream file completely: path=\"{}\"",
                            entry.dsfGroup->curDsfInfo().path);
            entry.dsfGroup->dsfInfos.pop();
            ++msgIter.archiveConsumedDsfCount[archivePath];
            BT_CPPLOGI_SPEC(
                msgIter.logger,
                "{0}/{1} consumed data stream files in trace archive: count={0}, total={1}, archive-path=\"{2}\"",
                msgIter.archiveConsumedDsfCount[archivePath], msgIter.heapEntries.size(),
                archivePath);

            /* Check if we need to delete the archive directory */
            if (comp.removeConsumedArchive &&
                msgIter.archiveConsumedDsfCount[archivePath] == msgIter.heapEntries.size()) {
                BT_CPPLOGI_SPEC(msgIter.logger,
                                "Removing consumed trace archive directory: path=\"{}\"",
                                archivePath);
                removeDir(archivePath);
                msgIter.archiveConsumedDsfCount.erase(archivePath);
            }
        }

        BT_ASSERT(!entry.curMsg || !entry.curMsg->isStreamBeginning());
    }

    if (!entry.msgIter) {
        /* No common message iterator: try to create one */
        BT_ASSERT(!entry.curMsg);
        BT_ASSERT(!entry.curMsgTs);

        /*
         * Try to find a new data stream file info for this data stream
         * file group.
         */
        tryAddNewDsFileInfos(comp, msgIter);

        if (entry.dsfGroup->dsfInfos.empty()) {
            /* Cannot heal now! */
            return;
        }

        /* Create a new common message iterator */
        entry.msgIter = createMsgIter(comp, msgIter, *entry.dsfGroup, entry.dsfGroup->curDsfInfo());

        /* Set the first message */
        entry.nextMsg();

        /* First message should be a stream beginning message without a timestamp */
        BT_ASSERT(entry.curMsg);
        BT_ASSERT(entry.curMsg->isStreamBeginning());
        BT_ASSERT(!entry.curMsgTs);

        if (skipStreamBegin) {
            entry.nextMsg();
            BT_ASSERT(entry.curMsg);
            BT_ASSERT(entry.curMsg->isPacketBeginning());
        }
    }
}

/*
 * Create the heap entries, adding them to `msgIter.heapEntries` and
 * immediately inserting them into `msgIter.heap`.
 *
 * `msgIter.sickHeapEntry` remains `nullptr` at this point.
 */
void createHeapEntries(ctf_la_component& comp, ctf_la_msg_iter_data& msgIter)
{
    /* For each data stream file group */
    for (auto& dataStreamClsMapPair : msgIter.dsfGroups) {
        for (auto& dataStreamIdDsfGroupPair : dataStreamClsMapPair.second) {
            /* Create an empty (sick) heap entry */
            auto heapEntry = bt2s::make_unique<HeapEntry>(dataStreamIdDsfGroupPair.second);

            /*
             * Heal it immediately.
             *
             * This works because at this point we have at least one
             * complete trace archive.
             */
            tryHealHeapEntry(comp, msgIter, *heapEntry, false);

            /* Everything should be set now */
            BT_ASSERT(!heapEntry->dsfGroup->dsfInfos.empty());
            BT_ASSERT(heapEntry->msgIter);
            BT_ASSERT(heapEntry->curMsg);
            BT_ASSERT(heapEntry->curMsg->isStreamBeginning());

            /* Move to owning vector */
            msgIter.heapEntries.emplace_back(std::move(heapEntry));

            /* Insert pointer into heap */
            msgIter.heap.insert(msgIter.heapEntries.back().get());
        }
    }
}

} /* Namespace */

bt_message_iterator_class_next_method_status
ctf_la_iterator_next(bt_self_message_iterator * const self, bt_message_array_const libMsgs,
                     uint64_t, uint64_t *count)
{
    ctf_la_msg_iter_data& msgIter =
        *static_cast<ctf_la_msg_iter_data *>(bt_self_message_iterator_get_data(self));
    ctf_la_component& comp = *static_cast<ctf_la_component *>(
        bt_self_component_get_data(bt_self_message_iterator_borrow_component(self)));

    try {
        /* State machine loop */
        while (true) {
            switch (msgIter.state) {
            case MsgIterState::Init:
                msgIter.state = MsgIterState::WaitForSessionToExist;
                break;

            case MsgIterState::WaitForSessionToExist:
                if (waitForSessionToExist(comp, msgIter)) {
                    msgIter.state = MsgIterState::TryCreateFirstArchiveTrace;
                } else {
                    /* No recording session yet */
                    return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
                }

                break;

            case MsgIterState::TryCreateFirstArchiveTrace:
                tryCreateFirstArchiveTrace(comp, msgIter,
                                           bt_self_message_iterator_borrow_component(self));

                if (comp.firstArchiveTrace) {
                    /*
                     * Found it!
                     *
                     * Create the data stream file groups and the
                     * corresponding heap entries.
                     */
                    createDsFileGroups(comp, msgIter);
                    createHeapEntries(comp, msgIter);
                    msgIter.state = MsgIterState::Work;
                    break;
                }

                /* No initial trace yet */
                return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;

            case MsgIterState::Work:
            {
                if (msgIter.sickHeapEntry) {
                    /* There's a sick heap entry to heal */
                    tryHealHeapEntry(comp, msgIter, *msgIter.sickHeapEntry, true);

                    if (msgIter.sickHeapEntry->curMsg) {
                        /* Healed: back to the heap */
                        msgIter.heap.insert(msgIter.sickHeapEntry);
                        msgIter.sickHeapEntry = nullptr;
                    } else {
                        /* Still sick: is there still a recording session? */
                        if (sessionInfo(comp)) {
                            /* Yes: cannot heal now */
                            BT_CPPLOGI_SPEC(
                                msgIter.logger,
                                "No data stream file exists yet: archive-dir=\"{}\", stream-cls-id={}, stream-id={}",
                                comp.archivesPath,
                                msgIter.sickHeapEntry->dsfGroup->dataStreamCls->id(),
                                msgIter.sickHeapEntry->dsfGroup->stream_id);
                            return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
                        } else {
                            /* No */
                            if (msgIter.waitedForNoMoreCurrentTraceChunk) {
                                /* Append stream end message for this stream */
                                auto msgs = bt2::ConstMessageArray::wrapEmpty(libMsgs, 1);

                                msgs.append(bt2::wrap(self).createStreamEndMessage(
                                    *msgIter.sickHeapEntry->dsfGroup->stream));
                                *count = msgs.release();

                                /* Not sick anymore */
                                msgIter.sickHeapEntry = nullptr;

                                if (msgIter.heap.isEmpty()) {
                                    /* Heap is now empty: end of iteration next */
                                    msgIter.state = MsgIterState::End;
                                }

                                return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
                            } else {
                                /* Wait for no more current trace chunk */
                                msgIter.state = MsgIterState::WaitForNoMoreCurrentTraceChunk;
                                break;
                            }
                        }
                    }
                }

                /* Emit current oldest message from heap */
                BT_ASSERT(!msgIter.sickHeapEntry);

                const auto top = msgIter.heap.top();

                msgIter.heap.removeTop();

                {
                    auto msgs = bt2::ConstMessageArray::wrapEmpty(libMsgs, 1);

                    msgs.append(top->curMsg);
                    *count = msgs.release();
                }

                /* This entry is sick now */
                top->curMsg.reset();
                top->curMsgTs.reset();
                msgIter.sickHeapEntry = top;
                return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
            }

            case MsgIterState::WaitForNoMoreCurrentTraceChunk:
                BT_CPPLOGI_SPEC(
                    msgIter.logger,
                    "Recording session was destroyed: waiting for : archive-dir=\"{}\", stream-cls-id={}, stream-id={}",
                    comp.archivesPath, msgIter.sickHeapEntry->dsfGroup->dataStreamCls->id(),
                    msgIter.sickHeapEntry->dsfGroup->stream_id);

                {
                    auto dirCount = 0U;

                    for (auto& entry : dirEntries(comp.outputDirPath)) {
                        if (entry.name != "archives" && entry.isDir) {
                            ++dirCount;
                        }
                    }

                    if (dirCount > 0) {
                        /*
                         * Any directory other than `archives` is
                         * considered a current trace chunk, which means
                         * it _will_ become an archive... soon.
                         */
                        return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
                    }
                }

                /*
                 * No more current trace chunk: try to work again.
                 *
                 * If there was a current trace chunk which became an
                 * archive, then the tryHealHeapEntry() call will handle
                 * it within the `MsgIterState::Work` state.
                 */
                msgIter.waitedForNoMoreCurrentTraceChunk = true;
                msgIter.state = MsgIterState::Work;
                break;

            case MsgIterState::End:
                return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
            }
        }
    } catch (const bt2::Error&) {
        return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
    }

    return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
}

void ctf_la_iterator_finalize(bt_self_message_iterator *it)
{
    ctf_la_msg_iter_data::UP {
        static_cast<ctf_la_msg_iter_data *>(bt_self_message_iterator_get_data(it))};
}

bt_message_iterator_class_initialize_method_status
ctf_la_iterator_init(bt_self_message_iterator *self_msg_iter,
                     bt_self_message_iterator_configuration *, bt_self_component_port_output *)
{
    try {
        auto msg_iter_data = bt2s::make_unique<ctf_la_msg_iter_data>(self_msg_iter);

        bt_self_message_iterator_set_data(self_msg_iter, msg_iter_data.release());
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}

void ctf_la_finalize(bt_self_component_source *component)
{
    ctf_la_component::UP {static_cast<ctf_la_component *>(
        bt_self_component_get_data(bt_self_component_source_as_self_component(component)))};
}

static bt_param_validation_map_value_entry_descr la_params_entries_descr[] = {
    {"session-name", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
     bt_param_validation_value_descr::makeString()},
    {"remove-consumed-archive", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"session-found-file-path", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString()},
    {"trace-name", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString()},
    {"clock-class-offset-s", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeSignedInteger()},
    {"clock-class-offset-ns", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeSignedInteger()},
    {"force-clock-class-origin-unix-epoch", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

ctf::src::la::Parameters read_src_la_parameters(const bt2::ConstMapValue params,
                                                const bt2c::Logger& logger)
{
    gchar *error = NULL;
    bt_param_validation_status validate_value_status =
        bt_param_validation_validate(params.libObjPtr(), la_params_entries_descr, &error);

    if (validate_value_status != BT_PARAM_VALIDATION_STATUS_OK) {
        bt2c::GCharUP errorFreer {error};
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2c::Error, "{}", error);
    }

    ctf::src::la::Parameters parameters {params["session-name"]->asString().value()};

    /* remove-consumed-archive parameter */
    if (const auto removeConsumedArchive = params["remove-consumed-archive"]) {
        parameters.removeConsumedArchive = removeConsumedArchive->asBool().value();
    }

    /* session-found-file-path parameter */
    if (const auto sessionFoundFilePath = params["session-found-file-path"]) {
        parameters.sessionFoundFilePath = sessionFoundFilePath->asString().value().str();
    }

    /* clock-class-offset-s parameter */
    if (const auto clockClassOffsetS = params["clock-class-offset-s"]) {
        parameters.clkClsCfg.offsetSec = clockClassOffsetS->asSignedInteger().value();
    }

    /* clock-class-offset-ns parameter */
    if (const auto clockClassOffsetNs = params["clock-class-offset-ns"]) {
        parameters.clkClsCfg.offsetNanoSec = clockClassOffsetNs->asSignedInteger().value();
    }

    /* force-clock-class-origin-unix-epoch parameter */
    if (const auto forceClockClassOriginUnixEpoch = params["force-clock-class-origin-unix-epoch"]) {
        parameters.clkClsCfg.forceOriginIsUnixEpoch =
            forceClockClassOriginUnixEpoch->asBool().value();
    }

    /* trace-name parameter */
    if (const auto traceName = params["trace-name"]) {
        parameters.traceName = traceName->asString().value().str();
    }

    return parameters;
}

static ctf_la_component::UP ctf_la_create(const bt2::ConstMapValue params,
                                          bt_self_component_source *self_comp_src)
{
    const bt2c::Logger logger {bt2::SelfSourceComponent {self_comp_src},
                               "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/COMP"};
    const auto parameters = read_src_la_parameters(params, logger);
    auto ctf_la = bt2s::make_unique<ctf_la_component>(parameters.clkClsCfg, parameters.sessionName,
                                                      parameters.removeConsumedArchive,
                                                      parameters.traceName, parameters.sessionFoundFilePath, logger);

    if (bt_self_component_source_add_output_port(self_comp_src, "out", nullptr, nullptr)) {
        return nullptr;
    }

    return ctf_la;
}

bt_component_class_initialize_method_status ctf_la_init(bt_self_component_source *self_comp_src,
                                                        bt_self_component_source_configuration *,
                                                        const bt_value *params, void *)
{
    try {
        ctf_la_component::UP ctf_la = ctf_la_create(bt2::ConstMapValue {params}, self_comp_src);
        if (!ctf_la) {
            return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        bt_self_component_set_data(bt_self_component_source_as_self_component(self_comp_src),
                                   ctf_la.release());
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
    } catch (const bt2::Error&) {
        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
    }
}
