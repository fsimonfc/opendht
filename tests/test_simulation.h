// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace test {

class SimulationTester : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(SimulationTester);
    CPPUNIT_TEST(testSimClockAdvance);
    CPPUNIT_TEST(testSingleNodeStart);
    CPPUNIT_TEST(testTwoNodePacketDelivery);
    CPPUNIT_TEST(testMultiNodeBootstrap);
    CPPUNIT_TEST(testDeterministicTrace);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override {}
    void tearDown() override {}

    void testSimClockAdvance();
    void testSingleNodeStart();
    void testTwoNodePacketDelivery();
    void testMultiNodeBootstrap();
    void testDeterministicTrace();
};

} // namespace test
