// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/simulator.h"
#include "opendht/sim/identity_cache.h"
#include "opendht/sim/packet_recorder.h"
#include "opendht/sim/sim_logger.h"
#include "opendht/sim/sim_network.h"
#include "opendht/sim/sim_socket.h"
#include "opendht/sim/workload.h"

#include "opendht/crypto.h"
#include "opendht/infohash.h"

#include <cstdio>
#include <stdexcept>

namespace dht {
namespace sim {

// ---------- private helpers -------------------------------------------------

void
Simulator::enqueue(time_point at, EventKind k, std::function<void()> fn, uint32_t a, uint32_t b, uint32_t c)
{
    if (at < steady_state_->now)
        throw std::logic_error {"Simulator: attempted to schedule event in the past"};
    queue_.push(Event {at, k, next_seq_++, a, b, c, std::move(fn)});
}

void
Simulator::scheduleNextWakeup(size_t i, time_point wakeup)
{
    if (wakeup == time_point {} || wakeup == time_point::max())
        return;
    if (wakeup < steady_state_->now)
        wakeup = steady_state_->now;
    scheduleTick(i, wakeup);
}

void
Simulator::tickNode(size_t i)
{
    if (i >= nodes_.size())
        return;
    auto& n = *nodes_[i];
    if (!n.runner)
        return;
    auto wakeup = n.runner->loop();
    scheduleNextWakeup(i, wakeup);
}

void
Simulator::runOne()
{
    Event e = queue_.top();
    queue_.pop();
    if (e.time > steady_state_->now)
        steady_state_->now = e.time;
    trace_.push_back(TraceRecord {e.time, e.kind, e.seq, e.a, e.b, e.c});
    switch (e.kind) {
    case EventKind::Timer:
        ++metrics_.timer_events;
        break;
    case EventKind::PacketArrival:
        ++metrics_.packet_events;
        break;
    case EventKind::Workload:
        ++metrics_.workload_events;
        break;
    }
    e.run();
}

std::pair<SockAddr, SockAddr>
Simulator::makeNodeAddrs(size_t i)
{
    std::uniform_int_distribution<uint16_t> portDist(1024, 65535);

    SockAddr a4, a6;
    bool bind4 = true, bind6 = true;
    // Simulate some nodes being single-stack by randomly failing to bind one of the families.
    // (Only applied to i > 0 to ensure node 0 is always dual-stack for bootstrapping.)
    if (i > 0 && cfg_.bind_failure_probability > 0.0) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        if (dist(rng_) < cfg_.bind_failure_probability) {
            if (dist(rng_) < 0.5)
                bind4 = false;
            else
                bind6 = false;
        }
    }

    if (bind4) {
        a4.setFamily(AF_INET);
        char buf4[32];
        std::snprintf(buf4, sizeof(buf4), "10.%zu.%zu.%zu", (i / (254 * 254)) % 255, (i / 254) % 254, (i % 254) + 1);
        a4.setAddress(buf4);
        a4.setPort(portDist(rng_));
    }

    if (bind6) {
        a6.setFamily(AF_INET6);
        char buf6[64];
        std::snprintf(buf6, sizeof(buf6), "fd00::%zx", i + 1);
        a6.setAddress(buf6);
        a6.setPort(portDist(rng_));
    }

    return {a4, a6};
}

void
Simulator::buildNodes()
{
    switch (cfg_.packet_recorder) {
    case PacketRecorderKind::None:
        recorder_.reset();
        break;
    case PacketRecorderKind::InMemory:
        recorder_ = std::make_shared<InMemoryPacketRecorder>();
        break;
    case PacketRecorderKind::Jsonl:
        recorder_ = JsonlPacketRecorder::openFile(cfg_.packet_recorder_file);
        break;
    }
    auto state = steady_state_;
    network_ = std::make_shared<SimNetwork>(
        cfg_.latency,
        cfg_.drop_probability,
        rng_,
        [this](SimSocket& dst_sock, const SimSocket& src_sock, SockAddr src, Blob data, std::chrono::nanoseconds latency) {
            auto deliver_at = steady_state_->now + latency;
            auto size = static_cast<uint32_t>(data.size());
            enqueue(
                deliver_at,
                EventKind::PacketArrival,
                [this, &dst_sock, &src_sock, src = std::move(src), data = std::move(data)]() mutable {
                    dst_sock.deliver(src, std::move(data));
                    // Drain rcv on the destination immediately after delivery.
                    tickNode(dst_sock.nodeId());
                },
                static_cast<uint32_t>(dst_sock.nodeId()),
                static_cast<uint32_t>(src_sock.nodeId()),
                size);
        },
        recorder_,
        [state]() { return state->now; });

    nodes_.reserve(cfg_.node_count);
    for (size_t i = 0; i < cfg_.node_count; ++i) {
        auto n = std::make_unique<SimNode>();
        n->id = i;
        auto [addr4, addr6] = makeNodeAddrs(i);
        n->addr4 = addr4;
        n->addr6 = addr6;

        std::chrono::system_clock::duration skew {0};
        if (cfg_.system_clock_skew_max.count() > 0) {
            std::uniform_int_distribution<long long> d(-cfg_.system_clock_skew_max.count(),
                                                       cfg_.system_clock_skew_max.count());
            skew = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds {d(rng_)});
        }
        n->clock = std::make_shared<SimClock>(steady_state_, skew);
        n->runner = std::make_shared<DhtRunner>();

        DhtRunner::Config rcfg {};
        rcfg.threaded = false;
        if (cfg_.identity_cache) {
            try {
                rcfg.dht_config.id = cfg_.identity_cache->get(cfg_.identity_seed, i);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Simulator: identity_cache->get failed for node ")
                                         + std::to_string(i) + ": " + e.what());
            }
        }

