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

InfoHash
RecordedSecureDht::getId() const
{
    return dht_->getId();
}

PkId
RecordedSecureDht::getLongId() const
{
    return dht_->getLongId();
}

Sp<crypto::PublicKey>
RecordedSecureDht::getPublicKey() const
{
    return dht_->getPublicKey();
}

void
RecordedSecureDht::putSigned(const InfoHash& hash, Sp<Value> val, DoneCallback callback, bool permanent)
{
    return dht_->putSigned(hash, val, callback, permanent);
}

void
RecordedSecureDht::putSigned(const InfoHash& hash, Value&& v, DoneCallback callback, bool permanent)
{
    return dht_->putSigned(hash, std::move(v), callback, permanent);
}

void
RecordedSecureDht::putEncrypted(const InfoHash& hash, const InfoHash& to, Sp<Value> val, DoneCallback callback, bool permanent)
{
    return dht_->putEncrypted(hash, to, val, callback, permanent);
}

void
RecordedSecureDht::putEncrypted(const InfoHash& hash, const InfoHash& to, Value&& v, DoneCallback callback, bool permanent)
{
    return dht_->putEncrypted(hash, to, std::move(v), callback, permanent);
}

void
RecordedSecureDht::putEncrypted(const InfoHash& hash, const crypto::PublicKey& to, Sp<Value> val, DoneCallback callback, bool permanent)
{
    return dht_->putEncrypted(hash, to, val, callback, permanent);
}

void
RecordedSecureDht::putEncrypted(const InfoHash& hash, const crypto::PublicKey& to, Value&& v, DoneCallback callback, bool permanent)
{
    return dht_->putEncrypted(hash, to, std::move(v), callback, permanent);
}

void
RecordedSecureDht::putEncrypted(const InfoHash& hash, const PkId& to, Sp<Value> val, DoneCallback callback, bool permanent)
{
    return dht_->putEncrypted(hash, to, val, callback, permanent);
}

void
RecordedSecureDht::findCertificate(const InfoHash& node, const std::function<void(const Sp<crypto::Certificate>)>& cb)
{
    return dht_->findCertificate(node, cb);
}
void
RecordedSecureDht::findPublicKey(const InfoHash& node, const std::function<void(const Sp<crypto::PublicKey>)>& cb)
{
    return dht_->findPublicKey(node, cb);
}

void
RecordedSecureDht::findCertificate(const PkId& id, const std::function<void(const Sp<crypto::Certificate>)>& cb)
{
    return dht_->findCertificate(id, cb);
}

void
RecordedSecureDht::findPublicKey(const PkId& id, const std::function<void(const Sp<crypto::PublicKey>)>& cb)
{
    return dht_->findPublicKey(id, cb);
}

void
RecordedSecureDht::registerCertificate(const Sp<crypto::Certificate>& cert)
{
    return dht_->registerCertificate(cert);
}

Sp<crypto::Certificate>
RecordedSecureDht::getCertificate(const InfoHash& node) const
{
    return dht_->getCertificate(node);
}

Sp<crypto::PublicKey>
RecordedSecureDht::getPublicKey(const InfoHash& node) const
{
    return dht_->getPublicKey(node);
}

void
RecordedSecureDht::setLocalCertificateStore(CertificateStoreQuery&& query_method)
{
    return dht_->setLocalCertificateStore(std::move(query_method));
}

void
RecordedSecureDht::forwardAllMessages(bool forward)
{
    return dht_->forwardAllMessages(forward);
}

NodeStatus
RecordedSecureDht::updateStatus(sa_family_t af)
{
    return dht_->updateStatus(af);
}

NodeStatus
RecordedSecureDht::getStatus(sa_family_t af) const
{
    return dht_->getStatus(af);
}

NodeStatus
RecordedSecureDht::getStatus() const
{
    return dht_->getStatus();
}

void
RecordedSecureDht::setOnPublicAddressChanged(PublicAddressChangedCb cb)
{
    return dht_->setOnPublicAddressChanged(cb);
}

net::DatagramSocket*
RecordedSecureDht::getSocket() const
{
    return dht_->getSocket();
}

const InfoHash&
RecordedSecureDht::getNodeId() const
{
    return dht_->getNodeId();
}

