LOCAL_PATH := $(call my-dir)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
#ANDROID_INCLUDES := $(MY_ANDROID_SRC_ROOT)/system/core/include
#ANDROID_INCLUDES += $(MY_ANDROID_SRC_ROOT)/external/wpa_supplicant
ANDROID_INCLUDES += $(HOME)/src/android-ndk-r8b/sources/
#ANDROID_INCLUDES += $(MY_ANDROID_SRC_ROOT)/external/bdh_apps/mocktime/

MY_SRCS := libpowertutor.cpp power_model.cpp timeops.cpp utils.cpp jni_wrappers.cpp debug.cpp
MY_CFLAGS := -g -ggdb -O0 -Wall -Werror -DANDROID -DNDK_BUILD -std=c++11

#include $(CLEAR_VARS)

#LOCAL_MODULE := libmocktime
#LOCAL_SRC_FILES := ../../../mocktime/obj/local/$(TARGET_ARCH_ABI)/libmocktime.so
#include $(PREBUILT_SHARED_LIBRARY)

# Use the static lib if there happen to be exception-related
# segfaults when using the shared lib.
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libpowertutor
LOCAL_C_INCLUDES := $(ANDROID_INCLUDES)
LOCAL_SRC_FILES := $(addprefix ../, $(MY_SRCS))
LOCAL_CFLAGS := $(MY_CFLAGS) -DBUILDING_SHLIB
LOCAL_SHARED_LIBRARIES := liblog mocktime
LOCAL_LDLIBS := -llog
#LOCAL_LDFLAGS := -L../../mocktime/obj/local/$(TARGET_ARCH_ABI) -lmocktime
LOCAL_PRELINK_MODULE := false
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/..
include $(BUILD_SHARED_LIBRARY)

$(call import-module, edu.umich.mobility/mocktime)
