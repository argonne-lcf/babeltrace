/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013 Julien Desfossez <jdesfossez@efficios.com>
 * Copyright (C) 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright (C) 2013 David Goulet <dgoulet@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_LTTNG_LIVE_LTTNG_VIEWER_ABI_HPP
#define BABELTRACE_PLUGINS_CTF_LTTNG_LIVE_LTTNG_VIEWER_ABI_HPP

#include <stdint.h>

#include "common/common.h"
#include "compat/limits.h"                /* IWYU pragma: keep  */
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

#define LTTNG_VIEWER_PATH_MAX      4096
#define LTTNG_VIEWER_NAME_MAX      255
#define LTTNG_VIEWER_HOST_NAME_MAX 64

/* Flags in reply to get_next_index and get_packet. */
enum
{
    /* New metadata is required to read this packet. */
    LTTNG_VIEWER_FLAG_NEW_METADATA = (1 << 0),
    /* New stream got added to the trace. */
    LTTNG_VIEWER_FLAG_NEW_STREAM = (1 << 1),
};

enum lttng_viewer_command
{
    LTTNG_VIEWER_CONNECT = 1,
    LTTNG_VIEWER_LIST_SESSIONS = 2,
    LTTNG_VIEWER_ATTACH_SESSION = 3,
    LTTNG_VIEWER_GET_NEXT_INDEX = 4,
    LTTNG_VIEWER_GET_PACKET = 5,
    LTTNG_VIEWER_GET_METADATA = 6,
    LTTNG_VIEWER_GET_NEW_STREAMS = 7,
    LTTNG_VIEWER_CREATE_SESSION = 8,
    LTTNG_VIEWER_DETACH_SESSION = 9,
};

inline const char *format_as(const lttng_viewer_command cmd) noexcept
{
    switch (cmd) {
    case LTTNG_VIEWER_CONNECT:
        return "CONNECT";

    case LTTNG_VIEWER_LIST_SESSIONS:
        return "LIST_SESSIONS";

    case LTTNG_VIEWER_ATTACH_SESSION:
        return "ATTACH_SESSION";

    case LTTNG_VIEWER_GET_NEXT_INDEX:
        return "GET_NEXT_INDEX";

    case LTTNG_VIEWER_GET_PACKET:
        return "GET_PACKET";

    case LTTNG_VIEWER_GET_METADATA:
        return "GET_METADATA";

    case LTTNG_VIEWER_GET_NEW_STREAMS:
        return "GET_NEW_STREAMS";

    case LTTNG_VIEWER_CREATE_SESSION:
        return "CREATE_SESSION";

    case LTTNG_VIEWER_DETACH_SESSION:
        return "DETACH_SESSION";
    }

    bt_common_abort();
}

enum lttng_viewer_attach_return_code
{
    LTTNG_VIEWER_ATTACH_OK = 1,         /* The attach command succeeded. */
    LTTNG_VIEWER_ATTACH_ALREADY = 2,    /* A viewer is already attached. */
    LTTNG_VIEWER_ATTACH_UNK = 3,        /* The session ID is unknown. */
    LTTNG_VIEWER_ATTACH_NOT_LIVE = 4,   /* The session is not live. */
    LTTNG_VIEWER_ATTACH_SEEK_ERR = 5,   /* Seek error. */
    LTTNG_VIEWER_ATTACH_NO_SESSION = 6, /* No viewer session created. */
};

enum lttng_viewer_detach_session_return_code
{
    LTTNG_VIEWER_DETACH_SESSION_OK = 1,
    LTTNG_VIEWER_DETACH_SESSION_UNK = 2,
    LTTNG_VIEWER_DETACH_SESSION_ERR = 3,
};

enum lttng_viewer_next_index_return_code
{
    LTTNG_VIEWER_INDEX_OK = 1,       /* Index is available. */
    LTTNG_VIEWER_INDEX_RETRY = 2,    /* Index not yet available. */
    LTTNG_VIEWER_INDEX_HUP = 3,      /* Index closed (trace destroyed). */
    LTTNG_VIEWER_INDEX_ERR = 4,      /* Unknow error. */
    LTTNG_VIEWER_INDEX_INACTIVE = 5, /* Inactive stream beacon. */
    LTTNG_VIEWER_INDEX_EOF = 6,      /* End of index file. */
};

