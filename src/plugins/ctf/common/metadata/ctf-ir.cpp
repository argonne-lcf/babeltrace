/*
 * Copyright (c) 2023-2024 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "ctf-ir.hpp"
#include "json-strings.hpp"

namespace ctf {
namespace ir {

const char * const ClkOrigin::_unixEpochNs = jsonstr::btNs;
const char * const ClkOrigin::_unixEpochName = jsonstr::unixEpoch;
const char * const ClkOrigin::_unixEpochUid = "";
const char * const defaultBlobMediaType = "application/octet-stream";

} /* namespace ir */
} /* namespace ctf */
