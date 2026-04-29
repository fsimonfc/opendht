// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/workloads.h"
#include "opendht/sim/simulator.h"

#include <fmt/format.h>

namespace dht {
namespace sim {

using namespace std::chrono_literals;

// ---- PutGetWorkload ---------------------------------------------------------

void
PutGetWorkload::run(Simulator& sim)
{
    if (sim.nodeCount() < 1)
        return;
    if (sim.nodeCount() > 1) {
        sim.bootstrapAll();
        sim.runUntil(
            [&]() {
                for (size_t i = 0; i < sim.nodeCount(); ++i)
                    if (sim.node(i).runner->getNodesStats(AF_INET).good_nodes == 0)
                        return false;
                return true;
            },
            opt_.settle_timeout);
    }

    const size_t writer = std::min(opt_.writer, sim.nodeCount() - 1);
    const size_t reader = std::min(opt_.reader, sim.nodeCount() - 1);

    auto& wnode = sim.node(writer);
    entries_.reserve(opt_.op_count);
    for (size_t i = 0; i < opt_.op_count; ++i) {
        auto e = std::make_unique<Entry>();
        e->key = InfoHash::get(
            fmt::format("putget-key-{:08x}-{:04x}", static_cast<uint32_t>(i), static_cast<uint16_t>(i * 7919)));
        std::string payload = fmt::format("v{:08x}", static_cast<uint32_t>(0xC0FFEE + i));
        e->expected.assign(payload.begin(), payload.end());
        Value v(e->expected);
        Entry* eptr = e.get();
        wnode.runner->put(eptr->key, std::move(v), [eptr](bool ok) { eptr->put_ok = ok; });
        entries_.push_back(std::move(e));
    }

    sim.runUntil(
        [&]() {
            for (auto& e : entries_)
                if (!e->put_ok.load())
                    return false;
            return true;
        },
        opt_.put_timeout);

    auto& rnode = sim.node(reader);
    for (auto& e : entries_) {
        Entry* eptr = e.get();
        rnode.runner->get(
            eptr->key,
            [eptr](const std::vector<Sp<Value>>& vals) {
                for (auto& v : vals) {
                    if (v && v->data == eptr->expected) {
                        eptr->get_ok = true;
                        return false; // stop iterating; we found ours
                    }
                }
                return true;
            },
            [](bool /*done*/) {});
    }

    sim.runUntil(
        [&]() {
            for (auto& e : entries_)
                if (!e->get_ok.load())
                    return false;
            return true;
        },
        opt_.get_timeout);
}

bool
PutGetWorkload::verify(Simulator&, std::string& error_out)
{
    size_t put_failed = 0, get_failed = 0;
    for (auto& e : entries_) {
        if (!e->put_ok.load())
            ++put_failed;
        if (!e->get_ok.load())
            ++get_failed;
    }
    if (put_failed || get_failed) {
        error_out = fmt::format("PutGet: {} put failed, {} get failed (of {})", put_failed, get_failed, entries_.size());
        return false;
    }
    return true;
}

// ---- ListenPutWorkload ------------------------------------------------------

void
ListenPutWorkload::run(Simulator& sim)
{
    if (sim.nodeCount() < 2) {
        // Need at least two nodes for this workload to make sense.
        return;
    }
    sim.bootstrapAll();
    sim.runUntil(
        [&]() {
            for (size_t i = 0; i < sim.nodeCount(); ++i)
                if (sim.node(i).runner->getNodesStats(AF_INET).good_nodes == 0)
                    return false;
            return true;
        },
        opt_.settle_timeout);

    const size_t listener = std::min(opt_.listener, sim.nodeCount() - 1);
    size_t writer = std::min(opt_.writer, sim.nodeCount() - 1);
    if (writer == listener)
        writer = (listener + 1) % sim.nodeCount();

    key_ = InfoHash::get("listenput-key");

    sim.node(listener).runner->listen(key_, [this](const std::vector<Sp<Value>>& vals) {
        hits_.fetch_add(vals.size());
        return true;
    });

    // Give the listen a chance to register on the network before publishing.
    sim.runFor(200ms);

    for (size_t i = 0; i < opt_.expected_hits; ++i) {
        auto payload = fmt::format("listenput-{:04x}", static_cast<uint16_t>(i));
        Value v {Blob(payload.begin(), payload.end())};
        v.id = static_cast<uint64_t>(i + 1);
        sim.node(writer).runner->put(key_, std::move(v));
    }

    sim.runUntil([this]() { return hits_.load() >= opt_.expected_hits; }, opt_.run_timeout);
}

bool
ListenPutWorkload::verify(Simulator&, std::string& error_out)
{
    if (hits_.load() < opt_.expected_hits) {
        error_out = fmt::format("ListenPut: only {} hits (expected >= {})", hits_.load(), opt_.expected_hits);
        return false;
    }
    return true;
}

} // namespace sim
} // namespace dht
