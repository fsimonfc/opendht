// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../infohash.h"
#include "../node_op.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace dht {
namespace sim {

/**
 * A single recorded node operation.
 */
struct NodeOpRecord
{
    std::chrono::steady_clock::time_point time;
    uint64_t seq;
    OpCode code;
    size_t node_id;
    InfoHash key;
    uint64_t value_id;
    size_t payload_size;
};

} // namespace sim
} // namespace dht
