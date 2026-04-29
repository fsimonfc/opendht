// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../time_interface.h"

#include <chrono>
#include <memory>

namespace dht {
namespace sim {

/**
 * Simulated TimeInterface backed by a value advanced explicitly by the simulator.
 *
 * One shared SteadyState is held by every per-node SimClock, so they all observe
 * the same `steadyNow()`. Each clock can carry its own `system_offset` to give
 * nodes a fixed wall-clock skew without affecting steady time.
 */
class OPENDHT_PUBLIC SimClock : public TimeInterface
{
public:
    using clock = std::chrono::steady_clock;
    using system_clock = std::chrono::system_clock;

    /** Shared, mutable steady-clock reading; advanced by the simulator. */
    struct SteadyState
    {
        clock::time_point now {clock::time_point {} + std::chrono::seconds {1'700'000'000}};
        /** Sim-wide offset from steady epoch to system epoch. */
        system_clock::duration system_anchor_offset {std::chrono::duration_cast<system_clock::duration>(
            system_clock::time_point {std::chrono::seconds {1'700'000'000}}.time_since_epoch())};
    };

    SimClock(std::shared_ptr<SteadyState> state, system_clock::duration per_node_offset = {})
        : state_(std::move(state))
        , per_node_offset_(per_node_offset)
    {}

    clock::time_point steadyNow() override { return state_->now; }

    system_clock::time_point systemNow() override
    {
        auto steady_d = std::chrono::duration_cast<system_clock::duration>(state_->now.time_since_epoch());
        return system_clock::time_point {steady_d + state_->system_anchor_offset + per_node_offset_};
    }

    clock::time_point from_time_t(std::time_t t) override
    {
        auto sys = std::chrono::duration_cast<system_clock::duration>(system_clock::from_time_t(t).time_since_epoch());
        auto steady_d = sys - state_->system_anchor_offset - per_node_offset_;
        return clock::time_point {std::chrono::duration_cast<clock::duration>(steady_d)};
    }

    std::time_t to_time_t(clock::time_point t) override
    {
        auto steady_d = std::chrono::duration_cast<system_clock::duration>(t.time_since_epoch());
        return system_clock::to_time_t(
            system_clock::time_point {steady_d + state_->system_anchor_offset + per_node_offset_});
    }

    const std::shared_ptr<SteadyState>& state() const { return state_; }

private:
    std::shared_ptr<SteadyState> state_;
    system_clock::duration per_node_offset_ {};
};

} // namespace sim
} // namespace dht
