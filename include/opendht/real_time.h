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
#include "recording.h"

#include <chrono>

namespace dht {

class Time : public AbstractTime {
public:
    time_point steadyNow()
    {
        auto now = clock::now();
        recorded_.now.push_back(now.time_since_epoch().count());
        return now;
    }

    time_point from_time_t(std::time_t t)
    {
        auto dt = system_clock::from_time_t(t) - system_clock::now();
        auto now = clock::now();
        if (dt > system_clock::duration(0) and now > time_point::max() - dt)
            return time_point::max();
        else if (dt < system_clock::duration(0) and now < time_point::min() - dt)
            return time_point::min();
        
        auto ret = now + dt;
        recorded_.from_time_t.push_back(ret.time_since_epoch().count());
        return ret;
    }

    std::time_t to_time_t(time_point t)
    {
        auto dt = t - clock::now();
        auto now = system_clock::now();

        std::time_t ret;
        if (dt > duration(0) and now >= system_clock::time_point::max() - dt)
            ret = system_clock::to_time_t(system_clock::time_point::max());
        else if (dt < duration(0) and now <= system_clock::time_point::min() - dt)
            ret = system_clock::to_time_t(system_clock::time_point::min());
        else
            ret = system_clock::to_time_t(now + std::chrono::duration_cast<system_clock::duration>(dt));
        
        recorded_.to_time_t.push_back(ret);
        return ret;
    }

    RecordedTime recorded_;
};

}