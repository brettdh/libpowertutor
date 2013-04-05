#ifndef libpowertutor_debug_h_incl
#define libpowertutor_debug_h_incl

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

#ifdef ANDROID
//#  define LOG_TAG "libpowertutor"
//#  include <android/log.h>
//#  define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
//#  define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#  define LOGD(...) dbgprintf(__VA_ARGS__)
#  define LOGE(...) dbgprintf(__VA_ARGS__)

CDECL void dbgprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else
//#  define SIMULATION_LOG
#  ifdef SIMULATION_LOG
#    define LOGD printf
#    define LOGE(...) fprintf(stderr, __VA_ARGS__)
#  else
#    define LOGD(...)
#    define LOGE(...)
#  endif
#endif


#endif
