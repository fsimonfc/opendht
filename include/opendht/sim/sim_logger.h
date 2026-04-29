// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../def.h"
#include "../logger.h"
#include "metric_sink.h"
#include "sim_clock.h"

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>

namespace dht {
namespace sim {

/**
 * Build a `Logger` whose timestamps come from the simulator's `SimClock` (so
 * runs are deterministic) and whose lines are tagged with `[node NNN]`. The
 * format mirrors `printfLog` in src/log.cpp.
 *
 * @param state    shared steady-clock state read for timestamps
 * @param node_id  numeric node id used in the `[node NNN]` prefix
 * @param out      stream to write to (defaults to std::cerr if null)
 * @param sink     optional metric sink; when set, every log line is also
 *                 forwarded via `sink->onLog(...)` (subject to the sink's mask)
 */
std::shared_ptr<Logger> makeSimLogger(std::shared_ptr<SimClock::SteadyState> state,
                                      size_t node_id,
                                      std::shared_ptr<std::ostream> out = {},
                                      std::shared_ptr<MetricSink> sink = {});

} // namespace sim
} // namespace dht
