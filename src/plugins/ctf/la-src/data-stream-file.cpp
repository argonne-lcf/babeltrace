/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include "compat/endian.h" /* IWYU pragma: keep  */
#include "compat/mman.h"   /* IWYU: pragma keep  */
#include "cpp-common/bt2c/glib-up.hpp"
#include "cpp-common/bt2s/make-unique.hpp"
#include "cpp-common/vendor/fmt/format.h"

#include "../common/src/pkt-props.hpp"
#include "data-stream-file.hpp"
#include "file.hpp"
#include "lttng-index.hpp"

using namespace bt2c::literals::datalen;

static bt2c::DataLen getFileSize(const char * const path, const bt2c::Logger& logger)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2::Error,
                                                     "Failed to stat stream file", "path={}", path);
    }

    return bt2c::DataLen::fromBytes(st.st_size);
}

ctf_la_ds_file_info::ctf_la_ds_file_info(std::string archivePathParam, std::string pathParam,
                                         ctf_la_ds_index indexParam,
                                         const bt2c::Logger& parentLogger) :
    logger {parentLogger, "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/DS-FILE-INFO"},
    archivePath {std::move(archivePathParam)}, path(std::move(pathParam)),
    size(getFileSize(path.c_str(), logger)), index {std::move(indexParam)}
{
}

/*
 * Return true if `offset_in_file` is in the current mapping.
 */

static bool offset_ist_mapped(struct ctf_la_ds_file *ds_file, off_t offset_in_file)
{
    if (!ds_file->mmap_addr)
        return false;

    return offset_in_file >= ds_file->mmap_offset_in_file &&
           offset_in_file < (ds_file->mmap_offset_in_file + ds_file->mmap_len);
}

enum ds_file_status
{
    DS_FILE_STATUS_OK = 0,
    DS_FILE_STATUS_ERROR = -1,
    DS_FILE_STATUS_EOF = 1,
};

static ds_file_status ds_file_munmap(struct ctf_la_ds_file *ds_file)
{
    BT_ASSERT(ds_file);

    if (!ds_file->mmap_addr) {
        return DS_FILE_STATUS_OK;
    }

    if (bt_munmap(ds_file->mmap_addr, ds_file->mmap_len)) {
        BT_CPPLOGE_ERRNO_SPEC(ds_file->logger, "Cannot memory-unmap file",
                              ": address={}, size={}, file_path=\"{}\", file={}",
                              fmt::ptr(ds_file->mmap_addr), ds_file->mmap_len,
                              ds_file->file ? ds_file->file->path : "NULL",
                              ds_file->file ? fmt::ptr(ds_file->file->fp) : nullptr);
        return DS_FILE_STATUS_ERROR;
    }

    ds_file->mmap_addr = NULL;

    return DS_FILE_STATUS_OK;
}

/*
 * mmap a region of `ds_file` such that `requested_offset_in_file` is in the
 * mapping.  If the currently mmap-ed region already contains
 * `requested_offset_in_file`, the mapping is kept.
 *
 * `requested_offset_in_file` must be a valid offset in the file.
 */
static ds_file_status ds_file_mmap(struct ctf_la_ds_file *ds_file, off_t requested_offset_in_file)
{
    /* Ensure the requested offset is in the file range. */
    BT_ASSERT(requested_offset_in_file >= 0);
    BT_ASSERT(requested_offset_in_file < ds_file->file->size);

    /*
     * If the mapping already contains the requested range, we have nothing to
     * do.
     */
    if (offset_ist_mapped(ds_file, requested_offset_in_file)) {
        return DS_FILE_STATUS_OK;
    }

    /* Unmap old region */
    ds_file_status status = ds_file_munmap(ds_file);
    if (status != DS_FILE_STATUS_OK) {
        return status;
    }

    /*
     * Compute a mapping that has the required alignment properties and
     * contains `requested_offset_in_file`.
     */
    size_t alignment = bt_mmap_get_offset_align_size(static_cast<int>(ds_file->logger.level()));
    ds_file->mmap_offset_in_file =
        requested_offset_in_file - (requested_offset_in_file % alignment);
    ds_file->mmap_len =
        MIN(ds_file->file->size - ds_file->mmap_offset_in_file, ds_file->mmap_max_len);

    BT_ASSERT(ds_file->mmap_len > 0);
    BT_ASSERT(requested_offset_in_file >= ds_file->mmap_offset_in_file);
    BT_ASSERT(requested_offset_in_file < (ds_file->mmap_offset_in_file + ds_file->mmap_len));

    ds_file->mmap_addr =
        bt_mmap(ds_file->mmap_len, PROT_READ, MAP_PRIVATE, fileno(ds_file->file->fp.get()),
                ds_file->mmap_offset_in_file, static_cast<int>(ds_file->logger.level()));
    if (ds_file->mmap_addr == MAP_FAILED) {
        BT_CPPLOGE_SPEC(ds_file->logger,
                        "Cannot memory-map address (size {}) of file \"{}\" ({}) at offset {}: {}",
                        ds_file->mmap_len, ds_file->file->path, fmt::ptr(ds_file->file->fp),
                        (intmax_t) ds_file->mmap_offset_in_file, strerror(errno));
        return DS_FILE_STATUS_ERROR;
    }

    return DS_FILE_STATUS_OK;
}

