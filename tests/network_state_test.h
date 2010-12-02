#ifndef NETWORK_STATE_TEST_H_1HOUZMOH
#define NETWORK_STATE_TEST_H_1HOUZMOH

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class NetworkStateTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(NetworkStateTest);
    //CPPUNIT_TEST(testChannelRate);
    //CPPUNIT_TEST(testQueueSize);
    CPPUNIT_TEST_SUITE_END();    

  public:
    void setUp();
    void tearDown();

    void testChannelRate();
    void testQueueSize();
};

#endif /* end of include guard: NETWORK_STATE_TEST_H_1HOUZMOH */
