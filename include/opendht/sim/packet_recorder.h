// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace dht {
namespace sim {

/** Per-packet record produced by `SimNetwork::send`. */
struct PacketRecord
{
    std::chrono::steady_clock::time_point t;
    std::size_t src;
    std::size_t dst;
    std::size_t size;
    bool dropped;
};

/**
 * Sink for per-packet records. `SimNetwork` calls `record()` once per packet
 * (including drops) and `flush()` on shutdown.
 */
class OPENDHT_PUBLIC PacketRecorder
{
public:
    virtual ~PacketRecorder() = default;
    virtual void record(const PacketRecord& p) = 0;
    virtual void flush() {}
};

/** Selects which `PacketRecorder` implementation `Simulator` should build. */
enum class PacketRecorderKind : uint8_t {
    None = 0,
    InMemory = 1,
    Jsonl = 2,
};

/** Keeps every record in a `std::vector` for in-process inspection (tests). */
class OPENDHT_PUBLIC InMemoryPacketRecorder final : public PacketRecorder
{
public:
    void record(const PacketRecord& p) override;
    const std::vector<PacketRecord>& packets() const noexcept { return packets_; }

private:
    std::vector<PacketRecord> packets_;
};

/** One JSON object per packet, newline-delimited, written to `out`. */
class OPENDHT_PUBLIC JsonlPacketRecorder final : public PacketRecorder
{
public:
    explicit JsonlPacketRecorder(std::ostream& out);

    /** Convenience: open a file by path. Throws on failure. */
    static std::shared_ptr<JsonlPacketRecorder> openFile(const std::string& path);

    void record(const PacketRecord& p) override;
    void flush() override;

private:
    std::shared_ptr<std::ostream> out_owner_;
    std::ostream& out_;
};

} // namespace sim
} // namespace dht
