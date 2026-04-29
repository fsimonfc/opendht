// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace test {

class SimulationCryptoTester : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(SimulationCryptoTester);
    CPPUNIT_TEST(testInMemoryCacheReuse);
    CPPUNIT_TEST(testOnDiskCachePersistence);
    CPPUNIT_TEST(testDifferentSeedDifferentIdentity);
    CPPUNIT_TEST(testDeterministicAcrossInstances);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override {}
    void tearDown() override {}

    void testInMemoryCacheReuse();
    void testOnDiskCachePersistence();
    void testDifferentSeedDifferentIdentity();
    void testDeterministicAcrossInstances();
};

} // namespace test
