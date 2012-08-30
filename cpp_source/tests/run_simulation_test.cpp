#include "libpowertutor.h"
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "libpowertutor.h"
#include "mocktime.h"

using CppUnit::TestFactoryRegistry;

class SimulationTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(SimulationTest);
    CPPUNIT_TEST(testIdleNetworkEnergy);
    CPPUNIT_TEST(testCellularNetworkEnergy);
    CPPUNIT_TEST(testWifiNetworkEnergy);
    CPPUNIT_TEST_SUITE_END();
    
    void makePowerModelIdle() {
        for (int i = 0; i < 60; ++i) {
            mocktime_usleep(1000000);
            update_energy_stats();
        }
    }

  public:
    void setUp() {
        mocktime_enable_mocking();
        
        if (start.tv_sec == 0) {
            gettimeofday(&start, NULL);
        }
        mocktime_settimeofday(&start, NULL);
        
        reset_stats();
    }

    void tearDown() {
        mocktime_gettimeofday(&start, NULL);
    }
    
    void testIdleNetworkEnergy() {
        int energy_mJ = energy_consumed_since_reset();
        CPPUNIT_ASSERT_EQUAL(0, energy_mJ);

        mocktime_usleep(1000000);
        update_energy_stats();

        energy_mJ = energy_consumed_since_reset();
        CPPUNIT_ASSERT(energy_mJ > 0); // idle consumption is nonzero
        CPPUNIT_ASSERT(energy_mJ < 100); // but not very much
    }

    void testCellularNetworkEnergy() {
        int bytes[2] = {0, 500}, packets[2] = {0, 1};
        set_mocked_net_dev_stats(TYPE_MOBILE, bytes, packets);
        mocktime_usleep(1000000);
        update_energy_stats();
        
        int energy_mJ = energy_consumed_since_reset();
        CPPUNIT_ASSERT(energy_mJ > 0);
        CPPUNIT_ASSERT(energy_mJ > 500); // DCH consumption

        // test tail energy for beginning of tail
        for (size_t i = 0; i < 3; ++i) {
            mocktime_usleep(1000000);
            update_energy_stats();
            energy_mJ = energy_consumed_since_reset() - energy_mJ;
            CPPUNIT_ASSERT(energy_mJ > 500);
        }

        makePowerModelIdle();
    }

    void testWifiNetworkEnergy() {
        int bytes[2] = {0, 50000}, packets[2] = {0, 50};
        set_mocked_net_dev_stats(TYPE_WIFI, bytes, packets);
        mocktime_usleep(1000000);
        update_energy_stats();

        int energy_mJ = energy_consumed_since_reset();
        CPPUNIT_ASSERT(energy_mJ > 0);
        CPPUNIT_ASSERT(energy_mJ > 400); // wifi-high consumption

        mocktime_usleep(1000000);
        update_energy_stats();

        energy_mJ = energy_consumed_since_reset() - energy_mJ;
        CPPUNIT_ASSERT(energy_mJ > 0);
        CPPUNIT_ASSERT(energy_mJ < 100); // wifi-low consumption
    }
    
  private:
    static struct timeval start;
};

struct timeval SimulationTest::start = {0, 0};

CPPUNIT_TEST_SUITE_REGISTRATION(SimulationTest);

static void run_all_tests()
{
    TestFactoryRegistry& registry = TestFactoryRegistry::getRegistry();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest(registry.makeTest());
    runner.run();
}

int main()
{
    run_all_tests();

    return 0;
}