void ctf_la_ds_index::updateOffsetsInStream()
{
    auto offsetInStream = 0_bytes;

    for (ctf_la_ds_index_entry& entry : this->entries) {
        entry.offsetInStream = offsetInStream;
        offsetInStream += entry.packetSize;
    }
}

static int convert_cycles_to_ns(const ctf::src::ClkCls& clockClass, uint64_t cycles, int64_t *ns)
{
    return bt_util_clock_cycles_to_ns_from_origin(cycles, clockClass.freq(),
                                                  clockClass.offsetFromOrigin().seconds(),
                                                  clockClass.offsetFromOrigin().cycles(), ns);
}

static bt2s::optional<ctf_la_ds_index> build_index_from_idx_file(const std::string& path,
                                                                 const ctf::src::TraceCls& traceCls,
                                                                 const bt2c::Logger& logger)
{
    const auto dsfSize = getFileSize(path.c_str(), logger);

    BT_CPPLOGI_SPEC(logger, "Building index from .idx file of stream file {}", path);

    /* Look for index file in relative path index/name.idx. */
    bt2c::GCharUP basename {g_path_get_basename(path.c_str())};
    if (!basename) {
        BT_CPPLOGE_SPEC(logger, "Cannot get the basename of datastream file {}", path);
        return bt2s::nullopt;
    }

    bt2c::GCharUP directory {g_path_get_dirname(path.c_str())};
    if (!directory) {
        BT_CPPLOGE_SPEC(logger, "Cannot get dirname of datastream file {}", path);
        return bt2s::nullopt;
    }

    std::string index_basename = fmt::format("{}.idx", basename.get());
    bt2c::GCharUP index_file_path {
        g_build_filename(directory.get(), "index", index_basename.c_str(), NULL)};
    bt2c::GMappedFileUP mapped_file {g_mapped_file_new(index_file_path.get(), FALSE, NULL)};
    if (!mapped_file) {
        BT_CPPLOGD_SPEC(logger, "Cannot create new mapped file {}", index_file_path.get());
        return bt2s::nullopt;
    }

    /*
     * The g_mapped_file API limits us to 4GB files on 32-bit.
     * Traces with such large indexes have never been seen in the wild,
     * but this would need to be adjusted to support them.
     */
    gsize filesize = g_mapped_file_get_length(mapped_file.get());
    if (filesize < sizeof(ctf_packet_index_file_hdr)) {
        BT_CPPLOGW_SPEC(logger,
                        "Invalid LTTng trace index file: "
                        "file size ({} bytes) < header size ({} bytes)",
                        filesize, sizeof(ctf_packet_index_file_hdr));
        return bt2s::nullopt;
    }

    const char *mmap_begin = g_mapped_file_get_contents(mapped_file.get());
    const ctf_packet_index_file_hdr *header = (const ctf_packet_index_file_hdr *) mmap_begin;

    const char *file_pos = g_mapped_file_get_contents(mapped_file.get()) + sizeof(*header);
    if (be32toh(header->magic) != CTF_INDEX_MAGIC) {
        BT_CPPLOGW_SPEC(logger, "Invalid LTTng trace index: \"magic\" field validation failed");
        return bt2s::nullopt;
    }

    uint32_t version_major = be32toh(header->index_major);
    uint32_t version_minor = be32toh(header->index_minor);
    if (version_major != 1) {
        BT_CPPLOGW_SPEC(logger, "Unknown LTTng trace index version: major={}, minor={}",
                        version_major, version_minor);
        return bt2s::nullopt;
    }

    size_t file_index_entry_size = be32toh(header->packet_index_len);
    if (file_index_entry_size < CTF_INDEX_1_0_SIZE) {
        BT_CPPLOGW_SPEC(
            logger,
            "Invalid `packet_index_len` in LTTng trace index file (`packet_index_len` < CTF index 1.0 index entry size): "
            "packet_index_len={}, CTF_INDEX_1_0_SIZE={}",
            file_index_entry_size, CTF_INDEX_1_0_SIZE);
        return bt2s::nullopt;
    }

    size_t file_entry_count = (filesize - sizeof(*header)) / file_index_entry_size;
    if ((filesize - sizeof(*header)) % file_index_entry_size) {
        BT_CPPLOGW_SPEC(logger,
                        "Invalid LTTng trace index: the index's size after the header "
                        "({} bytes) is not a multiple of the index entry size "
                        "({} bytes)",
                        (filesize - sizeof(*header)), sizeof(*header));
        return bt2s::nullopt;
    }

    /*
     * We need the clock class to convert cycles to ns.  For that, we need the
     * stream class.  Read the stream class id from the first packet's header.
     * We don't know the size of that packet yet, so pretend that it spans the
     * whole file (the reader will only read the header anyway).
     */
    ctf_la_ds_index_entry tempIndexEntry {path.c_str(), 0_bits, dsfSize};
    ctf_la_ds_index tempIndex;
    tempIndex.entries.emplace_back(tempIndexEntry);

    ctf::src::la::Medium::UP medium = bt2s::make_unique<ctf::src::la::Medium>(tempIndex, logger);
    ctf::src::PktProps props = ctf::src::readPktProps(traceCls, std::move(medium), 0_bytes, logger);

    const ctf::src::DataStreamCls *sc = props.dataStreamCls;
    BT_ASSERT(sc);
    if (!sc->defClkCls()) {
        BT_CPPLOGI_SPEC(logger, "Cannot find stream class's default clock class.");
        return bt2s::nullopt;
    }

    ctf_la_ds_index index;
    ctf_la_ds_index_entry *prev_index_entry = nullptr;
    auto totalPacketsSize = 0_bytes;

    for (size_t i = 0; i < file_entry_count; i++) {
        struct ctf_packet_index *file_index = (struct ctf_packet_index *) file_pos;
        const auto packetSize = bt2c::DataLen::fromBits(be64toh(file_index->packet_size));

        if (packetSize.hasExtraBits()) {
            BT_CPPLOGW_SPEC(logger, "Invalid packet size encountered in LTTng trace index file");
            return bt2s::nullopt;
        }

        const auto offset = bt2c::DataLen::fromBytes(be64toh(file_index->offset));

        if (i != 0 && offset < prev_index_entry->offsetInFile) {
            BT_CPPLOGW_SPEC(
                logger,
                "Invalid, non-monotonic, packet offset encountered in LTTng trace index file: "
                "previous offset={} bytes, current offset={} bytes",
                prev_index_entry->offsetInFile.bytes(), offset.bytes());
            return bt2s::nullopt;
        }

        ctf_la_ds_index_entry index_entry {path, offset, packetSize};
        index_entry.timestamp_begin = be64toh(file_index->timestamp_begin);
        index_entry.timestamp_end = be64toh(file_index->timestamp_end);
        if (index_entry.timestamp_end < index_entry.timestamp_begin) {
            BT_CPPLOGW_SPEC(
                logger,
                "Invalid packet time bounds encountered in LTTng trace index file (begin > end): "
                "timestamp_begin={}, timestamp_end={}",
                index_entry.timestamp_begin, index_entry.timestamp_end);
            return bt2s::nullopt;
        }

        /* Convert the packet's bound to nanoseconds since Epoch. */
        int ret = convert_cycles_to_ns(*sc->defClkCls(), index_entry.timestamp_begin,
                                       &index_entry.timestamp_begin_ns);
        if (ret) {
            BT_CPPLOGI_SPEC(
                logger,
                "Failed to convert raw timestamp to nanoseconds since Epoch during index parsing");
            return bt2s::nullopt;
        }
        ret = convert_cycles_to_ns(*sc->defClkCls(), index_entry.timestamp_end,
                                   &index_entry.timestamp_end_ns);
        if (ret) {
            BT_CPPLOGI_SPEC(
                logger,
                "Failed to convert raw timestamp to nanoseconds since Epoch during LTTng trace index parsing");
            return bt2s::nullopt;
        }

        if (version_minor >= 1) {
            index_entry.packet_seq_num = be64toh(file_index->packet_seq_num);
        }

        totalPacketsSize += packetSize;
        file_pos += file_index_entry_size;

        index.entries.emplace_back(index_entry);

        prev_index_entry = &index.entries.back();
    }

    /* Validate that the index addresses the complete stream. */
    if (dsfSize != totalPacketsSize) {
        BT_CPPLOGW_SPEC(logger,
                        "Invalid LTTng trace index file; indexed size != stream file size: "
                        "stream-file-size-bytes={}, total-packets-size-bytes={}",
                        dsfSize.bytes(), totalPacketsSize.bytes());
        return bt2s::nullopt;
    }

    return index;
}

