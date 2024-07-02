/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_LA_SRC_DATA_STREAM_FILE_HPP
#define BABELTRACE_PLUGINS_CTF_LA_SRC_DATA_STREAM_FILE_HPP

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <glib.h>
#include <stdio.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/trace-ir.hpp"
#include "cpp-common/bt2c/data-len.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../common/src/item-seq/medium.hpp"
#include "../common/src/metadata/ctf-ir.hpp"
#include "file.hpp"

struct ctf_la_ds_index_entry
{
    ctf_la_ds_index_entry(std::string pathParam, const bt2c::DataLen offsetInFileParam,
                          const bt2c::DataLen packetSizeParam) :
        path {std::move(pathParam)},
        offsetInFile {offsetInFileParam}, offsetInStream {offsetInFileParam},
        packetSize {packetSizeParam}
    {
    }

    /* Weak, belongs to ctf_la_ds_file_info. */
    std::string path;

    /* Position of the packet from the beginning of the file. */
    bt2c::DataLen offsetInFile;

    /*
     * Position of the packet from the beginning of the stream.  Starts equal
     * to `offsetInFile`, but can change when multiple data stream files
     * belonging to the same stream are merged.
     */
    bt2c::DataLen offsetInStream;

    /* Size of the packet. */
    bt2c::DataLen packetSize;

    /*
     * Extracted from the packet context, relative to the respective fields'
     * mapped clock classes (in cycles).
     */
    uint64_t timestamp_begin = 0, timestamp_end = 0;

    /*
     * Converted from the packet context, relative to the trace's EPOCH
     * (in ns since EPOCH).
     */
    int64_t timestamp_begin_ns = 0, timestamp_end_ns = 0;

    /*
     * Packet sequence number, or UINT64_MAX if not present in the index.
     */
    uint64_t packet_seq_num = UINT64_MAX;
};

struct ctf_la_ds_index
{
    using EntriesT = std::vector<ctf_la_ds_index_entry>;

    EntriesT entries;

    void updateOffsetsInStream();
};

struct ctf_la_ds_file_info
{
    using UP = std::unique_ptr<ctf_la_ds_file_info>;

    ctf_la_ds_file_info(std::string archivePathParam, std::string pathParam,
                        ctf_la_ds_index indexParam, const bt2c::Logger& parentLogger);

    bt2c::Logger logger;
    std::string archivePath;
    std::string path;
    bt2c::DataLen size;

    ctf_la_ds_index index;
};

struct ctf_la_ds_file
{
    using UP = std::unique_ptr<ctf_la_ds_file>;

    explicit ctf_la_ds_file(const bt2c::Logger& parentLogger, const size_t mmapMaxLenParam) :
        logger {parentLogger, "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/DS"}, mmap_max_len {mmapMaxLenParam}
    {
    }

    ctf_la_ds_file(const ctf_la_ds_file&) = delete;
    ctf_la_ds_file& operator=(const ctf_la_ds_file&) = delete;
    ~ctf_la_ds_file();

    bt2c::Logger logger;

    ctf_la_file::UP file;

    void *mmap_addr = nullptr;

    /*
     * Max length of chunk to mmap() when updating the current mapping.
     * This value must be page-aligned.
     */
    size_t mmap_max_len = 0;

    /* Length of the current mapping. Never exceeds the file's length. */
    size_t mmap_len = 0;

    /* Offset in the file where the current mapping starts. */
    off_t mmap_offset_in_file = 0;
};

struct ctf_la_ds_file_group
{
    using UP = std::unique_ptr<ctf_la_ds_file_group>;

    explicit ctf_la_ds_file_group(const ctf::src::DataStreamCls& dataStreamClsParam,
                                  const uint64_t streamInstanceId) noexcept :
        dataStreamCls(&dataStreamClsParam),
        stream_id(streamInstanceId)
    {
    }

    /*
     * Queue of data stream file infos to consume.
     *
     * The first element is the current one.
     */
    std::queue<ctf_la_ds_file_info> dsfInfos;

    ctf_la_ds_file_info& curDsfInfo() noexcept
    {
        BT_ASSERT(!dsfInfos.empty());
        return dsfInfos.front();
    }

    const ctf::src::DataStreamCls *dataStreamCls;
    bt2::Stream::Shared stream;

    /* Stream (instance) ID; -1ULL means none */
    uint64_t stream_id = 0;
};

ctf_la_ds_file::UP ctf_la_ds_file_create(std::string path, const bt2c::Logger& parentLogger);

bt2s::optional<ctf_la_ds_index> ctf_la_ds_file_build_index(const std::string& path,
                                                           const ctf::src::TraceCls& traceCls,
                                                           const bt2c::Logger& logger);

namespace ctf {
namespace src {
namespace la {

struct Medium : public ctf::src::Medium
{
    explicit Medium(ctf_la_ds_index index, const bt2c::Logger& parentLogger);

    ~Medium() = default;
    Medium(const Medium&) = delete;
    Medium& operator=(const Medium&) = delete;

    ctf::src::Buf buf(bt2c::DataLen offset, bt2c::DataLen minSize) override;

private:
    ctf_la_ds_index::EntriesT::const_iterator
    _mFindIndexEntryForOffset(bt2c::DataLen offsetInStream) const noexcept;

    ctf_la_ds_index _mIndex;
    bt2c::Logger _mLogger;
    ctf_la_ds_file::UP _mCurrentDsFile;
};

} /* namespace la */
} /* namespace src */
} /* namespace ctf */

#endif /* BABELTRACE_PLUGINS_CTF_LA_SRC_DATA_STREAM_FILE_HPP */
