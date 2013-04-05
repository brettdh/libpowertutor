LOCAL_PATH := $(call my-dir)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
ANDROID_INCLUDES := $(MY_ANDROID_SRC_ROOT)/system/core/include
ANDROID_INCLUDES += $(MY_ANDROID_SRC_ROOT)/external/wpa_supplicant
ANDROID_INCLUDES += $(HOME)/src/android-ndk-r8b/sources/
ANDROID_INCLUDES += $(MY_ANDROID_SRC_ROOT)/external/bdh_apps/mocktime/

MY_SRCS := libpowertutor.cpp power_model.cpp timeops.cpp utils.cpp jni_wrappers.cpp debug.cpp
MY_CFLAGS := -g -ggdb -O0 -Wall -Werror -DANDROID

include $(CLEAR_VARS)

LOCAL_MODULE := libmocktime
LOCAL_SRC_FILES := ../../../mocktime/obj/local/$(TARGET_ARCH_ABI)/libmocktime.so
include $(PREBUILT_SHARED_LIBRARY)

# Use the static lib if there happen to be exception-related
# segfaults when using the shared lib.
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libpowertutor
LOCAL_C_INCLUDES := $(ANDROID_INCLUDES)
LOCAL_SRC_FILES := $(addprefix ../, $(MY_SRCS))
LOCAL_CFLAGS := $(MY_CFLAGS) -DBUILDING_SHLIB
LOCAL_SHARED_LIBRARIES := liblog libmocktime
LOCAL_LDLIBS := -llog
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
