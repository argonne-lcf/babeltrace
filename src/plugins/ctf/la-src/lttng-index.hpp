/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013 Julien Desfossez <jdesfossez@efficios.com>
 * Copyright (C) 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright (C) 2013 David Goulet <dgoulet@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_LA_SRC_LTTNG_INDEX_HPP
#define BABELTRACE_PLUGINS_CTF_LA_SRC_LTTNG_INDEX_HPP

#include <cstdint>

#include "compat/limits.h" /* IWYU pragma: keep  */

#define CTF_INDEX_MAGIC    0xC1F1DCC1
#define CTF_INDEX_MAJOR    1
#define CTF_INDEX_MINOR    1
#define CTF_INDEX_1_0_SIZE offsetof(struct ctf_packet_index, stream_instance_id)

/*
 * Header at the beginning of each index file.
 * All integer fields are stored in big endian.
 */
struct ctf_packet_index_file_hdr
{
    uint32_t magic;
    uint32_t index_major;
    uint32_t index_minor;
    /* size of struct ctf_packet_index, in bytes. */
    uint32_t packet_index_len;
} __attribute__((__packed__));

/*
 * Packet index generated for each trace packet store in a trace file.
 * All integer fields are stored in big endian.
 */
struct ctf_packet_index
{
    uint64_t offset;       /* offset of the packet in the file, in bytes */
    uint64_t packet_size;  /* packet size, in bits */
    uint64_t content_size; /* content size, in bits */
    uint64_t timestamp_begin;
    uint64_t timestamp_end;
    uint64_t events_discarded;
    uint64_t stream_id;
    /* CTF_INDEX 1.0 limit */
    uint64_t stream_instance_id; /* ID of the channel instance */
    uint64_t packet_seq_num;     /* packet sequence number */
} __attribute__((__packed__));

#endif /* BABELTRACE_PLUGINS_CTF_LA_SRC_LTTNG_INDEX_HPP */
