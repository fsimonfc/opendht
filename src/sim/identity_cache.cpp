// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "opendht/sim/identity_cache.h"

#include <fmt/format.h>
#include <gnutls/abstract.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace dht {
namespace sim {

namespace fs = std::filesystem;

namespace {

constexpr unsigned RSA_BITS = 3072;

// Fixed cert validity window so cert bytes are stable across runs.
//   activation = 1700000000 (anchor of SimClock simulated epoch, see sim_clock.h)
//   expiration = activation + 100 years
constexpr time_t SIM_CERT_ACTIVATION = static_cast<time_t>(1'700'000'000);
constexpr time_t SIM_CERT_EXPIRATION = SIM_CERT_ACTIVATION + static_cast<time_t>(100ll * 365 * 24 * 3600);

/** SHA-256(identity_seed_le || index_le) -> 32-byte seed for keygen. */
std::array<unsigned char, 32>
deriveSeed(uint64_t identity_seed, size_t index)
{
    std::array<unsigned char, 16> in {};
    for (int i = 0; i < 8; ++i)
        in[i] = static_cast<unsigned char>((identity_seed >> (8 * i)) & 0xff);
    auto idx64 = static_cast<uint64_t>(index);
    for (int i = 0; i < 8; ++i)
        in[8 + i] = static_cast<unsigned char>((idx64 >> (8 * i)) & 0xff);
    auto h = crypto::hash(Blob {in.begin(), in.end()}, 32);
    std::array<unsigned char, 32> out {};
    std::memcpy(out.data(), h.data(), 32);
    return out;
}

struct GnuTlsKeyHolder
{
    gnutls_x509_privkey_t k {nullptr};
    GnuTlsKeyHolder() { if (gnutls_x509_privkey_init(&k)) k = nullptr; }
    ~GnuTlsKeyHolder() { if (k) gnutls_x509_privkey_deinit(k); }
    gnutls_x509_privkey_t release() { auto* r = k; k = nullptr; return r; }
};

struct GnuTlsCertHolder
{
    gnutls_x509_crt_t c {nullptr};
    GnuTlsCertHolder() { if (gnutls_x509_crt_init(&c)) c = nullptr; }
    ~GnuTlsCertHolder() { if (c) gnutls_x509_crt_deinit(c); }
    gnutls_x509_crt_t release() { auto* r = c; c = nullptr; return r; }
};

/**
 * Build an Identity deterministically from `(identity_seed, index)`.
 *
 * Determinism comes from:
 *  - GnuTLS RSA-3072 keygen with `GNUTLS_PRIVKEY_FLAG_PROVABLE` and a 32-byte
 *    `GNUTLS_KEYGEN_SEED` derived via SHA-256(identity_seed || index). For a
 *    fixed GnuTLS/nettle pair this is byte-identical across runs.
 *  - Cert activation/expiration are fixed compile-time constants anchored at
 *    the SimClock simulated epoch.
 *  - Cert serial = identity_seed XOR index, 8 bytes big-endian.
 *  - Subject DN derived from the seed/index pair, plus subject_key_id from
 *    the public key.
 *  - RSA PKCS#1 v1.5 SHA-256 signature, which is deterministic by definition.
 */
crypto::Identity
generateDeterministic(uint64_t identity_seed, size_t index)
{
    auto seed = deriveSeed(identity_seed, index);

    GnuTlsKeyHolder kh;
    if (!kh.k)
        throw crypto::CryptoException("gnutls_x509_privkey_init failed");
    gnutls_keygen_data_st kd {GNUTLS_KEYGEN_SEED, seed.data(), static_cast<unsigned>(seed.size())};
    if (auto err = gnutls_x509_privkey_generate2(kh.k, GNUTLS_PK_RSA, RSA_BITS, GNUTLS_PRIVKEY_FLAG_PROVABLE, &kd, 1))
        throw crypto::CryptoException(std::string {"deterministic key generation failed: "} + gnutls_strerror(err));

    auto pk_handle = kh.release();
    auto pkey = std::make_shared<crypto::PrivateKey>(pk_handle);

    GnuTlsCertHolder ch;
    if (!ch.c)
        throw crypto::CryptoException("gnutls_x509_crt_init failed");

    if (auto err = gnutls_x509_crt_set_version(ch.c, 3))
        throw crypto::CryptoException(std::string {"set_version: "} + gnutls_strerror(err));
    if (auto err = gnutls_x509_crt_set_key(ch.c, pk_handle))
        throw crypto::CryptoException(std::string {"set_key: "} + gnutls_strerror(err));
    if (auto err = gnutls_x509_crt_set_activation_time(ch.c, SIM_CERT_ACTIVATION))
        throw crypto::CryptoException(std::string {"set_activation: "} + gnutls_strerror(err));
    if (auto err = gnutls_x509_crt_set_expiration_time(ch.c, SIM_CERT_EXPIRATION))
        throw crypto::CryptoException(std::string {"set_expiration: "} + gnutls_strerror(err));

    std::array<unsigned char, 8> serial {};
    auto sv = identity_seed ^ static_cast<uint64_t>(index);
    for (int i = 0; i < 8; ++i)
        serial[i] = static_cast<unsigned char>((sv >> (8 * (7 - i))) & 0xff);
    if (auto err = gnutls_x509_crt_set_serial(ch.c, serial.data(), serial.size()))
        throw crypto::CryptoException(std::string {"set_serial: "} + gnutls_strerror(err));

    auto pk_id = pkey->getPublicKey().getId();
    if (auto err = gnutls_x509_crt_set_subject_key_id(ch.c, &pk_id, sizeof(pk_id)))
        throw crypto::CryptoException(std::string {"set_subject_key_id: "} + gnutls_strerror(err));

    auto common = fmt::format("sim-{:016x}-{:04x}", identity_seed, static_cast<unsigned>(index));
    if (auto err = gnutls_x509_crt_set_dn_by_oid(ch.c, GNUTLS_OID_X520_COMMON_NAME, 0, common.data(), common.size()))
        throw crypto::CryptoException(std::string {"set_dn CN: "} + gnutls_strerror(err));
    auto uid_str = pk_id.toString();
    if (auto err = gnutls_x509_crt_set_dn_by_oid(ch.c, GNUTLS_OID_LDAP_UID, 0, uid_str.data(), uid_str.size()))
        throw crypto::CryptoException(std::string {"set_dn UID: "} + gnutls_strerror(err));

    if (auto err = gnutls_x509_crt_set_ca_status(ch.c, 1))
        throw crypto::CryptoException(std::string {"set_ca_status: "} + gnutls_strerror(err));
    if (auto err = gnutls_x509_crt_set_key_usage(ch.c,
                                                 GNUTLS_KEY_DIGITAL_SIGNATURE | GNUTLS_KEY_KEY_CERT_SIGN
                                                 | GNUTLS_KEY_CRL_SIGN))
        throw crypto::CryptoException(std::string {"set_key_usage: "} + gnutls_strerror(err));

    if (auto err = gnutls_x509_crt_sign2(ch.c, ch.c, pk_handle, GNUTLS_DIG_SHA256, 0))
        throw crypto::CryptoException(std::string {"sign2: "} + gnutls_strerror(err));

    auto cert = std::make_shared<crypto::Certificate>(ch.release());
    return {std::move(pkey), std::move(cert)};
}

} // namespace

IdentityCache::IdentityCache(std::string cache_dir)
    : cache_dir_(std::move(cache_dir))
{}

std::string
IdentityCache::pathFor(uint64_t identity_seed, size_t index) const
{
    if (cache_dir_.empty())
        return {};
    return fmt::format("{}/{:016x}/{:04x}", cache_dir_, identity_seed, static_cast<unsigned>(index));
}

bool
IdentityCache::isPersisted(uint64_t identity_seed, size_t index) const
{
    auto base = pathFor(identity_seed, index);
    if (base.empty())
        return false;
    std::error_code ec;
    return fs::exists(base + ".pem", ec) && fs::exists(base + ".crt", ec);
}

void
IdentityCache::clearMemory()
{
    std::lock_guard<std::mutex> lk(mu_);
    mem_.clear();
}

crypto::Identity
IdentityCache::get(uint64_t identity_seed, size_t index)
{
    std::lock_guard<std::mutex> lk(mu_);
    auto key = std::make_pair(identity_seed, index);
    if (auto it = mem_.find(key); it != mem_.end())
        return it->second;

    auto base = pathFor(identity_seed, index);
    if (!base.empty() && fs::exists(base + ".pem") && fs::exists(base + ".crt")) {
        auto id = crypto::loadIdentity(base);
        mem_.emplace(key, id);
        return id;
    }

    auto id = generateDeterministic(identity_seed, index);

    if (!base.empty()) {
        std::error_code ec;
        fs::create_directories(fs::path(base).parent_path(), ec);
        crypto::saveIdentity(id, base);
    }
    mem_.emplace(key, id);
    return id;
}

} // namespace sim
} // namespace dht