ctf_la_ds_file::UP ctf_la_ds_file_create(std::string path, const bt2c::Logger& parentLogger)
{
    const auto offset_align = bt_mmap_get_offset_align_size(static_cast<int>(parentLogger.level()));
    auto ds_file = bt2s::make_unique<ctf_la_ds_file>(parentLogger, offset_align * 2048);

    ds_file->file = bt2s::make_unique<ctf_la_file>(ds_file->logger);
    ds_file->file->path = path;
    int ret = ctf_la_file_open(ds_file->file.get(), "rb");
    if (ret) {
        return nullptr;
    }

    return ds_file;
}

namespace ctf {
namespace src {
namespace la {

Medium::Medium(ctf_la_ds_index index, const bt2c::Logger& parentLogger) :
    _mIndex {std::move(index)}, _mLogger {parentLogger, "PLUGIN/SRC.CTF.LTTNG-ARCHIVE/DS-MEDIUM"}
{
    BT_ASSERT(!_mIndex.entries.empty());
}

ctf_la_ds_index::EntriesT::const_iterator
Medium::_mFindIndexEntryForOffset(bt2c::DataLen offsetInStream) const noexcept
{
    return std::lower_bound(
        _mIndex.entries.begin(), _mIndex.entries.end(), offsetInStream,
        [](const ctf_la_ds_index_entry& entry, bt2c::DataLen offsetInStreamLambda) {
            return (entry.offsetInStream + entry.packetSize - 1_bytes) < offsetInStreamLambda;
        });
}

ctf::src::Buf Medium::buf(const bt2c::DataLen requestedOffsetInStream, const bt2c::DataLen minSize)
{
    BT_CPPLOGD("buf called: offset-bytes={}, min-size-bytes={}", requestedOffsetInStream.bytes(),
               minSize.bytes());

    /* The medium only gets asked about whole byte offsets and min sizes. */
    BT_ASSERT_DBG(requestedOffsetInStream.extraBitCount() == 0);
    BT_ASSERT_DBG(minSize.extraBitCount() == 0);

    /*
     *  +-file 1-----+  +-file 2-----+------------+------------+
     *  |            |  |            |            |            |
     *  | packet 1   |  | packet 2   | packet 3   | packet 4   |
     *  |            |  |            |            |            |
     *  +------------+  +------------+------v-----+------------+
     *  ^-----------------------------------^               requestedOffsetInStream (passed as parameter)
     *  ^----------------------------^                      indexEntry.offsetInStream (known)
     *                  ^------------^                      indexEntry.offsetInFile (known)
     *  ^---------------^                                   fileStartInStream (computed)
     *                  ^-------------------^               requestedOffsetInFile (computed)
     *
     * Then, assuming mmap maps this region:
     *                                      ^------------^
     *
     *                  ^-------------------^               startOfMappingInFile
     *                  ^----------------^                  requestedOffsetInMapping
     *                  ^--------------------------------^  endOfMappingInFile
     */
    const ctf_la_ds_index::EntriesT::const_iterator indexEntryIt =
        this->_mFindIndexEntryForOffset(requestedOffsetInStream);
    if (indexEntryIt == _mIndex.entries.end()) {
        BT_CPPLOGD("no index entry containing this offset");
        throw NoData();
    }

    const ctf_la_ds_index_entry& indexEntry = *indexEntryIt;

    _mCurrentDsFile = ctf_la_ds_file_create(indexEntry.path, _mLogger);
    if (!_mCurrentDsFile) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2::Error, "Failed to create ctf_la_ds_file");
    }

