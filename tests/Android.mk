LOCAL_PATH := $(call my-dir)

common_C_INCLUDES := external/bdh_apps/cppunit/include \
                     external/bdh_apps/libcmm
#                    external/openssl/include
common_CFLAGS:=-g -O0 -Wall -Werror
common_STATIC_LIBRARIES:=libcppunit #libboost_thread
TESTSUITE_SRCS := run_all_tests.cpp test_common.cpp

# unit tests
include $(CLEAR_VARS)
TEST_SRCS := network_state_test.cpp

LOCAL_MODULE := run_libpt_unit_tests
LOCAL_SRC_FILES := $(TESTSUITE_SRCS) $(TEST_SRCS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_CFLAGS := $(common_CFLAGS)

# We need libpowertutor to be a static lib here because there's
# apparently a bug in the CrystaX toolchain that causes a segfault when
# exceptions are thrown between shared libs.  Or something like that.
# It's weird, because the exception is being thrown only in the
# testing executable.  But changing libpowertutor to a static lib
# removes the segfault, so that's what I'm doing for now.
# I think I'll build the shared lib too, though, for use on the device.
# Otherwise, everything that links with libpowertutor also has to link
# with libwpa_client to pull in the functions that are used for 
# querying the link speed.
LOCAL_STATIC_LIBRARIES := $(common_STATIC_LIBRARIES) libpowertutor
LOCAL_SHARED_LIBRARIES := liblog libwpa_client

include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := test_sending_estimation
LOCAL_SRC_FILES := test_sending_estimation.cpp ../utils.cpp \
	$(addprefix ../../libcmm/, libcmm_external_ipc.cpp debug.cpp)
LOCAL_CFLAGS := $(common_CFLAGS)
LOCAL_C_INCLUDES := $(common_C_INCLUDES)
LOCAL_SHARED_LIBRARIES := liblog libpowertutor
include $(BUILD_EXECUTABLE)
