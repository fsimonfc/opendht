// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/node_op_recorder.h"
#include "opendht/node_op.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <fstream>
#include <ostream>
#include <stdexcept>

namespace dht {
namespace sim {

static const char*
opCodeStr(OpCode c)
{
    switch (c) {
    case OpCode::Put:
        return "Put";
    case OpCode::Get:
        return "Get";
    case OpCode::Listen:
        return "Listen";
    case OpCode::CancelListen:
        return "CancelListen";
    case OpCode::Bootstrap:
        return "Bootstrap";
    }
    return "Unknown";
}

void
InMemoryNodeOpRecorder::record(const NodeOp& op, std::chrono::steady_clock::time_point t, uint64_t seq)
{
    records_.push_back(NodeOpRecord {t, seq, op.code, op.node_id, op.key, op.value_id, op.payload.size()});
}

JsonlNodeOpRecorder::JsonlNodeOpRecorder(std::ostream& out)
    : out_(out)
{}

std::shared_ptr<JsonlNodeOpRecorder>
JsonlNodeOpRecorder::openFile(const std::string& path)
{
    auto ofs = std::make_shared<std::ofstream>(path, std::ios::out | std::ios::trunc);
    if (!ofs->is_open())
        throw std::runtime_error("JsonlNodeOpRecorder: cannot open " + path);
    auto rec = std::shared_ptr<JsonlNodeOpRecorder>(new JsonlNodeOpRecorder(*ofs));
    rec->out_owner_ = ofs;
    return rec;
}

void
JsonlNodeOpRecorder::record(const NodeOp& op, std::chrono::steady_clock::time_point t, uint64_t seq)
{
    fmt::print(out_,
               "{{\"t\":{},\"seq\":{},\"op\":\"{}\",\"node\":{},\"key\":\"{}\",\"value_id\":{},\"payload_size\":{}}}\n",
               t.time_since_epoch().count(),
               seq,
               opCodeStr(op.code),
               op.node_id,
               op.key.toString(),
               op.value_id,
               op.payload.size());
}

void
JsonlNodeOpRecorder::flush()
{
    out_.flush();
}

} // namespace sim
} // namespace dht