void
RecordedSecureDht::shutdown(ShutdownCallback cb, bool stop)
{
    return dht_->shutdown(cb, stop);
}

bool
RecordedSecureDht::isRunning(sa_family_t af) const
{
    return dht_->isRunning(af);
}

void
RecordedSecureDht::registerType(const ValueType& type)
{
    return dht_->registerType(type);
}

const ValueType&
RecordedSecureDht::getType(ValueType::Id type_id) const
{
    return dht_->getType(type_id);
}

void
RecordedSecureDht::addBootstrap(const std::string& host, const std::string& service)
{
    return dht_->addBootstrap(host, service);
}

void
RecordedSecureDht::clearBootstrap()
{
    return dht_->clearBootstrap();
}

void
RecordedSecureDht::insertNode(const InfoHash& id, const SockAddr& addr)
{
    return dht_->insertNode(id, addr);
}

void
RecordedSecureDht::insertNode(const NodeExport& n)
{
    return dht_->insertNode(n);
}

void
RecordedSecureDht::pingNode(SockAddr addr, DoneCallbackSimple&& cb)
{
    return dht_->pingNode(addr, std::move(cb));
}

time_point
RecordedSecureDht::periodic(const uint8_t *buf, size_t buflen, SockAddr addr, const time_point& now)
{
    return dht_->periodic(buf, buflen, addr, now);
}

time_point
RecordedSecureDht::periodic(const uint8_t *buf, size_t buflen, const sockaddr* from, socklen_t fromlen, const time_point& now)
{
    return dht_->periodic(buf, buflen, from, fromlen, now);
}

void
RecordedSecureDht::get(const InfoHash& key, GetCallback cb, DoneCallback donecb, Value::Filter&& f, Where&& w)
{
    return dht_->get(key, cb, donecb, std::move(f), std::move(w));
}

void
RecordedSecureDht::get(const InfoHash& key, GetCallback cb, DoneCallbackSimple donecb, Value::Filter&& f, Where&& w)
{
    return dht_->get(key, cb, donecb, std::move(f), std::move(w));
}

void
RecordedSecureDht::get(const InfoHash& key, GetCallbackSimple cb, DoneCallback donecb, Value::Filter&& f, Where&& w)
{
    return dht_->get(key, cb, donecb, std::move(f), std::move(w));
}

void
RecordedSecureDht::get(const InfoHash& key, GetCallbackSimple cb, DoneCallbackSimple donecb, Value::Filter&& f, Where&& w)
{
    return dht_->get(key, cb, donecb, std::move(f), std::move(w));
}

void
RecordedSecureDht::query(const InfoHash& key, QueryCallback cb, DoneCallback done_cb, Query&& q)
{
    return dht_->query(key, cb, done_cb, std::move(q));
}

void
RecordedSecureDht::query(const InfoHash& key, QueryCallback cb, DoneCallbackSimple done_cb, Query&& q)
{
    return dht_->query(key, cb, done_cb, std::move(q));
}

std::vector<Sp<Value>>
RecordedSecureDht::getLocal(const InfoHash& key, const Value::Filter& f) const
{
    return dht_->getLocal(key, f);
}

Sp<Value>
RecordedSecureDht::getLocalById(const InfoHash& key, Value::Id vid) const
{
    return dht_->getLocalById(key, vid);
}

void
RecordedSecureDht::put(const InfoHash& key, Sp<Value> v, DoneCallback cb, time_point created, bool permanent)
{
    return dht_->put(key, v, cb, created, permanent);
}

void
RecordedSecureDht::put(const InfoHash& key, const Sp<Value>& v, DoneCallbackSimple cb, time_point created, bool permanent)
{
    return dht_->put(key, v, cb, created, permanent);
}

void
RecordedSecureDht::put(const InfoHash& key, Value&& v, DoneCallback cb, time_point created, bool permanent)
{
    return dht_->put(key, std::move(v), cb, created, permanent);
}

void
RecordedSecureDht::put(const InfoHash& key, Value&& v, DoneCallbackSimple cb, time_point created, bool permanent)
{
    return dht_->put(key, std::move(v), cb, created, permanent);
}

