// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/metric_sink.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <fstream>
#include <ostream>
#include <stdexcept>

namespace dht {
namespace sim {

namespace {

inline int64_t
nsOf(MetricSink::time_point t) noexcept
{
    return t.time_since_epoch().count();
}

const char*
levelName(log::LogLevel l) noexcept
{
    switch (l) {
    case log::LogLevel::debug:
        return "debug";
    case log::LogLevel::warning:
        return "warning";
    case log::LogLevel::error:
        return "error";
    }
    return "unknown";
}

/** Minimal JSON string escape (RFC 8259 §7) for control chars and quotes. */
std::string
jsonEscape(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
        case '"':
            out.append("\\\"");
            break;
        case '\\':
            out.append("\\\\");
            break;
        case '\b':
            out.append("\\b");
            break;
        case '\f':
            out.append("\\f");
            break;
        case '\n':
            out.append("\\n");
            break;
        case '\r':
            out.append("\\r");
            break;
        case '\t':
            out.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
                out.append(fmt::format("\\u{:04x}", static_cast<unsigned>(static_cast<unsigned char>(c))));
            else
                out.push_back(c);
        }
    }
    return out;
}

} // namespace

std::string_view
opKindName(OpKind k) noexcept
{
    switch (k) {
    case OpKind::Put:
        return "put";
    case OpKind::Get:
        return "get";
    case OpKind::Listen:
        return "listen";
    case OpKind::Bootstrap:
        return "bootstrap";
    case OpKind::Cancel:
        return "cancel";
    case OpKind::Other:
        return "other";
    }
    return "other";
}

MetricMask
parseMetricMask(std::string_view csv)
{
    MetricMask m = MetricMask::none();
    size_t pos = 0;
    while (pos < csv.size()) {
        auto comma = csv.find(',', pos);
        auto tok = csv.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
        // trim surrounding spaces
        while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t'))
            tok.remove_prefix(1);
        while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t'))
            tok.remove_suffix(1);
        if (tok == "packets")
            m.packets = true;
        else if (tok == "ops")
            m.ops = true;
        else if (tok == "logs")
            m.logs = true;
        else if (tok == "timers")
            m.timers = true;
        else if (tok == "all")
            m = MetricMask::all();
        else if (tok == "none")
            m = MetricMask::none();
        // unknown tokens are silently ignored
        if (comma == std::string_view::npos)
            break;
        pos = comma + 1;
    }
    return m;
}

// ---- InMemoryMetrics --------------------------------------------------------

void
InMemoryMetrics::onPacket(time_point t, std::size_t src, std::size_t dst, std::size_t size, bool dropped)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (dropped) {
        ++c_.packets_dropped;
        c_.bytes_dropped += size;
    } else {
        ++c_.packets_sent;
        c_.bytes_sent += size;
    }
    if (mask_.packets)
        packets_.push_back({t, src, dst, size, dropped});
}

void
InMemoryMetrics::onOp(time_point t, std::size_t node, OpKind kind, std::string_view key, std::string_view status)
{
    std::lock_guard<std::mutex> lk(mu_);
    ++c_.ops_recorded;
    if (mask_.ops)
        ops_.push_back({t, node, kind, std::string(key), std::string(status)});
}

void
InMemoryMetrics::onLog(time_point t, std::size_t node, log::LogLevel level, std::string_view message)
{
    std::lock_guard<std::mutex> lk(mu_);
    ++c_.logs_emitted;
    if (mask_.logs)
        logs_.push_back({t, node, level, std::string(message)});
}

void
InMemoryMetrics::onTimer(time_point t, std::size_t node, time_point scheduled_at)
{
    std::lock_guard<std::mutex> lk(mu_);
    ++c_.timers_fired;
    if (mask_.timers)
        timers_.push_back({t, node, scheduled_at});
}

MetricCounters
InMemoryMetrics::counters() const noexcept
{
    std::lock_guard<std::mutex> lk(mu_);
    return c_;
}

// ---- JsonlMetrics -----------------------------------------------------------

JsonlMetrics::JsonlMetrics(std::ostream& out, MetricMask mask)
    : out_(out)
    , mask_(mask)
{}

std::shared_ptr<JsonlMetrics>
JsonlMetrics::openFile(const std::string& path, MetricMask mask)
{
    auto ofs = std::make_shared<std::ofstream>(path, std::ios::out | std::ios::trunc);
    if (!ofs->is_open())
        throw std::runtime_error("JsonlMetrics: cannot open " + path);
    auto sink = std::shared_ptr<JsonlMetrics>(new JsonlMetrics(*ofs, mask));
    sink->out_owner_ = ofs;
    return sink;
}

void
JsonlMetrics::onPacket(time_point t, std::size_t src, std::size_t dst, std::size_t size, bool dropped)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (dropped) {
        ++c_.packets_dropped;
        c_.bytes_dropped += size;
    } else {
        ++c_.packets_sent;
        c_.bytes_sent += size;
    }
    if (!mask_.packets)
        return;
    fmt::print(out_,
               "{{\"t\":{},\"kind\":\"packet\",\"src\":{},\"dst\":{},\"size\":{},\"dropped\":{}}}\n",
               nsOf(t),
               src,
               dst,
               size,
               dropped ? "true" : "false");
}

void
JsonlMetrics::onOp(time_point t, std::size_t node, OpKind kind, std::string_view key, std::string_view status)
{
    std::lock_guard<std::mutex> lk(mu_);
    ++c_.ops_recorded;
    if (!mask_.ops)
        return;
    fmt::print(out_,
               "{{\"t\":{},\"kind\":\"op\",\"node\":{},\"op\":\"{}\",\"key\":\"{}\",\"status\":\"{}\"}}\n",
               nsOf(t),
               node,
               opKindName(kind),
               jsonEscape(key),
               jsonEscape(status));
}

void
JsonlMetrics::onLog(time_point t, std::size_t node, log::LogLevel level, std::string_view message)
{
    std::lock_guard<std::mutex> lk(mu_);
    ++c_.logs_emitted;
    if (!mask_.logs)
        return;
    fmt::print(out_,
               "{{\"t\":{},\"kind\":\"log\",\"node\":{},\"level\":\"{}\",\"msg\":\"{}\"}}\n",
               nsOf(t),
               node,
               levelName(level),
               jsonEscape(message));
}

void
JsonlMetrics::onTimer(time_point t, std::size_t node, time_point scheduled_at)
{
    std::lock_guard<std::mutex> lk(mu_);
    ++c_.timers_fired;
    if (!mask_.timers)
        return;
    fmt::print(out_,
               "{{\"t\":{},\"kind\":\"timer\",\"node\":{},\"scheduled_at\":{}}}\n",
               nsOf(t),
               node,
               nsOf(scheduled_at));
}

MetricCounters
JsonlMetrics::counters() const noexcept
{
    // mu_ protects c_; const_cast because `counters()` is conceptually const.
    std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(mu_));
    return c_;
}

void
JsonlMetrics::flush()
{
    std::lock_guard<std::mutex> lk(mu_);
    out_.flush();
}

} // namespace sim
} // namespace dht
