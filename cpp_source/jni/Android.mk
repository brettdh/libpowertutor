LOCAL_PATH := $(call my-dir)

MY_SRCS := libpowertutor.cpp power_model.cpp timeops.cpp utils.cpp jni_wrappers.cpp debug.cpp
MY_CFLAGS := -g -ggdb -Wall -Werror -DANDROID -DNDK_BUILD -std=c++11

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libpowertutor
LOCAL_SRC_FILES := $(addprefix ../, $(MY_SRCS))
LOCAL_CFLAGS := $(MY_CFLAGS) -DBUILDING_SHLIB
LOCAL_EXPORT_CFLAGS := $(MY_CFLAGS) -DBUILDING_SHLIB
LOCAL_SHARED_LIBRARIES := liblog mocktime
LOCAL_LDLIBS := -llog
LOCAL_PRELINK_MODULE := false
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/..
include $(BUILD_SHARED_LIBRARY)

$(call import-module, edu.umich.mobility/mocktime)
