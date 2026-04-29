// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../sockaddr.h"
#include "../utils.h"
#include "latency_model.h"
#include "packet_recorder.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <unordered_map>

namespace dht {
namespace sim {

class SimSocket;

/**
 * Routes packets between SimSockets. Owned by the Simulator; one per simulated
 * cluster. Per-socket registration is keyed by a stable string form of the
 * SockAddr so that packet routing never depends on pointer values.
 */
class SimNetwork : public std::enable_shared_from_this<SimNetwork>
{
public:
    struct Counters
    {
        uint64_t packets_sent {0};
        uint64_t packets_dropped {0};
        uint64_t bytes_sent {0};
        uint64_t bytes_dropped {0};
    };

    using DeliverHook = std::function<
        void(size_t dst_node_id, size_t src_node_id, SockAddr src, Blob data, std::chrono::nanoseconds latency)>;

    SimNetwork(std::shared_ptr<LatencyModel> latency,
               std::mt19937_64& rng,
               DeliverHook deliver,
               std::shared_ptr<PacketRecorder> recorder = {},
               std::function<std::chrono::steady_clock::time_point()> now = {});

    void registerSocket(SimSocket& s);
    void unregisterSocket(const SockAddr& addr);

    int send(const SockAddr& src, const SockAddr& dst, const uint8_t* data, size_t size);

    SimSocket* find(const SockAddr& addr);

    const Counters& counters() const noexcept { return counters_; }

private:
    std::shared_ptr<LatencyModel> latency_;
    std::mt19937_64& rng_;
    DeliverHook deliver_;
    std::shared_ptr<PacketRecorder> recorder_;
    std::function<std::chrono::steady_clock::time_point()> now_;
    std::unordered_map<std::string, SimSocket*> sockets_;
    Counters counters_ {};
};

} // namespace sim
} // namespace dht
