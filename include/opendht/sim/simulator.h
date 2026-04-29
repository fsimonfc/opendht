// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../dhtrunner.h"
#include "../sockaddr.h"
#include "identity_cache.h"
#include "latency_model.h"
#include "metric_sink.h"
#include "sim_clock.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace dht {
namespace sim {

class SimNetwork;
class SimSocket;
class Workload;

/** Discrete event kind, ordered for tie-breaking among events at the same time. */
enum class EventKind : uint8_t { Timer = 0, PacketArrival = 1, Workload = 2 };

/**
 * One entry in the simulator's deterministic event trace.
 *
 * The trace is the canonical record of "what the simulator did" and is used by
 * the determinism gate. Two runs with the same SimConfig must produce two
 * std::vector<TraceRecord>s that compare equal element-wise.
 *
 * Fields:
 *   time : when the event fired (simulated steady_clock).
 *   kind : Timer / PacketArrival / Workload (see §3.2 of SIMULATION_DESIGN.md).
 *   seq  : queue insertion order (monotonically increasing, no gaps).
 *   a    : Timer -> node id.        PacketArrival -> dst node id.   Workload -> 0.
 *   b    : Timer -> 0.              PacketArrival -> src node id.   Workload -> 0.
 *   c    : Timer -> 0.              PacketArrival -> packet size.   Workload -> 0.
 */
struct TraceRecord
{
    std::chrono::steady_clock::time_point time;
    EventKind kind;
    uint64_t seq;
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const TraceRecord& o) const noexcept
    {
        return time == o.time && kind == o.kind && seq == o.seq && a == o.a && b == o.b && c == o.c;
    }
    bool operator!=(const TraceRecord& o) const noexcept { return !(*this == o); }
};

struct SimConfig
{
    /** Seed for simulator events / latency / RNG-driven choices. */
    uint64_t seed {0xC0FFEEull};
    /** Seed for crypto identities (separate so cache hits across runs). */
    uint64_t identity_seed {0xDEADBEEFull};
    /** Number of simulated nodes. */
    size_t node_count {1};
    /** Per-node maximum random `systemNow()` skew; 0 disables skew. */
    std::chrono::milliseconds system_clock_skew_max {0};
    /** Latency model. Defaults to a fixed 20 ms link. */
    std::shared_ptr<LatencyModel> latency;
    /** Optional logger override. If unset, a SimLogger writing to stderr is used. */
    std::shared_ptr<Logger> logger_override;
    /** Optional metric sink. Defaults to a `NullMetricSink` (counters only). */
    std::shared_ptr<MetricSink> metrics;
    /**
     * Optional identity cache. When non-null the simulator fetches
     * `cache->get(identity_seed, i)` for each node and installs it as the
     * node's `crypto::Identity`. When null, nodes run without an identity
     * (faster; suitable for routing/perf tests).
     */
    std::shared_ptr<IdentityCache> identity_cache;
    /** When true, suppress per-node SimLogger output (useful in unit tests). */
    bool quiet {false};
};

/** A single simulated node: non-threaded DhtRunner + clock. The DatagramSocket
 *  is owned internally by the DhtRunner. */
struct SimNode
{
    size_t id {};
    SockAddr addr;
    std::shared_ptr<DhtRunner> runner;
    std::shared_ptr<SimClock> clock;
};

/**
 * The simulator. Owns the event queue, the SimNetwork, and N SimNodes. Single
 * threaded by construction.
 */
class OPENDHT_PUBLIC Simulator
{
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    explicit Simulator(SimConfig cfg);
    ~Simulator();

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    // ---- topology -----------------------------------------------------------
    SimNode& node(size_t i);
    size_t nodeCount() const;
    /** Wire each node (i > 0) to node(0) via DhtRunner::bootstrap. */
    void bootstrapAll();

    // ---- time & control -----------------------------------------------------
    time_point now() const;
    void runFor(std::chrono::nanoseconds);
    void runUntil(time_point deadline);
    /** Run until `predicate()` is true or `timeout` elapses. Returns predicate result. */
    bool runUntil(std::function<bool()> predicate, std::chrono::nanoseconds timeout);

    // ---- workload helpers ---------------------------------------------------
    std::mt19937_64& rng();
    void scheduleAt(time_point at, std::function<void()> fn);

    // ---- internals ----------------------------------------------------------
    std::shared_ptr<SimClock> clockFor(size_t node_index) const;
    std::shared_ptr<SimNetwork> network() const;
    /** Active metric sink (never null; defaults to NullMetricSink). */
    MetricSink& metrics() const;
    std::shared_ptr<MetricSink> metricsPtr() const;

    /** Schedule a Timer that calls `runner->loop()` on node `i` at `at`. */
    void scheduleTick(size_t i, time_point at);

    /** Deterministic event trace (one entry per popped event). */
    const std::vector<TraceRecord>& trace() const;
    /** SHA-256 hex of the trace, useful for cross-machine spot-checks. */
    std::string traceHash() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

/** Convenience helper: build a Simulator, run a workload, return verify() result. */
bool runWorkload(SimConfig cfg, Workload& w, std::string& error_out);

} // namespace sim
} // namespace dht
