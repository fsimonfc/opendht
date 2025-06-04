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

#include "recording.h"

namespace dht {

void
RecordedTime::save(const std::filesystem::path& filepath) const
{
    std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
    
    auto size = now.size();
    ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
    ofs.write(reinterpret_cast<const char*>(now.data()), size * sizeof(clock::rep));
    fmt::print(stderr, "Saved {} 'now' entries\n", size);

    size = from_time_t.size();
    ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
    ofs.write(reinterpret_cast<const char*>(from_time_t.data()), size * sizeof(clock::rep));
    fmt::print(stderr, "Saved {} 'from_time_t' entries\n", size);

    size = to_time_t.size();
    ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
    ofs.write(reinterpret_cast<const char*>(to_time_t.data()), size * sizeof(std::time_t));
    fmt::print(stderr, "Saved {} 'to_time_t' entries\n", size);
}

void
RecordedTime::load(const std::filesystem::path filepath)
{
    std::ifstream ifs(filepath, std::ios::in | std::ios::binary);

    clock::rep size;
    ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
    now.resize(size);
    ifs.read(reinterpret_cast<char*>(now.data()), size * sizeof(clock::rep));

    ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
    from_time_t.resize(size);
    ifs.read(reinterpret_cast<char*>(from_time_t.data()), size * sizeof(clock::rep));

    ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
    to_time_t.resize(size);
    ifs.read(reinterpret_cast<char*>(to_time_t.data()), size * sizeof(std::time_t));
}

RecordedData::RecordedData()
    : packetData(nullptr)
    , packetMaxSize(0)
    , packetIndex(0)
{}

RecordedData::RecordedData(uint32_t maxSize)
    : packetMaxSize(maxSize)
    , packetIndex(0)
{
    packetData = new uint8_t[packetMaxSize];
}

RecordedData::~RecordedData() {
    delete[] packetData;
}

void
RecordedData::addPacket(const net::ReceivedPacket& pkt)
{
    //static uint64_t packet_count;

    clock::rep received = pkt.received.time_since_epoch().count();
    socklen_t addrLen = pkt.from.getLength();
    const sockaddr *addr = pkt.from.get();
    size_t dataSize = pkt.data.size();
    const uint8_t *data = pkt.data.data();

    size_t totalSize = sizeof(received) + sizeof(addrLen) + addrLen + sizeof(dataSize) + dataSize;
    if (packetIndex + totalSize > packetMaxSize) {
        return;
    }

    std::memcpy(packetData + packetIndex, &received, sizeof(received));
    packetIndex += sizeof(received);

    std::memcpy(packetData + packetIndex, &addrLen, sizeof(addrLen));
    packetIndex += sizeof(addrLen);

    std::memcpy(packetData + packetIndex, addr, addrLen);
    packetIndex += addrLen;

    std::memcpy(packetData + packetIndex, &dataSize, sizeof(dataSize));
    packetIndex += sizeof(dataSize);

    std::memcpy(packetData + packetIndex, data, dataSize);
    packetIndex += dataSize;

    //fmt::print(stderr, "@packet {}: received={} addrLen={} dataSize={}\n", 
    //          packet_count++, received, addrLen, dataSize);
}

RecordedPacket
RecordedData::getPacket()
{
    //static uint64_t packet_count;

    clock::rep received;
    std::memcpy(&received, packetData + packetIndex, sizeof(received));
    packetIndex += sizeof(received);

    socklen_t addrLen;
    std::memcpy(&addrLen, packetData + packetIndex, sizeof(addrLen));
    packetIndex += sizeof(addrLen);

    SockAddr from(reinterpret_cast<const sockaddr*>(packetData + packetIndex), addrLen);
    packetIndex += addrLen;

    size_t dataSize;
    std::memcpy(&dataSize, packetData + packetIndex, sizeof(dataSize));
    packetIndex += sizeof(dataSize);

    const uint8_t *data = packetData + packetIndex;
    packetIndex += dataSize;

    //fmt::print(stderr, "@packet {}: received={} addrLen={} dataSize={}\n", 
    //          packet_count++, received, addrLen, dataSize);

    return {data, dataSize, from, clock::time_point{clock::duration{received}}};
}

void
RecordedData::save(const std::filesystem::path& filepath) const
{
    std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&rngSeed), sizeof(rngSeed));
    ofs.write(reinterpret_cast<const char*>(&packetIndex), sizeof(packetIndex));
    ofs.write(reinterpret_cast<const char*>(packetData), packetIndex);
    fmt::print(stderr, "Saved recorded data with RNG seed: {} packetData size: {}\n",
              rngSeed, packetIndex);
}

void
RecordedData::load(const std::filesystem::path& filepath)
{
    std::ifstream ifs(filepath, std::ios::in | std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&rngSeed), sizeof(rngSeed));
    if (!ifs) {
        throw std::runtime_error("Failed to load recorded data from " + filepath.string());
    }

    ifs.read(reinterpret_cast<char*>(&packetIndex), sizeof(packetIndex));
    packetMaxSize = packetIndex;
    packetIndex = 0;
    packetData = new uint8_t[packetMaxSize];
    ifs.read(reinterpret_cast<char*>(packetData), packetMaxSize);
    if (!ifs) {
        delete[] packetData;
        throw std::runtime_error("Failed to read packet data from " + filepath.string());
    }

    fmt::print(stderr, "Loaded recorded data with RNG seed: {} packetData size: {}\n",
               rngSeed, packetMaxSize);
}

void
RecordedProxyData::save(const std::filesystem::path& filepath) const
{
    std::ofstream ofs(filepath, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&rngSeed), sizeof(rngSeed));
    fmt::print(stderr, "Saved recorded data with RNG seed: {}\n", rngSeed);
}

void
RecordedProxyData::load(const std::filesystem::path& filepath)
{
    std::ifstream ifs(filepath, std::ios::in | std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&rngSeed), sizeof(rngSeed));
    if (!ifs) {
        throw std::runtime_error("Failed to load recorded data from " + filepath.string());
    }
    fmt::print(stderr, "Loaded recorded data with RNG seed: {}\n", rngSeed);
}

}