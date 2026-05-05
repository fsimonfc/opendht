// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "def.h"
#include "infohash.h"
#include "utils.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace dht {

class Value;

enum class OpCode : uint8_t {
    Put = 1,
    Get = 2,
    Listen = 3,
    CancelListen = 4,
    Bootstrap = 5,
};

/**
 * A self-contained description of a single DHT operation to be performed on a
 * node.
 */
struct OPENDHT_PUBLIC NodeOp
{
    OpCode code;
    size_t node_id {0};    // logical node index
    InfoHash key;          // target key (meaningful for Put/Get/Listen)
    uint64_t value_id {0}; // optional value identifier
    Blob payload;          // data for Put; empty for Get/Listen/Bootstrap
    // --- callbacks (used at execution time; not serialized) ---
    std::function<void(bool)> on_done;
    std::function<bool(const std::vector<Sp<Value>>&)> on_values;
};

} // namespace dht
