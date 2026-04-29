// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../network_utils.h"
#include "../sockaddr.h"
#include "sim_clock.h"

#include <memory>

namespace dht {
namespace sim {

class SimNetwork;

/**
 * In-memory DatagramSocket implementation. `sendTo()` forwards to the owning
 * SimNetwork, which decides latency / loss and enqueues a delivery event.
 * `deliver()` is called by the simulator when a packet arrives; it invokes the
 * `OnReceive` callback registered by `DhtRunner`.
 */
class SimSocket : public net::DatagramSocket
{
public:
    SimSocket(size_t node_id,
              SockAddr addr,
              std::shared_ptr<SimNetwork> net,
              std::shared_ptr<SimClock::SteadyState> clock_state);
    ~SimSocket() override;

    int sendTo(const SockAddr& dest, const uint8_t* data, size_t size, bool replied) override;

    bool hasIPv4() const override { return bound_.getFamily() == AF_INET; }
    bool hasIPv6() const override { return bound_.getFamily() == AF_INET6; }

    const SockAddr& getBoundRef(sa_family_t family = AF_UNSPEC) const override;

    void stop() override;

    /** Called by SimNetwork on packet arrival; invokes the registered OnReceive. */
    void deliver(const SockAddr& from, Blob data);

    const SockAddr& addr() const { return bound_; }
    size_t nodeId() const { return node_id_; }

private:
    size_t node_id_;
    SockAddr bound_;
    SockAddr unbound_ {};
    std::shared_ptr<SimNetwork> network_;
    std::shared_ptr<SimClock::SteadyState> clock_state_;
    bool stopped_ {false};
};

} // namespace sim
} // namespace dht
