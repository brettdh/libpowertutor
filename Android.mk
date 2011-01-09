LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
MY_OUT := $(MY_ANDROID_SRC_ROOT)/out/target/product/generic

MY_SRCS := libpowertutor.cpp timeops.cpp wifi.cpp utils.cpp
MY_CFLAGS := -g -O0 -Wall -Werror -DANDROID

LOCAL_MODULE := libpowertutor
LOCAL_C_INCLUDES := $(WIFI_INCLUDES)
LOCAL_SRC_FILES := $(MY_SRCS)
LOCAL_CFLAGS := $(MY_CFLAGS)
LOCAL_STATIC_LIBRARIES := liblog libcutils libwpa_client
include $(BUILD_STATIC_LIBRARY)

# Use the static lib if there happen to be exception-related
# segfaults when using the shared lib.
include $(CLEAR_VARS)
LOCAL_MODULE := libpowertutor
LOCAL_C_INCLUDES := $(WIFI_INCLUDES)
LOCAL_SRC_FILES := $(MY_SRCS)
LOCAL_CFLAGS := $(MY_CFLAGS) -DBUILDING_SHLIB
LOCAL_SHARED_LIBRARIES := liblog libcutils libwpa_client
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
