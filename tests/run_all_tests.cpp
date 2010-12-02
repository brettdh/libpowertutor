#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/Test.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestListener.h>
#include <cppunit/TestFailure.h>
#include <cppunit/Exception.h>
#include <cppunit/SourceLine.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using std::exception; using std::string; using std::endl;

using CppUnit::TestFactoryRegistry;
using CppUnit::TestListener; using CppUnit::TestRunner;
using CppUnit::TestResult; using CppUnit::TestFailure;
using CppUnit::Test;

//bool g_receiver = false;
//char *g_hostname = (char*)"localhost";

#define LOG_TAG "AndroidLogTestHarness"
#include <cutils/log.h>

class AndroidLogTestListener : public TestListener {
    TestFailure *lastFailure;
  public:
    AndroidLogTestListener() : lastFailure(NULL) {
        //DEBUG_LOG("Created test listener\n");
    }
    
    virtual ~AndroidLogTestListener() {
        //DEBUG_LOG("Destroyed test listener\n");
    }
    
    virtual void startTest(Test *test) {
        //DEBUG_LOG("Starting new test\n");
        //upcall("addTest", "(Ljava/lang/String;)V", test->getName().c_str());
        LOGD("Running test: %s\n", test->getName().c_str());
    }
    
    virtual void addFailure(const TestFailure &failure) {
        lastFailure = failure.clone();
    }
    
    virtual void endTest(Test *test) {
        if (lastFailure) {
            //DEBUG_LOG("Current test failed\n");
            std::ostringstream s;
            s << lastFailure->sourceLine().fileName() << ":"
              << lastFailure->sourceLine().lineNumber() << endl
              << lastFailure->thrownException()->what();
            //upcall("testFailure", "(Ljava/lang/String;Ljava/lang/String;)V",
            //       test->getName().c_str(), s.str().c_str());
            LOGD("Test failed: %s\n", test->getName().c_str());
            LOGD("%s\n", s.str().c_str());
            
            delete lastFailure;
            lastFailure = NULL;
        } else {
            LOGD("Test passed: %s\n", test->getName().c_str());
            //DEBUG_LOG("Current test passed\n");
            //upcall("testSuccess", "(Ljava/lang/String;)V", 
            //       test->getName().c_str());
        }
    }
    
    void addFailureMessage(const string& testName, const string& msg) {
        //upcall("addTest", "(Ljava/lang/String;)V", testName.c_str());
        //upcall("testFailure", "(Ljava/lang/String;Ljava/lang/String;)V",
        //       testName.c_str(), msg.c_str());
        LOGD("Test failed: %s\n", testName.c_str());
        LOGD("%s\n", msg.c_str());
    }
    
    virtual void startSuite(Test * /*suite*/) {}
    
    virtual void endSuite(Test * /*suite*/) {}
};

static void run_all_tests()
{
    LOGD("Running tests...\n");
    TestFactoryRegistry& registry = TestFactoryRegistry::getRegistry();
    //CppUnit::TextUi::TestRunner runner;
    
    AndroidLogTestListener listener;
    TestResult result;
    result.addListener(&listener);
    
    TestRunner runner;
    runner.addTest(registry.makeTest());
    runner.run(result);
}

int main(/*int argc, char *argv[]*/)
{
    // int ch;
    // while ((ch = getopt(argc, argv, "lh:")) != -1) {
    //     switch (ch) {
    //     case 'l':
    //         g_receiver = true;
    //         break;
    //     case 'h':
    //         g_hostname = optarg;
    //         break;
    //     case '?':
    //         exit(EXIT_FAILURE);
    //     default:
    //         break;
    //     }
    // }
    
    try {
        run_all_tests();
    } catch (exception& e) {
        LOGD("Error running tests: %s\n", e.what());
    }
    return 0;
}
