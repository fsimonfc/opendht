// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/sim_network.h"
#include "opendht/sim/sim_socket.h"

#include <stdexcept>

namespace dht {
namespace sim {

namespace {
std::string
keyOf(const SockAddr& a)
{
    return a.toString();
}
} // namespace

SimNetwork::SimNetwork(std::chrono::milliseconds latency,
                       double drop_probability,
                       std::mt19937_64& rng,
                       DeliverHook deliver,
                       std::shared_ptr<PacketRecorder> recorder,
                       std::function<std::chrono::steady_clock::time_point()> now)
    : latency_(latency)
    , drop_probability_(drop_probability)
    , rng_(rng)
    , deliver_(std::move(deliver))
    , recorder_(std::move(recorder))
    , now_(std::move(now))
{
    if (!deliver_)
        throw std::invalid_argument {"SimNetwork: deliver hook required"};
}

void
SimNetwork::registerSocket(SimSocket& s)
{
    sockets_[keyOf(s.addr())] = &s;
}

void
SimNetwork::unregisterSocket(const SockAddr& addr)
{
    sockets_.erase(keyOf(addr));
}

SimSocket*
SimNetwork::find(const SockAddr& addr)
{
    auto it = sockets_.find(keyOf(addr));
    return it == sockets_.end() ? nullptr : it->second;
}

int
SimNetwork::send(const SockAddr& src, const SockAddr& dst, const uint8_t* data, size_t size)
{
    auto* dst_sock = find(dst);
    if (!dst_sock)
        return -1; // unknown destination => silently drop, mimics UDP behavior
    auto* src_sock = find(src);
    size_t src_id = src_sock ? src_sock->nodeId() : static_cast<size_t>(-1);
    size_t dst_id = dst_sock->nodeId();

    bool dropped = false;
    if (drop_probability_ > 0.0) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        dropped = dist(rng_) < drop_probability_;
    }

    if (dropped) {
        ++counters_.packets_dropped;
        counters_.bytes_dropped += size;
    } else {
        ++counters_.packets_sent;
        counters_.bytes_sent += size;
    }
    if (recorder_) {
        auto t = now_ ? now_() : std::chrono::steady_clock::time_point {};
        recorder_->record(PacketRecord {t, src_id, dst_id, size, dropped});
    }
    if (dropped)
        return static_cast<int>(size);
    Blob payload(data, data + size);
    deliver_(dst_id, src_id, src, std::move(payload), latency_);
    return static_cast<int>(size);
}

} // namespace sim
} // namespace dht
