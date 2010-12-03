#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include "network_state_test.h"

#define LOG_TAG "NetworkStateTest"
#include <cutils/log.h>

CPPUNIT_TEST_SUITE_REGISTRATION(NetworkStateTest);

extern int get_mobile_queue_len(bool downlink);
extern int wifi_channel_rate();
extern int wifi_packet_rate();

void 
NetworkStateTest::setUp()
{
}

void 
NetworkStateTest::tearDown()
{
}

void 
NetworkStateTest::testChannelRate()
{
    int rate = wifi_channel_rate();
    CPPUNIT_ASSERT_MESSAGE("Got channel rate", rate >= 0);
    LOGD("Got channel rate: %dMbps\n", rate);
}

void 
NetworkStateTest::testQueueSize()
{
    int down = get_mobile_queue_len(true);
    int up = get_mobile_queue_len(false);
    
    CPPUNIT_ASSERT_MESSAGE("Got downlink queue length", down >= 0);
    LOGD("Got downlink queue length: %d\n", down);
    
    CPPUNIT_ASSERT_MESSAGE("Got uplink queue length", up >= 0);
    LOGD("Got uplink queue length: %d\n", up);
}
