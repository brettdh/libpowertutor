#ifndef libpowertutor_debug_h_incl
#define libpowertutor_debug_h_incl


//#define SIMULATION_LOG
#ifdef SIMULATION_LOG
#  define LOGD printf
#  define LOGE(...) fprintf(stderr, __VA_ARGS__)
#else
#  define LOGD(...) libpowertutor::dbgprintf(__VA_ARGS__)
#  define LOGE(...) libpowertutor::dbgprintf(__VA_ARGS__)

namespace libpowertutor {
    void dbgprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
}
#endif


#endif
