LOCAL_PATH := $(call my-dir)

DROID_ROOT := /Users/brettdh/src/android-source
common_C_INCLUDES := $(DROID_ROOT)/external/bdh_apps/cppunit/include \
                     $(DROID_ROOT)/external/bdh_apps/libcmm

common_CFLAGS:=-g -O0 -Wall -Werror -DANDROID -DNDK_BUILD
common_STATIC_LIBRARIES:=libcppunit #libboost_thread
TESTSUITE_SRCS := ../run_all_tests.cpp ../test_common.cpp

LOCAL_MODULE := cppunit
LOCAL_SRC_FILES := ../../../../libcmm/android_libs/libcppunit.a
include $(PREBUILT_STATIC_LIBRARY)

# unit tests
include $(CLEAR_VARS)
TEST_SRCS := ../network_state_test.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := run_libpt_unit_tests
LOCAL_SRC_FILES := $(TESTSUITE_SRCS) $(TEST_SRCS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_CFLAGS := $(common_CFLAGS)

LOCAL_STATIC_LIBRARIES := $(common_STATIC_LIBRARIES) 
LOCAL_SHARED_LIBRARIES := liblog libwpa_client libpowertutor

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := test_sending_estimation
LOCAL_SRC_FILES := ../test_sending_estimation.cpp ../../utils.cpp ../../timeops.cpp \
	$(addprefix ../../../../libcmm/, libcmm_external_ipc.cpp debug.cpp)
LOCAL_CFLAGS := $(common_CFLAGS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := liblog libpowertutor
include $(BUILD_EXECUTABLE)
