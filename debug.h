#ifndef libpowertutor_debug_h_incl
#define libpowertutor_debug_h_incl

#ifdef ANDROID
#define LOG_TAG "libpowertutor"
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGD printf
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

#endif
