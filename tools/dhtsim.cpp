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

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

void
printUsage(const char* prog)
{
    std::fprintf(stderr,
                 "Usage: %s [options]\n"
                 "  --seed N                    simulator seed (default 0xC0FFEE)\n"
                 "  --identity-seed N           crypto identity seed (default 0xDEADBEEF)\n"
                 "  --identities                generate per-node crypto identities (in-memory cache)\n"
                 "  --identity-cache-dir DIR    persistent identity cache directory (implies --identities)\n"
                 "  --nodes N                   number of simulated nodes (default 4)\n"
                 "  --duration S                when no workload, free-run for S seconds (default 60)\n"
                 "  --workload NAME             one of: PutGet, ListenPut\n"
                 "  --latency MS                one-way latency in ms (default 20)\n"
                 "  --drop P                    packet drop probability 0.0-1.0 (default 0)\n"
                 "  --system-clock-skew-max MS  per-node systemNow() skew bound (default 0)\n"
                 "  --packet-recorder-file PATH record packets as JSONL to PATH\n"
                 "  --counters                  print event/network counters at end\n"
                 "  --verbose                   enable per-node logging\n"
                 "  --trace-hash                print FNV-1a hash of the event trace\n"
                 "  --list-workloads            list available workloads and exit\n"
                 "  --help                      this message\n",
                 prog);
}

uint64_t
parseUint(std::string_view s)
{
    return std::strtoull(std::string {s}.c_str(), nullptr, 0);
}

} // namespace

int
main(int argc, char** argv)
{
    using namespace std::chrono_literals;
    using namespace dht::sim;

    SimConfig cfg;
    cfg.node_count = 4;
    std::chrono::seconds duration {60};
    std::string_view workload_name;
    bool trace_hash = false;
    bool list_workloads = false;

    bool use_identities = false;
    std::string identity_cache_dir;

    bool print_counters = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        auto need = [&](int n) {
            if (i + n >= argc) {
                std::fprintf(stderr, "missing value for %.*s\n", static_cast<int>(a.size()), a.data());
                std::exit(2);
            }
        };
        if (a == "--help" || a == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (a == "--seed") {
            need(1);
            cfg.seed = parseUint(argv[++i]);
        } else if (a == "--identity-seed") {
            need(1);
            cfg.identity_seed = parseUint(argv[++i]);
        } else if (a == "--identities") {
            use_identities = true;
        } else if (a == "--identity-cache-dir") {
            need(1);
            identity_cache_dir = argv[++i];
            use_identities = true;
        } else if (a == "--nodes") {
            need(1);
            cfg.node_count = parseUint(argv[++i]);
        } else if (a == "--duration") {
            need(1);
            duration = std::chrono::seconds {parseUint(argv[++i])};
        } else if (a == "--workload") {
            need(1);
            workload_name = argv[++i];
        } else if (a == "--latency") {
            need(1);
            cfg.latency = std::chrono::milliseconds {parseUint(argv[++i])};
        } else if (a == "--drop") {
            need(1);
            cfg.drop_probability = std::strtod(std::string {argv[++i]}.c_str(), nullptr);
        } else if (a == "--system-clock-skew-max") {
            need(1);
            cfg.system_clock_skew_max = std::chrono::milliseconds {parseUint(argv[++i])};
        } else if (a == "--packet-recorder-file") {
            need(1);
            cfg.packet_recorder_file = argv[++i];
            cfg.packet_recorder = PacketRecorderKind::Jsonl;
        } else if (a == "--counters") {
            print_counters = true;
        } else if (a == "--verbose") {
            cfg.verbose = true;
        } else if (a == "--trace-hash") {
            trace_hash = true;
        } else if (a == "--list-workloads") {
            list_workloads = true;
        } else {
            std::fprintf(stderr, "unknown option: %.*s\n", static_cast<int>(a.size()), a.data());
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
