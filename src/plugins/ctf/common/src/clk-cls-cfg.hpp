/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022 EfficiOS Inc. and Linux Foundation
 */

#ifndef CTF_COMMON_SRC_CLK_CLS_CFG_HPP
#define CTF_COMMON_SRC_CLK_CLS_CFG_HPP

#include <cstdint>

namespace ctf {
namespace src {

struct ClkClsCfg
{
    std::int64_t offsetSec = 0;
    std::int64_t offsetNanoSec = 0;
    bool forceOriginIsUnixEpoch = false;
};

} /* namespace src */
} /* namespace ctf */

#endif /* CTF_COMMON_SRC_CLK_CLS_CFG_HPP */