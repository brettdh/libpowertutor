#ifndef libpowertutor_debug_h_incl
#define libpowertutor_debug_h_incl


//#define LOGGING_ENABLED
#ifdef LOGGING_ENABLED
#  define LOGD(...) libpowertutor::dbgprintf(__VA_ARGS__)
#  define LOGE(...) libpowertutor::dbgprintf(__VA_ARGS__)

namespace libpowertutor {
    void dbgprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
}
#else
#  define LOGD(...)
#  define LOGE(...)
#endif


#endif
