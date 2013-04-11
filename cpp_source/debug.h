#ifndef libpowertutor_debug_h_incl
#define libpowertutor_debug_h_incl

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

//#define SIMULATION_LOG
#ifdef SIMULATION_LOG
#  define LOGD printf
#  define LOGE(...) fprintf(stderr, __VA_ARGS__)
#else
#  define LOGD(...) dbgprintf(__VA_ARGS__)
#  define LOGE(...) dbgprintf(__VA_ARGS__)

CDECL void dbgprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif


#endif