        DhtRunner::Context rctx;
        rctx.time = n->clock;
        rctx.sock = std::make_unique<SimSocket>(i, n->addr4, n->addr6, network_, steady_state_);
        if (cfg_.logger_override)
            rctx.logger = cfg_.logger_override;
        else if (cfg_.verbose)
            rctx.logger = makeSimLogger(steady_state_, i);
        rctx.rng = std::make_unique<std::mt19937_64>(std::uniform_int_distribution<uint64_t>()(rng_));

        n->runner->run(rcfg, std::move(rctx));

        nodes_.push_back(std::move(n));
    }

    // Schedule an initial tick for the bootstrap node to start its event loop.
    scheduleTick(0, steady_state_->now);
}

// ---------- public Simulator API --------------------------------------------

Simulator::Simulator(SimConfig cfg)
    : cfg_(std::move(cfg))
    , rng_(cfg_.seed)
{
    buildNodes();
}

Simulator::~Simulator()
{
    // Tear nodes down in reverse order. join() destroys the dht_, which owns
    // the SimSocket (which then unregisters from SimNetwork in its destructor).
    for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
        if ((*it)->runner)
            (*it)->runner->join();
    }
}

SimNode&
Simulator::node(size_t i)
{
    return *nodes_.at(i);
}

size_t
Simulator::nodeCount() const
{
    return nodes_.size();
}

void
Simulator::bootstrapAll(std::chrono::milliseconds maxDuration)
{
    std::uniform_int_distribution<uint64_t> dist(0, maxDuration.count());
    for (size_t i = 1; i < nodes_.size(); ++i) {
        auto bootstrap_at = steady_state_->now + std::chrono::milliseconds {dist(rng_)};
        enqueue(bootstrap_at, EventKind::Workload, [this, i]() {
            auto& boot = *nodes_[0];
            nodes_[i]->runner->bootstrap(boot.addr4);
            nodes_[i]->runner->bootstrap(boot.addr6);
            scheduleTick(i, steady_state_->now);
        });
    }
}

Simulator::time_point
Simulator::now() const
{
    return steady_state_->now;
}

void
Simulator::runFor(std::chrono::nanoseconds d)
{
    runUntil(steady_state_->now + d);
}

void
Simulator::runUntil(time_point deadline)
{
    while (!queue_.empty() && queue_.top().time <= deadline)
        runOne();
    if (deadline > steady_state_->now)
        steady_state_->now = deadline;
}

bool
Simulator::runUntil(std::function<bool()> predicate, std::chrono::nanoseconds timeout)
{
    auto deadline = steady_state_->now + timeout;
    while (!predicate()) {
        if (queue_.empty() || queue_.top().time > deadline) {
            steady_state_->now = deadline;
            return predicate();
        }
        runOne();
    }
    return true;
}

std::mt19937_64&
Simulator::rng()
{
    return rng_;
}

void
Simulator::scheduleAt(time_point at, std::function<void()> fn)
{
    enqueue(at, EventKind::Workload, std::move(fn));
}

std::shared_ptr<SimNetwork>
Simulator::network() const
{
    return network_;
}

const Simulator::Metrics&
Simulator::metrics() const noexcept
{
    return metrics_;
}

std::shared_ptr<PacketRecorder>
Simulator::packetRecorder() const
{
    return recorder_;
}

void
Simulator::scheduleTick(size_t i, time_point at)
{
    enqueue(at, EventKind::Timer, [this, i]() { tickNode(i); }, static_cast<uint32_t>(i));
}

const std::vector<TraceRecord>&
Simulator::trace() const
{
    return trace_;
}

std::string
Simulator::traceHash() const
{
    // SHA-256 over the binary footprint of the trace, truncated to 16 hex chars.
    Blob input;
    input.reserve(trace_.size() * 32);
    auto append = [&](const void* data, size_t n) {
        const auto* p = static_cast<const uint8_t*>(data);
        input.insert(input.end(), p, p + n);
    };
    for (const auto& r : trace_) {
        auto t = r.time.time_since_epoch().count();
        append(&t, sizeof(t));
        append(&r.kind, sizeof(r.kind));
        append(&r.seq, sizeof(r.seq));
        append(&r.a, sizeof(r.a));
        append(&r.b, sizeof(r.b));
        append(&r.c, sizeof(r.c));
    }
    auto digest = crypto::hash(input, 32); // SHA-256
    return toHex(digest.data(), 8);        // first 8 bytes -> 16 hex chars
}

} // namespace sim
} // namespace dht
