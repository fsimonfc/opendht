// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "test_simulation_workloads.h"

#include <opendht/sim/simulator.h>
#include <opendht/sim/workloads.h>
#include <opendht/sim/latency_model.h>

#include <chrono>
#include <string>

namespace test {

CPPUNIT_TEST_SUITE_REGISTRATION(SimulationWorkloadsTester);

using namespace std::chrono_literals;
using dht::sim::FixedLatency;
using dht::sim::ListenPutWorkload;
using dht::sim::PutGetWorkload;
using dht::sim::SimConfig;
using dht::sim::Simulator;

void
SimulationWorkloadsTester::testPutGetWorkload()
{
    SimConfig cfg;
    cfg.node_count = 4;
    cfg.quiet = true;
    cfg.latency = std::make_shared<FixedLatency>(5ms);
    Simulator sim(cfg);

    PutGetWorkload::Options o;
    o.op_count = 4;
    o.writer = 0;
    o.reader = 1;
    PutGetWorkload w(o);
    w.run(sim);

    std::string err;
    bool ok = w.verify(sim, err);
    CPPUNIT_ASSERT_MESSAGE(err, ok);
}

void
SimulationWorkloadsTester::testListenPutWorkload()
{
    SimConfig cfg;
    cfg.node_count = 3;
    cfg.quiet = true;
    cfg.latency = std::make_shared<FixedLatency>(5ms);
    Simulator sim(cfg);

    ListenPutWorkload::Options o;
    o.listener = 0;
    o.writer = 1;
    o.expected_hits = 1;
    ListenPutWorkload w(o);
    w.run(sim);

    std::string err;
    bool ok = w.verify(sim, err);
    CPPUNIT_ASSERT_MESSAGE(err, ok);
}

void
SimulationWorkloadsTester::testRunWorkloadHelper()
{
    SimConfig cfg;
    cfg.node_count = 3;
    cfg.quiet = true;
    cfg.latency = std::make_shared<FixedLatency>(5ms);

    PutGetWorkload::Options o;
    o.op_count = 2;
    o.writer = 0;
    o.reader = 1;
    PutGetWorkload w(o);

    std::string err;
    bool ok = dht::sim::runWorkload(cfg, w, err);
    CPPUNIT_ASSERT_MESSAGE(err, ok);
}

} // namespace test
