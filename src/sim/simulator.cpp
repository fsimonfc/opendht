// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/simulator.h"
#include "opendht/sim/identity_cache.h"
#include "opendht/sim/metric_sink.h"
#include "opendht/sim/sim_logger.h"
#include "opendht/sim/sim_network.h"
#include "opendht/sim/sim_socket.h"
#include "opendht/sim/workload.h"

#include <cstdio>
#include <queue>
#include <stdexcept>
#include <vector>

namespace dht {
namespace sim {

namespace {

/** Three event kinds, ordered Timer < PacketArrival < Workload at the same time
 *  to keep the schedule deterministic in case of ties. */

struct Event
{
    Simulator::time_point time;
    EventKind kind;
    uint64_t seq;
    uint32_t a {0};
    uint32_t b {0};
    uint32_t c {0};
    std::function<void()> run;

    bool operator<(const Event& other) const
    {
        // std::priority_queue is a max-heap; invert so smallest time fires first.
        if (time != other.time)
            return time > other.time;
        if (kind != other.kind)
            return static_cast<uint8_t>(kind) > static_cast<uint8_t>(other.kind);
        return seq > other.seq;
    }
};

SockAddr
makeNodeAddr(size_t i)
{
    SockAddr a;
    a.setFamily(AF_INET);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "10.0.0.%zu", (i % 254) + 1);
    a.setAddress(buf);
    a.setPort(net::DHT_DEFAULT_PORT);
    return a;
}

uint64_t
mix(uint64_t a, uint64_t b)
{
    uint64_t x = a + 0x9E3779B97F4A7C15ULL + (b << 6) + (b >> 2);
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

} // namespace

struct Simulator::Impl
{
    SimConfig cfg;
    std::shared_ptr<SimClock::SteadyState> steady_state {std::make_shared<SimClock::SteadyState>()};
    std::mt19937_64 rng;
    std::priority_queue<Event> queue;
    uint64_t next_seq {0};
    std::shared_ptr<SimNetwork> network;
    std::vector<std::unique_ptr<SimNode>> nodes;
    std::vector<TraceRecord> trace;

    explicit Impl(SimConfig c)
        : cfg(std::move(c))
        , rng(cfg.seed)
    {}

    void enqueue(time_point at, EventKind k, std::function<void()> fn, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0)
    {
        if (at < steady_state->now)
            throw std::logic_error {"Simulator: attempted to schedule event in the past"};
        queue.push(Event {at, k, next_seq++, a, b, c, std::move(fn)});
    }

    void scheduleNextWakeup(size_t i, std::chrono::steady_clock::time_point wakeup)
    {
        if (wakeup == std::chrono::steady_clock::time_point {} || wakeup == time_point::max())
            return;
        if (wakeup < steady_state->now)
            wakeup = steady_state->now;
        enqueue(wakeup, EventKind::Timer, [this, i]() { tickNode(i); }, static_cast<uint32_t>(i));
    }

    void tickNode(size_t i)
    {
        if (i >= nodes.size())
            return;
        auto& n = *nodes[i];
        if (!n.runner)
            return;
        auto wakeup = n.runner->loop();
        scheduleNextWakeup(i, wakeup);
    }

    void buildNodes();

    /** Pop one event, advance the clock, record the trace, run the handler. */
    void runOne()
    {
        Event e = queue.top();
        queue.pop();
        if (e.time > steady_state->now)
            steady_state->now = e.time;
        trace.push_back(TraceRecord {e.time, e.kind, e.seq, e.a, e.b, e.c});
        if (cfg.metrics && e.kind == EventKind::Timer)
            cfg.metrics->onTimer(e.time, static_cast<size_t>(e.a), e.time);
        e.run();
    }
};

void
Simulator::Impl::buildNodes()
{
    if (!cfg.metrics)
        cfg.metrics = std::make_shared<NullMetricSink>();
    auto state = steady_state;
    network = std::make_shared<SimNetwork>(
        cfg.latency,
        rng,
        [this](size_t dst_id, size_t src_id, SockAddr src, Blob data, std::chrono::nanoseconds latency) {
            auto deliver_at = steady_state->now + latency;
            auto size = static_cast<uint32_t>(data.size());
            enqueue(
                deliver_at,
                EventKind::PacketArrival,
                [this, dst_id, src = std::move(src), data = std::move(data)]() mutable {
                    if (dst_id >= nodes.size())
                        return;
                    auto& dst_node = *nodes[dst_id];
                    if (auto* s = network->find(dst_node.addr))
                        s->deliver(src, std::move(data));
                    // Drain rcv on the destination immediately after delivery.
                    tickNode(dst_id);
                },
                static_cast<uint32_t>(dst_id),
                static_cast<uint32_t>(src_id),
                size);
        },
        cfg.metrics,
        [state]() { return state->now; });

    nodes.reserve(cfg.node_count);
    for (size_t i = 0; i < cfg.node_count; ++i) {
        auto n = std::make_unique<SimNode>();
        n->id = i;
        n->addr = makeNodeAddr(i);

        std::chrono::system_clock::duration skew {0};
        if (cfg.system_clock_skew_max.count() > 0) {
            std::uniform_int_distribution<long long> d(-cfg.system_clock_skew_max.count(),
                                                       cfg.system_clock_skew_max.count());
            skew = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds {d(rng)});
        }
        n->clock = std::make_shared<SimClock>(steady_state, skew);
        n->runner = std::make_shared<DhtRunner>();

        DhtRunner::Config rcfg {};
        rcfg.threaded = false;
        if (cfg.identity_cache) {
            try {
                rcfg.dht_config.id = cfg.identity_cache->get(cfg.identity_seed, i);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Simulator: identity_cache->get failed for node ")
                                         + std::to_string(i) + ": " + e.what());
            }
        }

