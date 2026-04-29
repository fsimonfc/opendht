// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../logger.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dht {
namespace sim {

/**
 * Per-channel enable/disable for metric sinks. See SIMULATION_DESIGN.md §4.7.
 *
 * Aggregated counters (totals on `counters()`) are always recorded regardless
 * of mask; only per-event recording is gated by the mask.
 */
struct MetricMask
{
    bool packets : 1;
    bool ops : 1;
    bool logs : 1;
    bool timers : 1;

    static constexpr MetricMask all() noexcept { return {true, true, true, true}; }
    /** Same shape as a typical OpenDHT log file plus structured op records. */
    static constexpr MetricMask defaults() noexcept { return {false, true, true, false}; }
    static constexpr MetricMask none() noexcept { return {false, false, false, false}; }
};

enum class OpKind : uint8_t {
    Put = 0,
    Get = 1,
    Listen = 2,
    Bootstrap = 3,
    Cancel = 4,
    Other = 5,
};

/** Aggregated counters; always populated by default sinks. */
struct MetricCounters
{
    uint64_t packets_sent {0};
    uint64_t packets_dropped {0};
    uint64_t bytes_sent {0};
    uint64_t bytes_dropped {0};
    uint64_t ops_recorded {0};
    uint64_t timers_fired {0};
    uint64_t logs_emitted {0};
};

/**
 * Sink for simulator-emitted metric events. The simulator emits regardless of
 * the mask; the sink itself decides whether to record per-event.
 *
 * All callbacks receive `t` = simulated time (`SimClock::now()`).
 */
class OPENDHT_PUBLIC MetricSink
{
public:
    using time_point = std::chrono::steady_clock::time_point;
    virtual ~MetricSink() = default;

    /** Packet-level traffic accounting. `dropped` true means the LatencyModel
     *  decided to drop the packet (no delivery event will follow). */
    virtual void onPacket(time_point t, std::size_t src_node, std::size_t dst_node, std::size_t size, bool dropped) = 0;

    /** Workload-emitted op record. The simulator does *not* automatically emit
     *  these; workloads call this to mark notable points. `key` is typically a
     *  hex InfoHash. `status` is free-form (e.g. "started", "ok", "fail"). */
    virtual void onOp(time_point t, std::size_t node, OpKind kind, std::string_view key, std::string_view status) = 0;

    /** Pass-through for the per-node SimLogger. */
    virtual void onLog(time_point t, std::size_t node, log::LogLevel level, std::string_view message) = 0;

    /** Fired each time the simulator pops a Timer event. */
    virtual void onTimer(time_point t, std::size_t node, time_point scheduled_at) = 0;

    /** Aggregated, mask-independent counters. */
    virtual MetricCounters counters() const noexcept { return {}; }

    /** Persist any buffered output. */
    virtual void flush() {}
};

/** Drop-everything sink: counters only. Used when no sink is configured. */
class OPENDHT_PUBLIC NullMetricSink final : public MetricSink
{
public:
    void onPacket(time_point, std::size_t, std::size_t, std::size_t size, bool dropped) override
    {
        if (dropped) {
            ++c_.packets_dropped;
            c_.bytes_dropped += size;
        } else {
            ++c_.packets_sent;
            c_.bytes_sent += size;
        }
    }
    void onOp(time_point, std::size_t, OpKind, std::string_view, std::string_view) override { ++c_.ops_recorded; }
    void onLog(time_point, std::size_t, log::LogLevel, std::string_view) override { ++c_.logs_emitted; }
    void onTimer(time_point, std::size_t, time_point) override { ++c_.timers_fired; }
    MetricCounters counters() const noexcept override { return c_; }

private:
    MetricCounters c_ {};
};

/**
 * In-memory metric sink. Per-event records are kept in `std::vector`s so
 * tests can inspect them. Mask channels disable per-event recording but
 * counters are always updated.
 */
class OPENDHT_PUBLIC InMemoryMetrics final : public MetricSink
{
public:
    struct PacketRecord
    {
        time_point t;
        std::size_t src;
        std::size_t dst;
        std::size_t size;
        bool dropped;
    };
    struct OpRecord
    {
        time_point t;
        std::size_t node;
        OpKind kind;
        std::string key;
        std::string status;
    };
    struct LogRecord
    {
        time_point t;
        std::size_t node;
        log::LogLevel level;
        std::string message;
    };
    struct TimerRecord
    {
        time_point t;
        std::size_t node;
        time_point scheduled_at;
    };

    explicit InMemoryMetrics(MetricMask mask = MetricMask::defaults())
        : mask_(mask)
    {}

    void onPacket(time_point t, std::size_t src, std::size_t dst, std::size_t size, bool dropped) override;
    void onOp(time_point t, std::size_t node, OpKind kind, std::string_view key, std::string_view status) override;
    void onLog(time_point t, std::size_t node, log::LogLevel level, std::string_view message) override;
    void onTimer(time_point t, std::size_t node, time_point scheduled_at) override;
    MetricCounters counters() const noexcept override;

    MetricMask mask() const noexcept { return mask_; }

    // Read-only views; the caller must not mutate the simulator while iterating.
    const std::vector<PacketRecord>& packets() const noexcept { return packets_; }
    const std::vector<OpRecord>& ops() const noexcept { return ops_; }
    const std::vector<LogRecord>& logs() const noexcept { return logs_; }
    const std::vector<TimerRecord>& timers() const noexcept { return timers_; }

private:
    MetricMask mask_;
    mutable std::mutex mu_;
    MetricCounters c_ {};
    std::vector<PacketRecord> packets_;
    std::vector<OpRecord> ops_;
    std::vector<LogRecord> logs_;
    std::vector<TimerRecord> timers_;
};

/**
 * Newline-delimited JSON sink. Writes one JSON object per recorded event to
 * `out` (typically a `std::ofstream`). Useful with `dhtsim` for offline
 * analysis. Disabled channels are dropped at the sink boundary.
 */
class OPENDHT_PUBLIC JsonlMetrics final : public MetricSink
{
public:
    /** @param out  output stream (kept alive by `out_owner_` if owning). */
    JsonlMetrics(std::ostream& out, MetricMask mask = MetricMask::defaults());

    /** Convenience: open a file by path. Throws on failure. */
    static std::shared_ptr<JsonlMetrics> openFile(const std::string& path, MetricMask mask = MetricMask::defaults());

    void onPacket(time_point t, std::size_t src, std::size_t dst, std::size_t size, bool dropped) override;
    void onOp(time_point t, std::size_t node, OpKind kind, std::string_view key, std::string_view status) override;
    void onLog(time_point t, std::size_t node, log::LogLevel level, std::string_view message) override;
    void onTimer(time_point t, std::size_t node, time_point scheduled_at) override;
    MetricCounters counters() const noexcept override;
    void flush() override;

    MetricMask mask() const noexcept { return mask_; }

private:
    std::shared_ptr<std::ostream> out_owner_;
    std::ostream& out_;
    MetricMask mask_;
    std::mutex mu_;
    MetricCounters c_ {};
};

/** Parse "ops,logs,packets,timers" -> MetricMask. Unknown tokens are ignored. */
OPENDHT_PUBLIC MetricMask parseMetricMask(std::string_view csv);

/** "Put"/"Get"/etc. */
OPENDHT_PUBLIC std::string_view opKindName(OpKind k) noexcept;

} // namespace sim
} // namespace dht
