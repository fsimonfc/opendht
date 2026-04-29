// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../sockaddr.h"

#include <chrono>
#include <optional>
#include <random>

namespace dht {
namespace sim {

/**
 * Pluggable, RNG-driven model that decides how long a packet spends in flight
 * (or whether it is dropped at all). All packet decisions go through this
 * interface so that latency, jitter, drop and duplication policies can be
 * combined without leaking into the simulator core.
 */
class OPENDHT_PUBLIC LatencyModel
{
public:
    struct Decision
    {
        std::chrono::nanoseconds latency {0};
        bool drop {false};
        // Future: duplicate, reorder, etc.
    };

    virtual ~LatencyModel() = default;

    virtual Decision decide(const SockAddr& src, const SockAddr& dst, size_t bytes, std::mt19937_64& rng) = 0;
};

/** Constant latency, no loss. The default for M1. */
class OPENDHT_PUBLIC FixedLatency : public LatencyModel
{
public:
    explicit FixedLatency(std::chrono::nanoseconds d)
        : d_(d)
    {}
    Decision decide(const SockAddr&, const SockAddr&, size_t, std::mt19937_64&) override
    {
        return Decision {d_, false};
    }

private:
    std::chrono::nanoseconds d_;
};

/** Latency drawn uniformly from [lo, hi]. */
class OPENDHT_PUBLIC UniformLatency : public LatencyModel
{
public:
    UniformLatency(std::chrono::nanoseconds lo, std::chrono::nanoseconds hi)
        : lo_(lo)
        , hi_(hi)
    {}
    Decision decide(const SockAddr&, const SockAddr&, size_t, std::mt19937_64& rng) override
    {
        std::uniform_int_distribution<long long> dist(lo_.count(), hi_.count());
        return Decision {std::chrono::nanoseconds {dist(rng)}, false};
    }

private:
    std::chrono::nanoseconds lo_, hi_;
};

} // namespace sim
} // namespace dht
