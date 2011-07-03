LOCAL_PATH := $(call my-dir)

MY_ANDROID_SRC_ROOT := $(HOME)/src/android-source
ANDROID_INCLUDES := $(MY_ANDROID_SRC_ROOT)/system/core/include
ANDROID_INCLUDES += $(MY_ANDROID_SRC_ROOT)/external/wpa_supplicant

MY_SRCS := libpowertutor.cpp timeops.cpp wifi.cpp utils.cpp
MY_CFLAGS := -g -O0 -Wall -Werror -DANDROID

include $(CLEAR_VARS)

LOCAL_MODULE := libcutils
LOCAL_SRC_FILES := prebuilt/libcutils.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := libwpa_client
LOCAL_SRC_FILES := prebuilt/libwpa_client.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

# Use the static lib if there happen to be exception-related
# segfaults when using the shared lib.
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libpowertutor
LOCAL_C_INCLUDES := $(ANDROID_INCLUDES)
LOCAL_SRC_FILES := $(addprefix ../, $(MY_SRCS))
LOCAL_CFLAGS := $(MY_CFLAGS) -DBUILDING_SHLIB
LOCAL_SHARED_LIBRARIES := liblog libcutils libwpa_client
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
