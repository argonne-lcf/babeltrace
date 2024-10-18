/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_FS_SINK_TRANSLATE_CTF_IR_TO_JSON_HPP
#define BABELTRACE_PLUGINS_CTF_FS_SINK_TRANSLATE_CTF_IR_TO_JSON_HPP

#include <glib.h>

void translate_trace_ctf_ir_to_json(struct fs_sink_ctf_trace *trace, GString *json);

#endif /* BABELTRACE_PLUGINS_CTF_FS_SINK_TRANSLATE_CTF_IR_TO_JSON_HPP */
