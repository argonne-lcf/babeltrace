/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_LA_SRC_METADATA_HPP
#define BABELTRACE_PLUGINS_CTF_LA_SRC_METADATA_HPP

#include <stdio.h>

#include <babeltrace2/babeltrace.h>

namespace bt2c {

class Logger;

} /* namespace bt2c */

#include "../common/src/clk-cls-cfg.hpp"

#define CTF_LA_METADATA_FILENAME "metadata"

int ctf_la_metadata_set_trace_class(bt_self_component *self_comp, struct ctf_la_trace *ctf_la_trace,
                                    const ctf::src::ClkClsCfg& clkClsCfg);

FILE *ctf_la_metadata_open_file(const char *trace_path, const bt2c::Logger& logger);

bool ctf_metadata_is_packetized(FILE *fp, int *byte_order);

#endif /* BABELTRACE_PLUGINS_CTF_LA_SRC_METADATA_HPP */