std::vector<Sp<Value>>
RecordedSecureDht::getPut(const InfoHash& key) const
{
    return dht_->getPut(key);
}

Sp<Value>
RecordedSecureDht::getPut(const InfoHash& key, const Value::Id& id) const
{
    return dht_->getPut(key, id);
}

bool
RecordedSecureDht::cancelPut(const InfoHash& key, const Value::Id& id)
{
    return dht_->cancelPut(key, id);
}

size_t
RecordedSecureDht::listen(const InfoHash& key, GetCallback cb, Value::Filter f, Where w)
{
    return dht_->listen(key, cb, f, w);
}

size_t
RecordedSecureDht::listen(const InfoHash& key, GetCallbackSimple cb, Value::Filter f, Where w)
{
    return dht_->listen(key, cb, f, w);
}

size_t
RecordedSecureDht::listen(const InfoHash& key, ValueCallback cb, Value::Filter f, Where w)
{
    return dht_->listen(key, cb, f, w);
}

bool
RecordedSecureDht::cancelListen(const InfoHash& key, size_t token)
{
    return dht_->cancelListen(key, token);
}

void
RecordedSecureDht::connectivityChanged(sa_family_t af)
{
    return dht_->connectivityChanged(af);
}

void
RecordedSecureDht::connectivityChanged()
{
    return dht_->connectivityChanged();
}

std::vector<NodeExport>
RecordedSecureDht::exportNodes() const
{
    return dht_->exportNodes();
}

std::vector<ValuesExport>
RecordedSecureDht::exportValues() const
{
    return dht_->exportValues();
}

void
RecordedSecureDht::importValues(const std::vector<ValuesExport>& import)
{
    return dht_->importValues(import);
}

NodeStats
RecordedSecureDht::getNodesStats(sa_family_t af) const
{
    return dht_->getNodesStats(af);
}

std::string
RecordedSecureDht::getStorageLog() const
{
    return dht_->getStorageLog();
}

std::string
RecordedSecureDht::getStorageLog(const InfoHash& h) const
{
    return dht_->getStorageLog(h);
}

std::string
RecordedSecureDht::getRoutingTablesLog(sa_family_t af) const
{
    return dht_->getRoutingTablesLog(af);
}

std::string
RecordedSecureDht::getSearchesLog(sa_family_t af) const
{
    return dht_->getSearchesLog(af);
}

std::string
RecordedSecureDht::getSearchLog(const InfoHash& h, sa_family_t af) const
{
    return dht_->getSearchLog(h, af);
}

void
RecordedSecureDht::dumpTables() const
{
    return dht_->dumpTables();
}

std::vector<unsigned>
RecordedSecureDht::getNodeMessageStats(bool in)
{
    return dht_->getNodeMessageStats(in);
}

void
RecordedSecureDht::setStorageLimit(size_t limit)
{
    return dht_->setStorageLimit(limit);
}

size_t
RecordedSecureDht::getStorageLimit() const
{
    return dht_->getStorageLimit();
}

std::pair<size_t, size_t>
RecordedSecureDht::getStoreSize() const
{
    return dht_->getStoreSize();
}

std::vector<SockAddr>
RecordedSecureDht::getPublicAddress(sa_family_t family)
{
    return dht_->getPublicAddress(family);
}

void
RecordedSecureDht::setLogger(const Logger& l)
{
    return dht_->setLogger(l);
}

void
RecordedSecureDht::setLogger(const std::shared_ptr<Logger>& l)
{
    return dht_->setLogger(l);
}

void
RecordedSecureDht::setLogFilter(const InfoHash& f)
{
    return dht_->setLogFilter(f);
}

void
RecordedSecureDht::setPushNotificationToken(const std::string& token)
{
    return dht_->setPushNotificationToken(token);
}

void
RecordedSecureDht::setPushNotificationTopic(const std::string& topic)
{
    return dht_->setPushNotificationTopic(topic);
}

void
RecordedSecureDht::setPushNotificationPlatform(const std::string& platform)
{
    return dht_->setPushNotificationPlatform(platform);
}

PushNotificationResult
RecordedSecureDht::pushNotificationReceived(const std::map<std::string, std::string>& data)
{
    return dht_->pushNotificationReceived(data);
}

}