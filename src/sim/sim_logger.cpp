// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/sim_logger.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <chrono>
#include <iostream>

namespace dht {
namespace sim {

namespace {
using log_precision = std::chrono::microseconds;
constexpr auto den = log_precision::period::den;

constexpr std::string_view color_red = "\033[31m";
constexpr std::string_view color_yellow = "\033[33m";
constexpr std::string_view color_reset = "\033[39m";
} // namespace

std::shared_ptr<Logger>
makeSimLogger(std::shared_ptr<SimClock::SteadyState> state, size_t node_id, std::shared_ptr<std::ostream> out)
{
    if (!out) {
        // Wrap std::cerr in a non-owning shared_ptr.
        out = std::shared_ptr<std::ostream>(&std::cerr, [](std::ostream*) {});
    }
    auto node_tag = fmt::format("[node {:03d}] ", node_id);
    return std::make_shared<Logger>([state, out, node_tag = std::move(node_tag)](log::source_loc loc,
                                                                                 log::LogLevel level,
                                                                                 std::string_view prefix,
                                                                                 std::string&& message) {
        auto num = std::chrono::duration_cast<log_precision>(state->now.time_since_epoch()).count();
        if (level == log::LogLevel::error)
            *out << color_red;
        else if (level == log::LogLevel::warning)
            *out << color_yellow;
        if (!loc.file.empty())
            fmt::print(*out,
                       "{}[{:06d}.{:06d} {:>20}:{:<5} {:<24}] {}",
                       node_tag,
                       num / den,
                       num % den,
                       loc.file,
                       loc.line,
                       loc.function,
                       prefix);
        else
            fmt::print(*out, "{}[{:06d}.{:06d}] {}", node_tag, num / den, num % den, prefix);
        *out << message << color_reset << std::endl;
    });
}

} // namespace sim
} // namespace dht
