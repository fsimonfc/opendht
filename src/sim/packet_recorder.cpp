// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/packet_recorder.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <fstream>
#include <ostream>
#include <stdexcept>

namespace dht {
namespace sim {

void
InMemoryPacketRecorder::record(const PacketRecord& p)
{
    std::lock_guard<std::mutex> lk(mu_);
    packets_.push_back(p);
}

JsonlPacketRecorder::JsonlPacketRecorder(std::ostream& out)
    : out_(out)
{}

std::shared_ptr<JsonlPacketRecorder>
JsonlPacketRecorder::openFile(const std::string& path)
{
    auto ofs = std::make_shared<std::ofstream>(path, std::ios::out | std::ios::trunc);
    if (!ofs->is_open())
        throw std::runtime_error("JsonlPacketRecorder: cannot open " + path);
    auto rec = std::shared_ptr<JsonlPacketRecorder>(new JsonlPacketRecorder(*ofs));
    rec->out_owner_ = ofs;
    return rec;
}

void
JsonlPacketRecorder::record(const PacketRecord& p)
{
    std::lock_guard<std::mutex> lk(mu_);
    fmt::print(out_,
               "{{\"t\":{},\"src\":{},\"dst\":{},\"size\":{},\"dropped\":{}}}\n",
               p.t.time_since_epoch().count(),
               p.src,
               p.dst,
               p.size,
               p.dropped ? "true" : "false");
}

void
JsonlPacketRecorder::flush()
{
    std::lock_guard<std::mutex> lk(mu_);
    out_.flush();
}

} // namespace sim
} // namespace dht
