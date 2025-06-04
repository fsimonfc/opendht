/*
 *  Copyright (C) 2025 Savoir-faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "utils.h"
#include "network_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace dht {

struct RecordedTime {
    std::vector<clock::rep> now;
    std::vector<clock::rep> from_time_t;
    std::vector<std::time_t> to_time_t;

    void save(const std::filesystem::path& filepath) const;
    void load(const std::filesystem::path filepath);
};

struct RecordedPacket {
    const uint8_t *data;
    size_t dataSize;
    SockAddr from;
    time_point received;
};

struct RecordedData {
    uint64_t rngSeed;

    uint8_t *packetData;
    uint32_t packetMaxSize;
    uint32_t packetIndex;

    RecordedData();
    RecordedData(uint32_t maxSize);
    ~RecordedData();
    void addPacket(const net::ReceivedPacket& pkt);
    RecordedPacket getPacket();
    void save(const std::filesystem::path& filepath) const;
    void load(const std::filesystem::path& filepath);
};

struct RecordedProxyData {
    uint64_t rngSeed;

    void save(const std::filesystem::path& filepath) const;
    void load(const std::filesystem::path& filepath);
};

}