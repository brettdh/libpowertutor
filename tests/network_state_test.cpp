#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include "network_state_test.h"

#define LOG_TAG "NetworkStateTest"
#include <cutils/log.h>

CPPUNIT_TEST_SUITE_REGISTRATION(NetworkStateTest);

extern int get_mobile_queue_len(int *down, int *up);
extern int wifi_channel_rate();
extern int wifi_packet_rate();
extern int wifi_uplink_data_rate();
extern int update_wifi_estimated_rates(bool&);

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
    int down = -1, up = -1;
    (void)get_mobile_queue_len(&down, &up);
    
    CPPUNIT_ASSERT_MESSAGE("Got downlink queue length", down >= 0);
    LOGD("Got downlink queue length: %d\n", down);
    
    CPPUNIT_ASSERT_MESSAGE("Got uplink queue length", up >= 0);
    LOGD("Got uplink queue length: %d\n", up);
}

void 
NetworkStateTest::testWifiParams()
{
    bool dummy;
    for (int i = -1; i < 5; ++i) {
        int rc = update_wifi_estimated_rates(dummy);
        CPPUNIT_ASSERT_MESSAGE("Updating wifi state succeeded", rc == 0);
        if (i >= 0) {
            int packet_rate = wifi_packet_rate();
            int data_rate = wifi_uplink_data_rate();
            LOGD("Got packet rate %d data rate %d\n", packet_rate, data_rate);
            CPPUNIT_ASSERT_MESSAGE("Got packet rate", packet_rate >= 0);
            CPPUNIT_ASSERT_MESSAGE("Got uplink data rate", data_rate >= 0);
        }
        sleep(1);
    }
}
