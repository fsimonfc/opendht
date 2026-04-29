// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/sim_socket.h"
#include "opendht/sim/sim_network.h"

namespace dht {
namespace sim {

SimSocket::SimSocket(size_t node_id,
                     SockAddr addr,
                     std::shared_ptr<SimNetwork> net,
                     std::shared_ptr<SimClock::SteadyState> clock_state)
    : node_id_(node_id)
    , bound_(std::move(addr))
    , network_(std::move(net))
    , clock_state_(std::move(clock_state))
{
    if (network_)
        network_->registerSocket(*this);
}

SimSocket::~SimSocket()
{
    if (network_)
        network_->unregisterSocket(bound_);
}

const SockAddr&
SimSocket::getBoundRef(sa_family_t family) const
{
    if (family == AF_UNSPEC || family == bound_.getFamily())
        return bound_;
    return unbound_;
}

int
SimSocket::sendTo(const SockAddr& dest, const uint8_t* data, size_t size, bool /*replied*/)
{
    if (stopped_ || !network_)
        return -1;
    return network_->send(bound_, dest, data, size);
}

void
SimSocket::stop()
{
    stopped_ = true;
}

void
SimSocket::deliver(const SockAddr& from, Blob data)
{
    if (stopped_)
        return;
    net::PacketList pkts = getNewPacket();
    auto& pkt = pkts.front();
    pkt.data = std::move(data);
    pkt.from = from;
    pkt.received = clock_state_ ? clock_state_->now : std::chrono::steady_clock::time_point {};
    onReceived(std::move(pkts));
}

} // namespace sim
} // namespace dht
