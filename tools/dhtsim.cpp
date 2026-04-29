// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
//
// dhtsim — minimal CLI driver for the deterministic DHT simulator.
// See SIMULATION_DESIGN.md §7 for the planned option surface.

#include <opendht/sim/identity_cache.h>
#include <opendht/sim/latency_model.h>
#include <opendht/sim/metric_sink.h>
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
                 "  --duration MS               when no workload, free-run for MS ms (default 5000)\n"
                 "  --workload NAME             one of: PutGet, ListenPut\n"
                 "  --latency-min MS            min latency in ms (default 10)\n"
                 "  --latency-max MS            max latency in ms (default = latency-min => fixed)\n"
                 "  --system-clock-skew-max MS  per-node systemNow() skew bound (default 0)\n"
                 "  --metrics-file PATH         write JSONL metrics to PATH\n"
                 "  --metrics-channels CSV      enable channels (default: ops,logs)\n"
                 "                              tokens: packets,ops,logs,timers,all,none\n"
                 "  --metrics-counters          print aggregated counters at end\n"
                 "  --quiet                     suppress per-node logging\n"
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
    std::chrono::milliseconds duration {5000};
    std::chrono::milliseconds lat_min {10}, lat_max {0};
    std::string_view workload_name;
    bool trace_hash = false;
    bool list_workloads = false;

    bool use_identities = false;
    std::string identity_cache_dir;

    std::string metrics_file;
    MetricMask metrics_mask = MetricMask::defaults();
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
            duration = std::chrono::milliseconds {parseUint(argv[++i])};
        } else if (a == "--workload") {
            need(1);
            workload_name = argv[++i];
        } else if (a == "--latency-min") {
            need(1);
            lat_min = std::chrono::milliseconds {parseUint(argv[++i])};
        } else if (a == "--latency-max") {
            need(1);
            lat_max = std::chrono::milliseconds {parseUint(argv[++i])};
        } else if (a == "--system-clock-skew-max") {
            need(1);
            cfg.system_clock_skew_max = std::chrono::milliseconds {parseUint(argv[++i])};
        } else if (a == "--metrics-file") {
            need(1);
            metrics_file = argv[++i];
        } else if (a == "--metrics-channels") {
            need(1);
            metrics_mask = parseMetricMask(argv[++i]);
        } else if (a == "--metrics-counters") {
            print_counters = true;
        } else if (a == "--quiet") {
            cfg.quiet = true;
        } else if (a == "--trace-hash") {
            trace_hash = true;
            cfg.quiet = true; // hashes are noise without -q
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

    if (lat_max.count() <= 0)
        cfg.latency = std::make_shared<FixedLatency>(lat_min);
    else
        cfg.latency = std::make_shared<UniformLatency>(lat_min, lat_max);

    if (use_identities)
        cfg.identity_cache = std::make_shared<IdentityCache>(identity_cache_dir);

    std::shared_ptr<JsonlMetrics> jsonl;
    if (!metrics_file.empty()) {
        jsonl = JsonlMetrics::openFile(metrics_file, metrics_mask);
        cfg.metrics = jsonl;
    } else if (print_counters) {
        cfg.metrics = std::make_shared<NullMetricSink>();
    }

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

    if (jsonl)
        jsonl->flush();

    if (print_counters) {
        auto c = sim.metrics().counters();
        std::fprintf(stderr,
                     "metrics: packets_sent=%llu dropped=%llu bytes_sent=%llu bytes_dropped=%llu "
                     "ops=%llu timers=%llu logs=%llu\n",
                     static_cast<unsigned long long>(c.packets_sent),
                     static_cast<unsigned long long>(c.packets_dropped),
                     static_cast<unsigned long long>(c.bytes_sent),
                     static_cast<unsigned long long>(c.bytes_dropped),
                     static_cast<unsigned long long>(c.ops_recorded),
                     static_cast<unsigned long long>(c.timers_fired),
                     static_cast<unsigned long long>(c.logs_emitted));
    }

    if (trace_hash)
        std::cout << sim.traceHash() << "\n";

    return rc;
}
