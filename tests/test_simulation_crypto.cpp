// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#include "test_simulation_crypto.h"

#include <opendht/crypto.h>
#include <opendht/sim/identity_cache.h>

#include <filesystem>
#include <fmt/format.h>
#include <unistd.h>

namespace test {

CPPUNIT_TEST_SUITE_REGISTRATION(SimulationCryptoTester);

namespace fs = std::filesystem;
using dht::sim::IdentityCache;

namespace {

fs::path
makeTempDir(const char* tag)
{
    auto base = fs::temp_directory_path() / fmt::format("opendht-sim-cache-{}-{}", tag, ::getpid());
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    return base;
}

} // namespace

void
SimulationCryptoTester::testInMemoryCacheReuse()
{
    // No on-disk cache. Repeated get(seed, idx) returns the *same* Identity
    // object (shared_ptr equality), proving the in-memory map hits.
    IdentityCache cache; // empty cache_dir => in-memory only
    auto a = cache.get(0xC0DE, 0);
    auto b = cache.get(0xC0DE, 0);
    CPPUNIT_ASSERT(a.first.get() == b.first.get());
    CPPUNIT_ASSERT(a.second.get() == b.second.get());
}

void
SimulationCryptoTester::testOnDiskCachePersistence()
{
    auto dir = makeTempDir("persist");

    // First cache instance: cold start. Cache should generate and persist.
    {
        IdentityCache cache(dir.string());
        CPPUNIT_ASSERT(!cache.isPersisted(0xABCD, 7));
        auto id = cache.get(0xABCD, 7);
        CPPUNIT_ASSERT(id.first && id.second);
        CPPUNIT_ASSERT(cache.isPersisted(0xABCD, 7));
    }

    // Second cache instance pointing at the same dir: should load the same
    // bytes from disk (no regeneration).
    IdentityCache c1(dir.string());
    IdentityCache c2(dir.string());
    auto a = c1.get(0xABCD, 7);
    auto b = c2.get(0xABCD, 7);
    auto a_pub = a.second->getPacked();
    auto b_pub = b.second->getPacked();
    CPPUNIT_ASSERT_EQUAL(a_pub.size(), b_pub.size());
    CPPUNIT_ASSERT(a_pub == b_pub);
    CPPUNIT_ASSERT_EQUAL(a.first->getPublicKey().getId(), b.first->getPublicKey().getId());

    std::error_code ec;
    fs::remove_all(dir, ec);
}

void
SimulationCryptoTester::testDifferentSeedDifferentIdentity()
{
    auto dir = makeTempDir("seed");
    IdentityCache cache(dir.string());

    auto a = cache.get(0x1111, 0);
    auto b = cache.get(0x2222, 0);
    auto c = cache.get(0x1111, 1);

    // Different (seed,index) pairs must produce different identities. We compare
    // via the public-key id which is a SHA-1-style fingerprint.
    CPPUNIT_ASSERT(a.first->getPublicKey().getId() != b.first->getPublicKey().getId());
    CPPUNIT_ASSERT(a.first->getPublicKey().getId() != c.first->getPublicKey().getId());
    CPPUNIT_ASSERT(b.first->getPublicKey().getId() != c.first->getPublicKey().getId());

    std::error_code ec;
    fs::remove_all(dir, ec);
}

void
SimulationCryptoTester::testDeterministicAcrossInstances()
{
    // No on-disk cache: identities come from gnutls_x509_privkey_generate2
    // with a seeded GNUTLS_KEYGEN_SEED. Two independent IdentityCache
    // instances must therefore produce byte-identical key+cert bytes.
    IdentityCache c1; // in-memory only
    IdentityCache c2;

    auto a = c1.get(0xFEED, 3);
    auto b = c2.get(0xFEED, 3);

    auto a_key = a.first->serialize();
    auto b_key = b.first->serialize();
    auto a_crt = a.second->getPacked();
    auto b_crt = b.second->getPacked();

    CPPUNIT_ASSERT_EQUAL(a_key.size(), b_key.size());
    CPPUNIT_ASSERT(a_key == b_key);
    CPPUNIT_ASSERT_EQUAL(a_crt.size(), b_crt.size());
    CPPUNIT_ASSERT(a_crt == b_crt);
    CPPUNIT_ASSERT_EQUAL(a.first->getPublicKey().getId(), b.first->getPublicKey().getId());
}

} // namespace test