        DhtRunner::Context rctx;
        rctx.time = n->clock;
        rctx.sock = std::make_unique<SimSocket>(i, n->addr, network, steady_state);
        if (cfg.logger_override) {
            // User-provided logger bypasses the metric `logs` channel; document
            // it: callers wiring their own logger are expected to handle their
            // own metric instrumentation.
            rctx.logger = cfg.logger_override;
        } else if (!cfg.quiet) {
            rctx.logger = makeSimLogger(steady_state, i, {}, cfg.metrics);
        } else if (cfg.metrics) {
            // Quiet mode but metrics requested: emit logs to the sink only.
            auto sink = cfg.metrics;
            auto state = steady_state;
            rctx.logger = std::make_shared<Logger>(
                [sink, state, i](log::source_loc, log::LogLevel level, std::string_view, std::string&& message) {
                    sink->onLog(state->now, i, level, message);
                });
        }
        rctx.rng = std::make_unique<std::mt19937_64>(mix(cfg.seed, static_cast<uint64_t>(i)));

        n->runner->run(rcfg, std::move(rctx));

        // Schedule an initial tick so the scheduler bootstraps.
        enqueue(steady_state->now, EventKind::Timer, [this, i]() { tickNode(i); }, static_cast<uint32_t>(i));

        nodes.push_back(std::move(n));
    }
}

// ---------- public Simulator API --------------------------------------------

Simulator::Simulator(SimConfig cfg)
    : p_(std::make_unique<Impl>(std::move(cfg)))
{
    if (!p_->cfg.latency)
        p_->cfg.latency = std::make_shared<FixedLatency>(std::chrono::milliseconds {20});
    p_->buildNodes();
}

Simulator::~Simulator()
{
    if (!p_)
        return;
    // Tear nodes down in reverse order. join() destroys the dht_, which owns
    // the SimSocket (which then unregisters from SimNetwork in its destructor).
    for (auto it = p_->nodes.rbegin(); it != p_->nodes.rend(); ++it) {
        if ((*it)->runner)
            (*it)->runner->join();
    }
}

SimNode&
Simulator::node(size_t i)
{
    return *p_->nodes.at(i);
}
size_t
Simulator::nodeCount() const
{
    return p_->nodes.size();
}

void
Simulator::bootstrapAll()
{
    if (p_->nodes.size() < 2)
        return;
    auto& boot = *p_->nodes[0];
    for (size_t i = 1; i < p_->nodes.size(); ++i) {
        p_->nodes[i]->runner->bootstrap(boot.addr);
        p_->enqueue(p_->steady_state->now, EventKind::Timer, [this, i]() { p_->tickNode(i); }, static_cast<uint32_t>(i));
    }
}

Simulator::time_point
Simulator::now() const
{
    return p_->steady_state->now;
}

void
Simulator::runFor(std::chrono::nanoseconds d)
{
    runUntil(p_->steady_state->now + d);
}

void
Simulator::runUntil(time_point deadline)
{
    while (!p_->queue.empty() && p_->queue.top().time <= deadline)
        p_->runOne();
    if (deadline > p_->steady_state->now)
        p_->steady_state->now = deadline;
}

bool
Simulator::runUntil(std::function<bool()> predicate, std::chrono::nanoseconds timeout)
{
    auto deadline = p_->steady_state->now + timeout;
    while (!predicate()) {
        if (p_->queue.empty() || p_->queue.top().time > deadline) {
            p_->steady_state->now = deadline;
            return predicate();
        }
        p_->runOne();
    }
    return true;
}

std::mt19937_64&
Simulator::rng()
{
    return p_->rng;
}

void
Simulator::scheduleAt(time_point at, std::function<void()> fn)
{
    p_->enqueue(at, EventKind::Workload, std::move(fn));
}

std::shared_ptr<SimClock>
Simulator::clockFor(size_t i) const
{
    return p_->nodes.at(i)->clock;
}
std::shared_ptr<SimNetwork>
Simulator::network() const
{
    return p_->network;
}

MetricSink&
Simulator::metrics() const
{
    return *p_->cfg.metrics;
}

std::shared_ptr<MetricSink>
Simulator::metricsPtr() const
{
    return p_->cfg.metrics;
}

void
Simulator::scheduleTick(size_t i, time_point at)
{
    p_->enqueue(at, EventKind::Timer, [this, i]() { p_->tickNode(i); }, static_cast<uint32_t>(i));
}

const std::vector<TraceRecord>&
Simulator::trace() const
{
    return p_->trace;
}

std::string
Simulator::traceHash() const
{
    // Simple FNV-1a 64-bit over the binary footprint of each trace record,
    // returned as 16-char hex. SHA-256 was suggested in the design but FNV-1a
    // is sufficient for spot-checks and avoids a new dependency.
    uint64_t h = 0xcbf29ce484222325ULL;
    auto mix = [&](const void* data, size_t n) {
        const auto* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 0x100000001b3ULL;
        }
    };
    for (const auto& r : p_->trace) {
        auto t = r.time.time_since_epoch().count();
        mix(&t, sizeof(t));
        mix(&r.kind, sizeof(r.kind));
        mix(&r.seq, sizeof(r.seq));
        mix(&r.a, sizeof(r.a));
        mix(&r.b, sizeof(r.b));
        mix(&r.c, sizeof(r.c));
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return std::string {buf};
}

bool
runWorkload(SimConfig cfg, Workload& w, std::string& error_out)
{
    Simulator sim(std::move(cfg));
    w.run(sim);
    return w.verify(sim, error_out);
}

} // namespace sim
} // namespace dht
