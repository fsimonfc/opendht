// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"

#include <string>
#include <string_view>

namespace dht {
namespace sim {

class Simulator;

/**
 * Workload base class. Concrete workloads override `run()` to drive the
 * simulator and `verify()` to assert success.
 *
 * Lifecycle:
 *   1. Simulator constructs nodes and (optionally) bootstraps them.
 *   2. `run(sim)` is called: workloads typically issue puts/gets via
 *      `sim.node(i).runner` and call `sim.runUntil(...)` to drive time.
 *   3. After `run()` returns, `verify(sim, error_out)` is invoked.
 */
class OPENDHT_PUBLIC Workload
{
public:
    virtual ~Workload() = default;
    virtual void run(Simulator& sim) = 0;
    virtual bool verify(Simulator& sim, std::string& error_out) = 0;
    virtual std::string_view name() const = 0;
};

} // namespace sim
} // namespace dht
