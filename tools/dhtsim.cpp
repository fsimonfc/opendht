// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
//
// dhtsim — minimal CLI driver for the deterministic DHT simulator.
// See SIMULATION_DESIGN.md §7 for the planned option surface.

#include <opendht/sim/identity_cache.h>
#include <opendht/sim/packet_recorder.h>
#include <opendht/sim/sim_network.h>
#include <opendht/sim/simulator.h>
#include <opendht/sim/workloads.h>

#ifndef _WIN32
#include <getopt.h>
#else
#include "wingetopt.h"
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

void
printUsage(const char* prog)
{
    std::fprintf(stderr,
                 "Usage: %s [options]\n"
                 "--seed N                    simulator seed (default 0xC0FFEE)\n"
                 "--identity-seed N           crypto identity seed (default 0xDEADBEEF)\n"
                 "--identities                generate per-node crypto identities (in-memory cache)\n"
                 "--identity-cache-dir DIR    persistent identity cache directory (implies -i)\n"
                 "--nodes N                   number of simulated nodes (default 4)\n"
                 "--duration S                when no workload, free-run for S seconds (default 60)\n"
                 "--workload NAME             one of: PutGet, ListenPut\n"
                 "--latency MS                one-way latency in ms (default 20)\n"
                 "--drop P                    packet drop probability 0.0-1.0 (default 0)\n"
                 "--system-clock-skew-max MS  per-node systemNow() skew bound (default 0)\n"
                 "--packet-recorder-file PATH record packets as JSONL to PATH\n"
                 "--counters                  print event/network counters at end\n"
                 "--trace-hash                print hash of the event trace\n"
                 "--list-workloads            list available workloads and exit\n"
                 "-v, --verbose               enable per-node logging\n"
                 "-h, --help                  this message\n",
                 prog);
}

// clang-format off
static const struct option long_options[] = {
    {"seed",                  required_argument, nullptr, 's'},
    {"identity-seed",         required_argument, nullptr, 'I'},
    {"identities",            no_argument,       nullptr, 'i'},
    {"identity-cache-dir",    required_argument, nullptr, 'C'},
    {"nodes",                 required_argument, nullptr, 'n'},
    {"duration",              required_argument, nullptr, 'd'},
    {"workload",              required_argument, nullptr, 'w'},
    {"latency",               required_argument, nullptr, 'l'},
    {"drop",                  required_argument, nullptr, 'D'},
    {"system-clock-skew-max", required_argument, nullptr, 'S'},
    {"packet-recorder-file",  required_argument, nullptr, 'r'},
    {"counters",              no_argument,       nullptr, 'c'},
    {"verbose",               no_argument,       nullptr, 'v'},
    {"trace-hash",            no_argument,       nullptr, 't'},
    {"list-workloads",        no_argument,       nullptr, 'L'},
    {"help",                  no_argument,       nullptr, 'h'},
    {nullptr,                 0,                 nullptr,  0 }
};
// clang-format on

} // namespace

int
main(int argc, char** argv)
{
    using namespace std::chrono_literals;
    using namespace dht::sim;

    SimConfig cfg;
    cfg.node_count = 4;
    std::chrono::seconds duration {60};
    std::string workload_name;
    bool trace_hash = false;
    bool list_workloads = false;

    bool use_identities = false;
    std::string identity_cache_dir;

    bool print_counters = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "vh", long_options, nullptr)) != -1) {
        switch (opt) {
        case 's':
            cfg.seed = std::strtoull(optarg, nullptr, 0);
            break;
        case 'I':
            cfg.identity_seed = std::strtoull(optarg, nullptr, 0);
            break;
        case 'i':
            use_identities = true;
            break;
        case 'C':
            identity_cache_dir = optarg;
            use_identities = true;
            break;
        case 'n':
            cfg.node_count = std::strtoull(optarg, nullptr, 0);
            break;
        case 'd':
            duration = std::chrono::seconds {std::strtoull(optarg, nullptr, 0)};
            break;
        case 'w':
            workload_name = optarg;
            break;
        case 'l':
            cfg.latency = std::chrono::milliseconds {std::strtoull(optarg, nullptr, 0)};
            break;
        case 'D':
            cfg.drop_probability = std::strtod(optarg, nullptr);
            break;
        case 'S':
            cfg.system_clock_skew_max = std::chrono::milliseconds {std::strtoull(optarg, nullptr, 0)};
            break;
        case 'r':
            cfg.packet_recorder_file = optarg;
            cfg.packet_recorder = PacketRecorderKind::Jsonl;
            break;
        case 'c':
            print_counters = true;
            break;
        case 'v':
            cfg.verbose = true;
            break;
        case 't':
            trace_hash = true;
            break;
        case 'L':
            list_workloads = true;
            break;
        case 'h':
            printUsage(argv[0]);
            return 0;
        default:
            printUsage(argv[0]);
            return 2;
        }
    }

    if (list_workloads) {
        std::cout << "PutGet\n";
        std::cout << "ListenPut\n";
        return 0;
    }

    if (use_identities)
        cfg.identity_cache = std::make_shared<IdentityCache>(identity_cache_dir);

    std::unique_ptr<Workload> wl;
    if (workload_name == "PutGet") {
        wl = std::make_unique<PutGetWorkload>();
    } else if (workload_name == "ListenPut") {
        wl = std::make_unique<ListenPutWorkload>();
    } else if (!workload_name.empty()) {
        std::fprintf(stderr, "unknown workload: %.*s\n", static_cast<int>(workload_name.size()), workload_name.data());
        return 2;
    }

    Simulator sim(std::move(cfg));
    int rc = 0;
    if (wl) {
        wl->run(sim);
        std::string err;
        bool ok = wl->verify(sim, err);
        if (!ok) {
            std::fprintf(stderr,
                         "workload %.*s FAILED: %s\n",
                         static_cast<int>(wl->name().size()),
                         wl->name().data(),
                         err.c_str());
            rc = 1;
        } else {
            std::fprintf(stderr, "workload %.*s OK\n", static_cast<int>(wl->name().size()), wl->name().data());
        }
    } else {
        sim.bootstrapAll();
        sim.runFor(duration);
    }

    if (auto rec = sim.packetRecorder())
        rec->flush();

    if (print_counters) {
        const auto& nc = sim.network()->counters();
        const auto& ec = sim.counters();
        std::fprintf(stderr,
                     "network: packets_sent=%llu packets_dropped=%llu bytes_sent=%llu bytes_dropped=%llu\n"
                     "events:  timer=%llu packet=%llu workload=%llu\n",
                     static_cast<unsigned long long>(nc.packets_sent),
                     static_cast<unsigned long long>(nc.packets_dropped),
                     static_cast<unsigned long long>(nc.bytes_sent),
                     static_cast<unsigned long long>(nc.bytes_dropped),
                     static_cast<unsigned long long>(ec.timer_events),
                     static_cast<unsigned long long>(ec.packet_events),
                     static_cast<unsigned long long>(ec.workload_events));
    }

    if (trace_hash)
        std::fprintf(stderr, "trace_hash=%s\n", sim.traceHash().c_str());

    return rc;
}
