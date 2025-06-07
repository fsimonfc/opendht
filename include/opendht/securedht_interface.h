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

#include "dht.h"
#include "crypto.h"

namespace dht {

class OPENDHT_PUBLIC SecureDhtInterface : public DhtInterface {
public:
    SecureDhtInterface() = default;
    SecureDhtInterface(const std::shared_ptr<Logger>& l) : DhtInterface(l) {}
    virtual ~SecureDhtInterface() = default;

    virtual InfoHash getId() const = 0;
    virtual PkId getLongId() const  = 0;
    virtual Sp<crypto::PublicKey> getPublicKey() const = 0;

    virtual void putSigned(const InfoHash& hash, Sp<Value> val, DoneCallback callback, bool permanent = false) = 0;
    virtual void putSigned(const InfoHash& hash, Value&& v, DoneCallback callback, bool permanent = false) = 0;

    virtual void putEncrypted(const InfoHash& hash, const InfoHash& to, Sp<Value> val, DoneCallback callback, bool permanent = false) = 0;
    virtual void putEncrypted(const InfoHash& hash, const InfoHash& to, Value&& v, DoneCallback callback, bool permanent = false) = 0;
    virtual void putEncrypted(const InfoHash& hash, const crypto::PublicKey& to, Sp<Value> val, DoneCallback callback, bool permanent = false) = 0;
    virtual void putEncrypted(const InfoHash& hash, const crypto::PublicKey& to, Value&& v, DoneCallback callback, bool permanent = false) = 0;
    virtual void putEncrypted(const InfoHash& hash, const PkId& to, Sp<Value> val, DoneCallback callback, bool permanent = false) = 0;

    virtual void findCertificate(const InfoHash& node, const std::function<void(const Sp<crypto::Certificate>)>& cb) = 0;
    virtual void findPublicKey(const InfoHash& node, const std::function<void(const Sp<crypto::PublicKey>)>& cb) = 0;

    virtual void findCertificate(const PkId& id, const std::function<void(const Sp<crypto::Certificate>)>& cb) = 0;
    virtual void findPublicKey(const PkId& id, const std::function<void(const Sp<crypto::PublicKey>)>& cb) = 0;

    virtual void registerCertificate(const Sp<crypto::Certificate>& cert) = 0;

    virtual Sp<crypto::Certificate> getCertificate(const InfoHash& node) const = 0;
    virtual Sp<crypto::PublicKey> getPublicKey(const InfoHash& node) const = 0;

    virtual void setLocalCertificateStore(CertificateStoreQuery&& query_method) = 0;
    virtual void forwardAllMessages(bool forward) = 0;
};

}