    const auto fileStartInStream = indexEntry.offsetInStream - indexEntry.offsetInFile;
    const auto requestedOffsetInFile = requestedOffsetInStream - fileStartInStream;

    ds_file_status status = ds_file_mmap(_mCurrentDsFile.get(), requestedOffsetInFile.bytes());
    if (status != DS_FILE_STATUS_OK) {
        throw bt2::Error("Failed to mmap file");
    }

    const auto startOfMappingInFile =
        bt2c::DataLen::fromBytes(_mCurrentDsFile->mmap_offset_in_file);
    const auto requestedOffsetInMapping = requestedOffsetInFile - startOfMappingInFile;
    const auto exclEndOfMappingInFile =
        startOfMappingInFile + bt2c::DataLen::fromBytes(_mCurrentDsFile->mmap_len);

    /*
     * Find where to end the mapping.  We can map the following entries as long as
     *
     *  1) there are following entries
     *  2) they are located in the same file as our starting entry
     *  3) they are (at least partially) within the mapping
     */

    ctf_la_ds_index::EntriesT::const_iterator endIndexEntryIt = indexEntryIt;
    while (true) {
        ctf_la_ds_index::EntriesT::const_iterator nextIndexEntryIt = endIndexEntryIt + 1;

        if (nextIndexEntryIt == _mIndex.entries.end()) {
            break;
        }

        if (nextIndexEntryIt->path != indexEntryIt->path) {
            break;
        }

        if (nextIndexEntryIt->offsetInFile >= exclEndOfMappingInFile) {
            break;
        }

        endIndexEntryIt = nextIndexEntryIt;
    }

