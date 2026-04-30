// Copyright (c) 2014-2026 Savoir-faire Linux Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace test {

class SimulationWorkloadsTester : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(SimulationWorkloadsTester);
    CPPUNIT_TEST(testPutGetWorkload);
    CPPUNIT_TEST(testListenPutWorkload);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override {}
    void tearDown() override {}

    void testPutGetWorkload();
    void testListenPutWorkload();
};

} // namespace test
