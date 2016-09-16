/*
 *  Copyright (C) 2014-2016 Savoir-faire Linux Inc.
 *  Author(s) : Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *              Simon Désaulniers <sim.desaulniers@gmail.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#include "infohash.h" // includes socket structures
#include "utils.h"
#include "sockaddr.h"

#include <list>

namespace dht {

struct Request;

struct Node {
    InfoHash id;
    SockAddr addr;

    time_point time {time_point::min()};            /* last time eared about */
    time_point reply_time {time_point::min()};      /* time of last correct reply received */

    Node(const InfoHash& id, const sockaddr* sa, socklen_t salen)
        : id(id), addr(sa, salen) {}
    Node(const InfoHash& id, const SockAddr& addr) : id(id), addr(addr) {}

    InfoHash getId() const {
        return id;
    }
    std::pair<const sockaddr*, socklen_t> getAddr() const {
        return {(const sockaddr*)&addr.first, addr.second};
    }
    std::string getAddrStr() const {
        return addr.toString();
    }
    bool isExpired() const { return expired_; }
    bool isGood(time_point now) const;
    bool isPendingMessage() const;
    size_t getPendingMessageCount() const;

    NodeExport exportNode() const { return NodeExport {id, addr.first, addr.second}; }
    sa_family_t getFamily() const { return addr.getFamily(); }

    void update(const SockAddr&);

    void requested(std::shared_ptr<Request>& req);
    void received(time_point now, std::shared_ptr<Request> req);

    void setExpired();

    /**
     * Resets the state of the node so it's not expired anymore.
     */
    void reset() { expired_ = false; }

    std::string toString() const;

    friend std::ostream& operator<< (std::ostream& s, const Node& h);

    static constexpr const std::chrono::minutes NODE_GOOD_TIME {120};

    /* The time after which we consider a node to be expirable. */
    static constexpr const std::chrono::minutes NODE_EXPIRE_TIME {10};

    /* Time for a request to timeout */
    static constexpr const std::chrono::seconds MAX_RESPONSE_TIME {1};

private:
    std::list<std::weak_ptr<Request>> requests_ {};
    bool expired_ {false};

    void clearPendingQueue() {
        requests_.remove_if([](std::weak_ptr<Request>& w) {
            return w.expired();
        });
    }
};

}
