// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../infohash.h"
#include "../utils.h"
#include "workload.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace dht {
namespace sim {

/**
 * Phase 3 workload #1: write N random key/value pairs from node 0, then read
 * them back from node `reader` and assert each value is found.
 *
 * Settles routing tables via `bootstrapAll()` first if more than one node is
 * present. The list of (key, expected_blob) pairs is held internally; verify()
 * checks that the value seen during the get matches the value originally put.
 */
class OPENDHT_PUBLIC PutGetWorkload : public Workload
{
public:
    struct Options
    {
        size_t op_count {16};
        size_t reader {0}; ///< node index that issues gets (default: node 0)
        size_t writer {0}; ///< node index that issues puts (default: node 0)
        std::chrono::seconds settle_timeout {30};
        std::chrono::seconds put_timeout {30};
        std::chrono::seconds get_timeout {30};
    };

    PutGetWorkload() = default;
    explicit PutGetWorkload(Options o)
        : opt_(o)
    {}

    void run(Simulator& sim) override;
    bool verify(Simulator& sim, std::string& error_out) override;
    std::string_view name() const override { return "PutGet"; }

private:
    Options opt_;
    struct Entry
    {
        InfoHash key;
        Blob expected;
        std::atomic<bool> put_ok {false};
        std::atomic<bool> get_ok {false};
    };
    std::vector<std::unique_ptr<Entry>> entries_;
};

/**
 * Phase 3 workload #2: one node `listener` registers a listen on a key; another
 * node `writer` puts to that key; verify() asserts the listener fired at least
 * `expected_hits` times.
 */
class OPENDHT_PUBLIC ListenPutWorkload : public Workload
{
public:
    struct Options
    {
        size_t listener {0};
        size_t writer {1};
        size_t expected_hits {1};
        std::chrono::seconds settle_timeout {30};
        std::chrono::seconds run_timeout {30};
    };

    ListenPutWorkload() = default;
    explicit ListenPutWorkload(Options o)
        : opt_(o)
    {}

    void run(Simulator& sim) override;
    bool verify(Simulator& sim, std::string& error_out) override;
    std::string_view name() const override { return "ListenPut"; }

private:
    Options opt_;
    InfoHash key_ {};
    std::atomic<size_t> hits_ {0};
};

} // namespace sim
} // namespace dht
