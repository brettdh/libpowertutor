LOCAL_PATH := $(call my-dir)

DROID_ROOT := /Users/brettdh/src/android-source
common_C_INCLUDES := $(DROID_ROOT)/external/bdh_apps/cppunit/include \
                     $(DROID_ROOT)/external/bdh_apps/libcmm

common_CFLAGS:=-g -O3 -Wall -Werror -DANDROID -DNDK_BUILD  -std=c++11
common_STATIC_LIBRARIES:=cppunit #libboost_thread
TESTSUITE_SRCS := ../run_all_tests.cpp ../test_common.cpp

include $(CLEAR_VARS)

LOCAL_MODULE := libcppunit
LOCAL_SRC_FILES := libcppunit.a
#include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libpowertutor
LOCAL_SRC_FILES := ../../obj/local/armeabi/libpowertutor.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libmocktime
LOCAL_SRC_FILES := ../../../../mocktime/obj/local/armeabi/libmocktime.so
include $(PREBUILT_SHARED_LIBRARY)

# unit tests
include $(CLEAR_VARS)
LOCAL_MODULE := test_functor
LOCAL_SRC_FILES := ../test_functor.cpp ../../utils.cpp ../../timeops.cpp
LOCAL_CFLAGS := $(common_CFLAGS)
LOCAL_CPP_INCLUDES := $(common_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := liblog libpowertutor
LOCAL_LDLIBS := -L./obj/local/armeabi -L$(SYSROOT)/usr/lib -llog
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
TEST_SRCS := ../network_state_test.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := run_libpt_unit_tests
LOCAL_SRC_FILES := $(TESTSUITE_SRCS) $(TEST_SRCS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_CFLAGS := $(common_CFLAGS)

LOCAL_STATIC_LIBRARIES := $(common_STATIC_LIBRARIES) 
LOCAL_SHARED_LIBRARIES := powertutor mocktime
LOCAL_LDLIBS := -L./obj/local/armeabi -llog -lmocktime
LOCAL_PRELINK_MODULE := false
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog
#include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := test_sending_estimation
LOCAL_SRC_FILES := ../test_sending_estimation.cpp ../../utils.cpp ../../timeops.cpp \
	$(addprefix ../../../../libcmm/, libcmm_external_ipc.cpp debug.cpp)
LOCAL_CFLAGS := $(common_CFLAGS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := liblog libpowertutor
LOCAL_LDLIBS := -L./obj/local/armeabi -L$(SYSROOT)/usr/lib -llog -lmocktime
#include $(BUILD_EXECUTABLE)