inline const char *format_as(const lttng_viewer_next_index_return_code code) noexcept
{
    switch (code) {
    case LTTNG_VIEWER_INDEX_OK:
        return "INDEX_OK";

    case LTTNG_VIEWER_INDEX_RETRY:
        return "INDEX_RETRY";

    case LTTNG_VIEWER_INDEX_HUP:
        return "INDEX_HUP";

    case LTTNG_VIEWER_INDEX_ERR:
        return "INDEX_ERR";

    case LTTNG_VIEWER_INDEX_INACTIVE:
        return "INDEX_INACTIVE";

    case LTTNG_VIEWER_INDEX_EOF:
        return "INDEX_EOF";
    }

    bt_common_abort();
}

enum lttng_viewer_get_packet_return_code
{
    LTTNG_VIEWER_GET_PACKET_OK = 1,
    LTTNG_VIEWER_GET_PACKET_RETRY = 2,
    LTTNG_VIEWER_GET_PACKET_ERR = 3,
    LTTNG_VIEWER_GET_PACKET_EOF = 4,
};

inline const char *format_as(const lttng_viewer_get_packet_return_code code) noexcept
{
    switch (code) {
    case LTTNG_VIEWER_GET_PACKET_OK:
        return "GET_PACKET_OK";

    case LTTNG_VIEWER_GET_PACKET_RETRY:
        return "GET_PACKET_RETRY";

    case LTTNG_VIEWER_GET_PACKET_ERR:
        return "GET_PACKET_ERR";

    case LTTNG_VIEWER_GET_PACKET_EOF:
        return "GET_PACKET_EOF";
    }

    bt_common_abort();
};

enum lttng_viewer_get_metadata_return_code
{
    LTTNG_VIEWER_METADATA_OK = 1,
    LTTNG_VIEWER_NO_NEW_METADATA = 2,
    LTTNG_VIEWER_METADATA_ERR = 3,
};

enum lttng_viewer_connection_type
{
    LTTNG_VIEWER_CLIENT_COMMAND = 1,
    LTTNG_VIEWER_CLIENT_MESSAGE = 2,
};

enum lttng_viewer_seek
{
    /* Receive the trace packets from the beginning. */
    LTTNG_VIEWER_SEEK_BEGINNING = 1,
    /* Receive the trace packets from now. */
    LTTNG_VIEWER_SEEK_LAST = 2,
};

inline const char *format_as(const lttng_viewer_seek seek) noexcept
{
    switch (seek) {
    case LTTNG_VIEWER_SEEK_BEGINNING:
        return "SEEK_BEGINNING";

    case LTTNG_VIEWER_SEEK_LAST:
        return "SEEK_LAST";
    }

    bt_common_abort();
}

enum lttng_viewer_new_streams_return_code
{
    LTTNG_VIEWER_NEW_STREAMS_OK = 1,     /* If new streams are being sent. */
    LTTNG_VIEWER_NEW_STREAMS_NO_NEW = 2, /* If no new streams are available. */
    LTTNG_VIEWER_NEW_STREAMS_ERR = 3,    /* Error. */
    LTTNG_VIEWER_NEW_STREAMS_HUP = 4,    /* Session closed. */
};

enum lttng_viewer_create_session_return_code
{
    LTTNG_VIEWER_CREATE_SESSION_OK = 1,
    LTTNG_VIEWER_CREATE_SESSION_ERR = 2,
};

struct lttng_viewer_session_common
{
    uint64_t id;
    uint32_t live_timer;
    uint32_t clients;
    uint32_t streams;
} __attribute__((__packed__));

struct lttng_viewer_session_v2_4
{
    lttng_viewer_session_common common;

    char hostname[LTTNG_VIEWER_HOST_NAME_MAX];
    char session_name[LTTNG_VIEWER_NAME_MAX];
} __attribute__((__packed__));

struct lttng_viewer_session_v2_15
{
    lttng_viewer_session_common common;

    uint32_t hostname_len;
    uint32_t session_name_len;
    uint32_t trace_format;

