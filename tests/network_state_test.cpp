#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cutils/log.h>
#include "network_state_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(NetworkStateTest);

extern int get_mobile_queue_len(bool downlink);
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
NetworkStateTest::testPacketRate()
{

}

void 
NetworkStateTest::testQueueSize()
{
    int down = get_mobile_queue_len(true);
    int up = get_mobile_queue_len(false);
    
    if (down < 0) {
        LOGE("Failed to get downlink queue length!\n");
    } else {
        LOGD("Got downlink queue length: %d\n", down);
    }
    
    if (up < 0) {
        LOGE("Failed to get uplink queue length!\n");
    } else {
        LOGD("Got uplink queue length: %d\n", up);
    }
    //CPPUNIT_ASSERT_MESSAGE("Got downlink queue length", down >= 0);
    //CPPUNIT_ASSERT_MESSAGE("Got uplink queue length", up >= 0);
}
