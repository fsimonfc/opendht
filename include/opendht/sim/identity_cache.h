// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "../crypto.h"
#include "../def.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace dht {
namespace sim {

/**
 * Persistent identity cache keyed by (identity_seed, index).
 *
 * Identities are generated **deterministically** via GnuTLS
 * `gnutls_x509_privkey_generate2` with `GNUTLS_PRIVKEY_FLAG_PROVABLE` and a
 * `GNUTLS_KEYGEN_SEED` derived from `(identity_seed, index)` (SHA-256). The
 * X.509 certificate is built with fixed activation/expiration/serial and
 * signed with RSA PKCS#1 v1.5, all of which are deterministic. For a given
 * `(identity_seed, index)` and a fixed GnuTLS/nettle pair, the resulting key
 * and certificate are byte-identical across runs and across machines.
 *
 * On `get(seed, i)`:
 *   - if `cache_dir/<seed>/<i>.{pem,crt}` exists -> load and return.
 *   - otherwise generate deterministically and (if `cache_dir` is set)
 *     persist for fast subsequent runs.
 *
 * On-disk persistence is therefore an **optimization** (RSA-3072 keygen with
 * provable primes is slow), not a correctness mechanism.
 *
 * Thread-safety: serialized by an internal mutex. The cache is intended to be
 * shared across `Simulator` instances and across tests in the same process.
 */
class OPENDHT_PUBLIC IdentityCache
{
public:
    /** @param cache_dir  base directory (e.g. tests/data/sim-identities). Empty
     *                    disables on-disk persistence (in-memory only). */
    explicit IdentityCache(std::string cache_dir = {});

    /** Look up or generate-and-persist `(identity_seed, index)`. Throws on I/O
     *  or crypto failure. */
    crypto::Identity get(uint64_t identity_seed, size_t index);

    /** Path to the on-disk file used for `(identity_seed, index)` (without
     *  extension; saveIdentity appends `.pem` and `.crt`). Empty if no
     *  cache_dir was provided. */
    std::string pathFor(uint64_t identity_seed, size_t index) const;

    /** True if `pathFor(seed, index) + ".pem"` already exists on disk. */
    bool isPersisted(uint64_t identity_seed, size_t index) const;

    /** Clear in-memory entries (does not touch the on-disk cache). */
    void clearMemory();

private:
    std::string cache_dir_;
    mutable std::mutex mu_;
    std::map<std::pair<uint64_t, size_t>, crypto::Identity> mem_;
};

} // namespace sim
} // namespace dht