    /* Hostname goes here (length: `hostname_len`) */
    /* Session name goes here (length: `session_name_len`) */
} __attribute__((__packed__));

enum lttng_live_trace_format
{
    LTTNG_LIVE_TRACE_FORMAT_CTF_V1_8 = 0,
    LTTNG_LIVE_TRACE_FORMAT_CTF_V2_0 = 1,
};

struct lttng_viewer_stream
{
    uint64_t id;
    uint64_t ctf_trace_id;
    uint32_t metadata_flag;
    char path_name[LTTNG_VIEWER_PATH_MAX];
    char channel_name[LTTNG_VIEWER_NAME_MAX];
} __attribute__((__packed__));

struct lttng_viewer_cmd
{
    uint64_t data_size;   /* data size following this header */
    uint32_t cmd;         /* enum lttcomm_relayd_command */
    uint32_t cmd_version; /* command version */
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_CONNECT payload.
 */
struct lttng_viewer_connect
{
    /* session ID assigned by the relay for command connections */
    uint64_t viewer_session_id;
    uint32_t major;
    uint32_t minor;
    uint32_t type; /* enum lttng_viewer_connection_type */
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_LIST_SESSIONS payload.
 */
struct lttng_viewer_list_sessions
{
    uint32_t sessions_count;
    char session_list[]; /* struct lttng_viewer_session */
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_ATTACH_SESSION payload.
 */
struct lttng_viewer_attach_session_request
{
    uint64_t session_id;
    uint64_t offset; /* unused for now */
    uint32_t seek;   /* enum lttng_viewer_seek */
} __attribute__((__packed__));

struct lttng_viewer_attach_session_response
{
    /* enum lttng_viewer_attach_return_code */
    uint32_t status;
    uint32_t streams_count;
    /* struct lttng_viewer_stream */
    char stream_list[];
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_GET_NEXT_INDEX payload.
 */
struct lttng_viewer_get_next_index
{
    uint64_t stream_id;
} __attribute__((__packed__));

struct lttng_viewer_index
{
    uint64_t offset;
    uint64_t packet_size;
    uint64_t content_size;
    uint64_t timestamp_begin;
    uint64_t timestamp_end;
    uint64_t events_discarded;
    uint64_t stream_id;
    uint32_t status; /* enum lttng_viewer_next_index_return_code */
    uint32_t flags;  /* LTTNG_VIEWER_FLAG_* */
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_GET_PACKET payload.
 */
struct lttng_viewer_get_packet
{
    uint64_t stream_id;
    uint64_t offset;
    uint32_t len;
} __attribute__((__packed__));

struct lttng_viewer_trace_packet
{
    uint32_t status; /* enum lttng_viewer_get_packet_return_code */
    uint32_t len;
    uint32_t flags; /* LTTNG_VIEWER_FLAG_* */
    char data[];
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_GET_METADATA payload.
 */
struct lttng_viewer_get_metadata
{
    uint64_t stream_id;
} __attribute__((__packed__));

struct lttng_viewer_metadata_packet
{
    uint64_t len;
    uint32_t status; /* enum lttng_viewer_get_metadata_return_code */
    char data[];
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_GET_NEW_STREAMS payload.
 */
struct lttng_viewer_new_streams_request
{
    uint64_t session_id;
} __attribute__((__packed__));

struct lttng_viewer_new_streams_response
{
    /* enum lttng_viewer_new_streams_return_code */
    uint32_t status;
    uint32_t streams_count;
    /* struct lttng_viewer_stream */
    char stream_list[];
} __attribute__((__packed__));

struct lttng_viewer_create_session_response
{
    /* enum lttng_viewer_create_session_return_code */
    uint32_t status;
} __attribute__((__packed__));

/*
 * LTTNG_VIEWER_DETACH_SESSION payload.
 */
struct lttng_viewer_detach_session_request
{
    uint64_t session_id;
} __attribute__((__packed__));

struct lttng_viewer_detach_session_response
{
    /* enum lttng_viewer_detach_session_return_code */
    uint32_t status;
} __attribute__((__packed__));

#endif /* BABELTRACE_PLUGINS_CTF_LTTNG_LIVE_LTTNG_VIEWER_ABI_HPP */
