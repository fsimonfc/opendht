// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "test_simulation.h"

#include <opendht/sim/simulator.h>
#include <opendht/sim/sim_clock.h>
#include <opendht/sim/sim_network.h>

#include <chrono>

namespace test {

CPPUNIT_TEST_SUITE_REGISTRATION(SimulationTester);

using namespace std::chrono_literals;
using dht::sim::SimConfig;
using dht::sim::Simulator;

void
SimulationTester::testSimClockAdvance()
{
    SimConfig cfg;
    cfg.node_count = 1;
    Simulator sim(cfg);
    auto t0 = sim.now();
    sim.runFor(500ms);
    CPPUNIT_ASSERT_EQUAL((sim.now() - t0).count(), std::chrono::nanoseconds(500ms).count());
}

void
SimulationTester::testSingleNodeStart()
{
    // Single non-threaded node should bring up its DHT, run a few maintenance
    // ticks, and not crash.
    SimConfig cfg;
    cfg.node_count = 1;
    Simulator sim(cfg);
    sim.runFor(2s);
    auto& n = sim.node(0);
    CPPUNIT_ASSERT(n.runner != nullptr);
    CPPUNIT_ASSERT(n.runner->getNodeId().toString().size() > 0);
}

void
SimulationTester::testTwoNodePacketDelivery()
{
    // Bring up two nodes, bootstrap node 1 -> node 0, and verify that within
    // the simulated time some packets are exchanged (i.e. the routing tables
    // are non-empty on both ends).
    SimConfig cfg;
    cfg.node_count = 2;
    cfg.latency = 10ms;
    Simulator sim(cfg);
    sim.bootstrapAll();

    sim.runUntil(
        [&]() {
            return sim.node(0).runner->getNodesStats(AF_INET).good_nodes > 0
                   && sim.node(1).runner->getNodesStats(AF_INET).good_nodes > 0;
        },
        30s);

    CPPUNIT_ASSERT(sim.node(0).runner->getNodesStats(AF_INET).good_nodes > 0);
    CPPUNIT_ASSERT(sim.node(1).runner->getNodesStats(AF_INET).good_nodes > 0);
}

void
SimulationTester::testMultiNodeBootstrap()
{
    // Bring up a small cluster, bootstrap each non-zero node to node(0), and
    // assert that within the simulated time everyone has at least one good
    // contact in the routing table.
    SimConfig cfg;
    cfg.node_count = 5;
    cfg.latency = 5ms;
    Simulator sim(cfg);
    sim.bootstrapAll();

    sim.runUntil(
        [&]() {
            for (size_t i = 0; i < sim.nodeCount(); ++i)
                if (sim.node(i).runner->getNodesStats(AF_INET).good_nodes == 0)
                    return false;
            return true;
        },
        60s);

    for (size_t i = 0; i < sim.nodeCount(); ++i)
        CPPUNIT_ASSERT(sim.node(i).runner->getNodesStats(AF_INET).good_nodes > 0);
}

void
SimulationTester::testDeterministicTrace()
{
    // Two simulators with the same SimConfig must produce equal event traces.
    auto runOnce = [](uint64_t seed) {
        SimConfig cfg;
        cfg.seed = seed;
        cfg.node_count = 3;
        cfg.latency = 7ms;
        Simulator sim(cfg);
        sim.bootstrapAll();
        sim.runFor(20s);
        return std::pair {sim.trace(), sim.traceHash()};
    };

    auto [a_trace, a_hash] = runOnce(0xABCDEF);
    auto [b_trace, b_hash] = runOnce(0xABCDEF);

    CPPUNIT_ASSERT_EQUAL(a_trace.size(), b_trace.size());
    CPPUNIT_ASSERT(a_trace.size() > 0);
    CPPUNIT_ASSERT(a_trace == b_trace);
    CPPUNIT_ASSERT_EQUAL(a_hash, b_hash);

    // A different seed should not (in any reasonable case) produce the same
    // hash for a non-trivial run. This is the canary for "is anything actually
    // varying with the seed".
    auto [c_trace, c_hash] = runOnce(0x123456);
    CPPUNIT_ASSERT(a_hash != c_hash);
    (void) c_trace;
}

} // namespace test