    /*
     * It's possible the mapping ends in the middle of our end entry.  Choose
     * the end of the mapping or the end of the end entry, whichever comes
     * first, as the end of the returned buffer.
     */
    const auto exclEndOfEndEntryInFile =
        endIndexEntryIt->offsetInFile + endIndexEntryIt->packetSize;
    const auto bufEndInFile = std::min(exclEndOfMappingInFile, exclEndOfEndEntryInFile);
    const auto bufLen = bufEndInFile - requestedOffsetInFile;
    const uint8_t *bufStart =
        (const uint8_t *) _mCurrentDsFile->mmap_addr + requestedOffsetInMapping.bytes();

    if (bufLen < minSize) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
            bt2::Error,
            "Insufficient data in file to fulfill request: path=\"{}\", requested-offset-in-file-bytes={}, "
            "remaining-data-len-in-file-bytes={}, min-size-bytes={}",
            indexEntry.path, requestedOffsetInFile.bytes(), bufLen.bytes(), minSize.bytes());
    }

    ctf::src::Buf buf {bufStart, bufLen};

    BT_CPPLOGD("CtfLaMedium::buf returns: buf-addr={}, buf-size-bytes={}\n", fmt::ptr(buf.addr()),
               buf.size().bytes());

    return buf;
}

} /* namespace la */
} /* namespace src */
} /* namespace ctf */

bt2s::optional<ctf_la_ds_index> ctf_la_ds_file_build_index(const std::string& path,
                                                           const ctf::src::TraceCls& traceCls,
                                                           const bt2c::Logger& logger)
{
    auto index = build_index_from_idx_file(path, traceCls, logger);

    BT_ASSERT(index);
    return index;
}

ctf_la_ds_file::~ctf_la_ds_file()
{
    (void) ds_file_munmap(this);
}
