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
#include "securedht.h"

namespace dht {

namespace net {
    class DatagramSocket;
}

class OPENDHT_PUBLIC RecordedSecureDht final : public SecureDhtInterface {
public:
    RecordedSecureDht(std::unique_ptr<DhtInterface> dht, SecureDhtConfig config, IdentityAnnouncedCb iacb, const std::shared_ptr<Logger>& l)
    {
        dht_ = std::make_unique<SecureDht>(std::move(dht), config, std::move(iacb), l);
    }

    InfoHash getId() const override;
    PkId getLongId() const override;
    Sp<crypto::PublicKey> getPublicKey() const override;

    void putSigned(const InfoHash& hash, Sp<Value> val, DoneCallback callback, bool permanent) override;
    void putSigned(const InfoHash& hash, Value&& v, DoneCallback callback, bool permanent) override;

    void putEncrypted(const InfoHash& hash, const InfoHash& to, Sp<Value> val, DoneCallback callback, bool permanent) override;
    void putEncrypted(const InfoHash& hash, const InfoHash& to, Value&& v, DoneCallback callback, bool permanent) override;
    void putEncrypted(const InfoHash& hash, const crypto::PublicKey& to, Sp<Value> val, DoneCallback callback, bool permanent) override;
    void putEncrypted(const InfoHash& hash, const crypto::PublicKey& to, Value&& v, DoneCallback callback, bool permanent) override;
    void putEncrypted(const InfoHash& hash, const PkId& to, Sp<Value> val, DoneCallback callback, bool permanent = false);

    void findCertificate(const InfoHash& node, const std::function<void(const Sp<crypto::Certificate>)>& cb) override;
    void findPublicKey(const InfoHash& node, const std::function<void(const Sp<crypto::PublicKey>)>& cb) override;

    void findCertificate(const PkId& id, const std::function<void(const Sp<crypto::Certificate>)>& cb) override;
    void findPublicKey(const PkId& id, const std::function<void(const Sp<crypto::PublicKey>)>& cb) override;

    void registerCertificate(const Sp<crypto::Certificate>& cert) override;

    Sp<crypto::Certificate> getCertificate(const InfoHash& node) const override;
    Sp<crypto::PublicKey> getPublicKey(const InfoHash& node) const override;

    void setLocalCertificateStore(CertificateStoreQuery&& query_method) override;
    void forwardAllMessages(bool forward) override;

    NodeStatus updateStatus(sa_family_t af) override;
    NodeStatus getStatus(sa_family_t af) const override;
    NodeStatus getStatus() const override;

    void setOnPublicAddressChanged(PublicAddressChangedCb) override;

    net::DatagramSocket* getSocket() const override;

    const InfoHash& getNodeId() const override;

    void shutdown(ShutdownCallback cb, bool stop) override;
    bool isRunning(sa_family_t af) const override;

    void registerType(const ValueType& type) override;
    const ValueType& getType(ValueType::Id type_id) const override;

    void addBootstrap(const std::string& /*host*/, const std::string& /*service*/) override;
    void clearBootstrap() override;

    void insertNode(const InfoHash& id, const SockAddr&) override;
    void insertNode(const NodeExport& n) override;

    void pingNode(SockAddr, DoneCallbackSimple&& cb) override;

    time_point periodic(const uint8_t *buf, size_t buflen, SockAddr, const time_point& now) override;
    time_point periodic(const uint8_t *buf, size_t buflen, const sockaddr* from, socklen_t fromlen, const time_point& now) override;

    void get(const InfoHash& key, GetCallback cb, DoneCallback donecb, Value::Filter&& f, Where&& w) override;
    void get(const InfoHash& key, GetCallback cb, DoneCallbackSimple donecb, Value::Filter&& f, Where&& w) override;
    void get(const InfoHash& key, GetCallbackSimple cb, DoneCallback donecb, Value::Filter&& f, Where&& w) override;
    void get(const InfoHash& key, GetCallbackSimple cb, DoneCallbackSimple donecb, Value::Filter&& f, Where&& w) override;

    void query(const InfoHash& key, QueryCallback cb, DoneCallback done_cb, Query&& q) override;
    void query(const InfoHash& key, QueryCallback cb, DoneCallbackSimple done_cb, Query&& q) override;

    std::vector<Sp<Value>> getLocal(const InfoHash& key, const Value::Filter& f) const override;
    Sp<Value> getLocalById(const InfoHash& key, Value::Id vid) const override;

    void put(const InfoHash& key, Sp<Value>, DoneCallback cb, time_point created, bool permanent) override;
    void put(const InfoHash& key, const Sp<Value>& v, DoneCallbackSimple cb, time_point created, bool permanent) override;
    void put(const InfoHash& key, Value&& v, DoneCallback cb, time_point created, bool permanent) override;
    void put(const InfoHash& key, Value&& v, DoneCallbackSimple cb, time_point created, bool permanent) override;

    std::vector<Sp<Value>> getPut(const InfoHash&) const override;
    Sp<Value> getPut(const InfoHash&, const Value::Id&) const override;
    bool cancelPut(const InfoHash&, const Value::Id&) override;

    size_t listen(const InfoHash&, GetCallback, Value::Filter, Where w) override;
    size_t listen(const InfoHash& key, GetCallbackSimple cb, Value::Filter f, Where w) override;
    size_t listen(const InfoHash&, ValueCallback, Value::Filter, Where w) override;
    bool cancelListen(const InfoHash&, size_t token) override;

    void connectivityChanged(sa_family_t) override;
    void connectivityChanged() override;

    std::vector<NodeExport> exportNodes() const override;
    std::vector<ValuesExport> exportValues() const override;
    void importValues(const std::vector<ValuesExport>&) override;
    NodeStats getNodesStats(sa_family_t af) const override;
    std::string getStorageLog() const override;
    std::string getStorageLog(const InfoHash&) const override;
    std::string getRoutingTablesLog(sa_family_t) const override;
    std::string getSearchesLog(sa_family_t) const override;
    std::string getSearchLog(const InfoHash&, sa_family_t af) const override;
    void dumpTables() const override;
    std::vector<unsigned> getNodeMessageStats(bool in) override;

    void setStorageLimit(size_t limit) override;
    size_t getStorageLimit() const override;

    std::pair<size_t, size_t> getStoreSize() const override;

    std::vector<SockAddr> getPublicAddress(sa_family_t family) override;

    void setLogger(const Logger& l) override;
    void setLogger(const std::shared_ptr<Logger>& l) override;
    void setLogFilter(const InfoHash& f) override;

    void setPushNotificationToken(const std::string&) override;
    void setPushNotificationTopic(const std::string&) override;
    void setPushNotificationPlatform(const std::string&) override;
    PushNotificationResult pushNotificationReceived(const std::map<std::string, std::string>& data) override;

private:
    std::unique_ptr<SecureDht> dht_;
};

} // namespace dht
