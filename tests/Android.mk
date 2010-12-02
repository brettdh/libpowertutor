LOCAL_PATH := $(call my-dir)

common_C_INCLUDES := external/bdh_apps/cppunit/include \
                     external/bdh_apps/libcmm
#                    external/openssl/include
common_CFLAGS:=-g -O0 -Wall -Werror
common_STATIC_LIBRARIES:=libcppunit #libboost_thread
TESTSUITE_SRCS := run_all_tests.cpp test_common.cpp #StdioOutputter.cpp

# unit tests
include $(CLEAR_VARS)
TEST_SRCS := network_state_test.cpp

LOCAL_MODULE := run_libpt_unit_tests
LOCAL_SRC_FILES := $(TESTSUITE_SRCS) $(TEST_SRCS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_CFLAGS := $(common_CFLAGS)
LOCAL_STATIC_LIBRARIES := $(common_STATIC_LIBRARIES)
LOCAL_SHARED_LIBRARIES := libpowertutor liblog
include $(BUILD_EXECUTABLE)
