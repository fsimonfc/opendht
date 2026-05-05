// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../dhtrunner.h"
#include "../node_op.h"
#include "../sockaddr.h"
#include "identity_cache.h"
#include "node_op_record.h"
#include "packet_recorder.h"
#include "sim_clock.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace dht {
namespace sim {

class NodeOpRecorder;
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
    /** Fixed one-way latency for all packets. */
    std::chrono::milliseconds latency {20};
    /** Probability that any given packet is dropped (0.0 = no loss, 1.0 = total loss). */
    double drop_probability {0.0};
    /**
     * Probability that a node fails to bind one of the two address families (IPv4 or IPv6).
     * When non-zero, each node independently has this chance of being bound to only one
     * address family (chosen at random). 0.0 = all nodes are dual-stack.
     */
    double bind_failure_probability {0.0};
    /** Optional logger override. If unset, a SimLogger writing to stderr is used. */
    std::shared_ptr<Logger> logger_override;
    /** Selects the per-packet recorder built by `Simulator`. */
    PacketRecorderKind packet_recorder {PacketRecorderKind::None};
    /** Output path for `PacketRecorderKind::Jsonl`. Ignored otherwise. */
    std::string packet_recorder_file;
    /**
     * Optional identity cache. When non-null the simulator fetches
     * `cache->get(identity_seed, i)` for each node and installs it as the
     * node's `crypto::Identity`. When null, nodes run without an identity
     * (faster; suitable for routing/perf tests).
     */
    std::shared_ptr<IdentityCache> identity_cache;
    /** When true, enable per-node SimLogger output. */
    bool verbose {false};
    /** When true, record all node operations (Put/Get/Listen etc.) executed via schedule(). */
    bool record_node_ops {false};
    /** Output path for node-operation JSONL dump. Ignored when record_node_ops is false. */
    std::string node_ops_file;
};

/** A single simulated node: non-threaded DhtRunner + clock. The DatagramSocket
 *  is owned internally by the DhtRunner. */
struct SimNode
{
    size_t id {};
    SockAddr addr4;
    SockAddr addr6;
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

    /** Per-event-kind metrics maintained by `runOne()`. */
    struct Metrics
    {
        uint64_t timer_events {0};
        uint64_t packet_events {0};
        uint64_t workload_events {0};
    };

    explicit Simulator(SimConfig cfg);
    ~Simulator();

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    // ---- topology -----------------------------------------------------------
    SimNode& node(size_t i);
    size_t nodeCount() const;
    /** Bootstrap all nodes at random times within `maxDuration` from now. */
    void bootstrapAll(std::chrono::milliseconds maxDuration = std::chrono::milliseconds {15000});

    // ---- time & control -----------------------------------------------------
    time_point now() const;
    void runFor(std::chrono::nanoseconds);
    void runUntil(time_point deadline);
    /** Run until `predicate()` is true or `timeout` elapses. Returns predicate result. */
    bool runUntil(std::function<bool()> predicate, std::chrono::nanoseconds timeout);

    // ---- workload helpers ---------------------------------------------------
    std::mt19937_64& rng();
    /** Schedule a typed node operation at a given simulated time. */
    void schedule(time_point at, NodeOp op);
    /** Schedule a typed node operation at the current simulated time. */
    void schedule(NodeOp op);

    // ---- internals ----------------------------------------------------------
    std::shared_ptr<SimNetwork> network() const;
    /** Per-event-kind metrics. Use `network()->metrics()` for packet/byte metrics. */
    const Metrics& metrics() const noexcept;
    /** Active per-packet recorder (may be null when configured as `None`). */
    std::shared_ptr<PacketRecorder> packetRecorder() const;
    /** Active node-operation recorder (may be null when disabled). */
    std::shared_ptr<NodeOpRecorder> nodeOpRecorder() const;
    /** Schedule a Timer that calls `runner->loop()` on node `i` at `at`. */
    void scheduleTick(size_t i, time_point at);
    /** Deterministic event trace (one entry per popped event). */
    const std::vector<TraceRecord>& trace() const;
    /** FNV-1a hash of the event trace, useful for cross-machine spot-checks. */
    std::string traceHash() const;

private:
    struct Event
    {
        time_point time;
        EventKind kind;
        uint64_t seq;
        uint32_t a {0};
        uint32_t b {0};
        uint32_t c {0};
        std::function<void()> run;

        bool operator<(const Event& other) const
        {
            if (time != other.time)
                return time > other.time;
            return seq > other.seq;
        }
    };

    void enqueue(time_point at, EventKind k, std::function<void()> fn, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0);
    void executeOp(NodeOp op);
    void scheduleNextWakeup(size_t i, time_point wakeup);
    void tickNode(size_t i);
    void buildNodes();
    std::pair<SockAddr, SockAddr> makeNodeAddrs(size_t i);
    void runOne();

    SimConfig cfg_;
    std::shared_ptr<SimClock::SteadyState> steady_state_ {std::make_shared<SimClock::SteadyState>()};
    std::mt19937_64 rng_;
    std::priority_queue<Event> queue_;
    uint64_t next_seq_ {0};
    std::shared_ptr<SimNetwork> network_;
    std::vector<std::unique_ptr<SimNode>> nodes_;
    std::vector<TraceRecord> trace_;
    std::shared_ptr<PacketRecorder> recorder_;
    std::shared_ptr<NodeOpRecorder> op_recorder_;
    Metrics metrics_ {};
};

} // namespace sim
} // namespace dht
