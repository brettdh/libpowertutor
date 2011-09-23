LOCAL_PATH := $(call my-dir)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
LIBCMM_ROOT := $(MY_ANDROID_SRC_ROOT)/external/bdh_apps/libcmm

include $(CLEAR_VARS)

LOCAL_MODULE := powertutor
LOCAL_SRC_FILES := ../../../cpp_source/obj/local/armeabi/libpowertutor.so
include $(PREBUILT_SHARED_LIBRARY)
