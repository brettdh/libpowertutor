LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
MY_OUT := $(MY_ANDROID_SRC_ROOT)/out/target/product/generic

LOCAL_MODULE := libpowertutor
WIFI_INCLUDES := $(call include-path-for, libhardware_legacy)/hardware_legacy
LOCAL_C_INCLUDES := $(WIFI_INCLUDES)
LOCAL_SRC_FILES := libpowertutor.cpp timeops.cpp
LOCAL_CFLAGS := -g -O0 -Wall -Werror
LOCAL_SHARED_LIBRARIES := liblog #libhardware_legacy 
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
