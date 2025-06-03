// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "time_interface.h"

#include <chrono>

namespace dht {

class RealTime : public TimeInterface
{
    using clock = std::chrono::steady_clock;
    using system_clock = std::chrono::system_clock;

public:
    clock::time_point steadyNow() override { return clock::now(); }
    system_clock::time_point systemNow() override { return system_clock::now(); }

    clock::time_point from_time_t(std::time_t t) override
    {
        auto dt = system_clock::from_time_t(t) - system_clock::now();
        auto now = clock::now();
        if (dt > system_clock::duration(0) and now > clock::time_point::max() - dt)
            return clock::time_point::max();
        else if (dt < system_clock::duration(0) and now < clock::time_point::min() - dt)
            return clock::time_point::min();
        return now + dt;
    }

    std::time_t to_time_t(clock::time_point t) override
    {
        auto dt = t - clock::now();
        auto now = system_clock::now();
        if (dt > clock::duration(0) and now >= system_clock::time_point::max() - dt)
            return system_clock::to_time_t(system_clock::time_point::max());
        else if (dt < clock::duration(0) and now <= system_clock::time_point::min() - dt)
            return system_clock::to_time_t(system_clock::time_point::min());
        return system_clock::to_time_t(now + std::chrono::duration_cast<system_clock::duration>(dt));
    }
};

} // namespace dht
