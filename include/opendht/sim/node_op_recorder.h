// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "node_op_record.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace dht {
struct NodeOp;
namespace sim {

/**
 * Records all node operations executed via Simulator::schedule(). Analogous to
 * PacketRecorder but for DHT-level operations (Put/Get/Listen/etc.).
 */
class OPENDHT_PUBLIC NodeOpRecorder
{
public:
    virtual ~NodeOpRecorder() = default;
    virtual void record(const NodeOp& op, std::chrono::steady_clock::time_point t, uint64_t seq) = 0;
    virtual void flush() {}
};

/** Keeps every record in a vector for in-process inspection (tests). */
class OPENDHT_PUBLIC InMemoryNodeOpRecorder final : public NodeOpRecorder
{
public:
    void record(const NodeOp& op, std::chrono::steady_clock::time_point t, uint64_t seq) override;
    const std::vector<NodeOpRecord>& records() const noexcept { return records_; }
    void clear() { records_.clear(); }

private:
    std::vector<NodeOpRecord> records_;
};

/** One JSON object per operation, newline-delimited, written to `out`. */
class OPENDHT_PUBLIC JsonlNodeOpRecorder final : public NodeOpRecorder
{
public:
    explicit JsonlNodeOpRecorder(std::ostream& out);

    /** Convenience: open a file by path. Throws on failure. */
    static std::shared_ptr<JsonlNodeOpRecorder> openFile(const std::string& path);

    void record(const NodeOp& op, std::chrono::steady_clock::time_point t, uint64_t seq) override;
    void flush() override;

private:
    std::shared_ptr<std::ostream> out_owner_;
    std::ostream& out_;
};

} // namespace sim
} // namespace dht
