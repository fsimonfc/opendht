// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "def.h"

#include <chrono>

namespace dht {

class OPENDHT_PUBLIC TimeInterface
{
    using clock = std::chrono::steady_clock;
    using system_clock = std::chrono::system_clock;

public:
    virtual ~TimeInterface() = default;

    virtual clock::time_point steadyNow() = 0;
    virtual system_clock::time_point systemNow() = 0;
    virtual clock::time_point from_time_t(std::time_t t) = 0;
    virtual std::time_t to_time_t(clock::time_point t) = 0;
};

} // namespace dht
