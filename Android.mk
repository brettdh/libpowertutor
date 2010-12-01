LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
MY_OUT := $(MY_ANDROID_SRC_ROOT)/out/target/product/generic

LOCAL_MODULE := libpowertutor
LOCAL_SRC_FILES := libpowertutor.cpp timeops.cpp
LOCAL_CFLAGS := -g -O0 -Wall -Werror
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
