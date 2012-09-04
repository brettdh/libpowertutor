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
    CPPUNIT_TEST(testReset);
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
        
        libpowertutor_init_mocking();
    }

    void tearDown() {
        //mocktime_gettimeofday(&start, NULL);
    }
    
    void testIdleNetworkEnergy() {
        reset_stats();
        
        int energy_mJ = energy_consumed_since_reset();
        CPPUNIT_ASSERT_EQUAL(0, energy_mJ);

        mocktime_usleep(1000000);
        update_energy_stats();

        energy_mJ = energy_consumed_since_reset();
        CPPUNIT_ASSERT(energy_mJ > 0); // idle consumption is nonzero
        CPPUNIT_ASSERT(energy_mJ < 100); // but not very much
    }

    void testCellularNetworkEnergy() {
        add_bytes_up(TYPE_MOBILE, 500);
        add_packets_up(TYPE_MOBILE, 1);
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
        add_bytes_up(TYPE_WIFI, 50000);
        add_packets_up(TYPE_WIFI, 50);
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

    void testReset() {
        testWifiNetworkEnergy();

        libpowertutor_init_mocking();
        
        testWifiNetworkEnergy();
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
