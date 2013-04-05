#include "debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sstream>
#include <string>
#include <iomanip>
using std::ostringstream; using std::string; 
using std::setw; using std::setfill;

#ifdef ANDROID
#define LIBPT_LOGFILE "/sdcard/intnw/libpowertutor.log"
#  ifdef NDK_BUILD
#  include <android/log.h>
#  else
#  include <cutils/logd.h>
#  endif
#endif

pthread_key_t thread_name_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void delete_name_string(void *arg)
{
    char *name_str = (char*)arg;
    delete [] name_str;
}

static void make_key()
{
    (void)pthread_key_create(&thread_name_key, delete_name_string);
    pthread_setspecific(thread_name_key, NULL);
}

char * get_thread_name()
{
    (void) pthread_once(&key_once, make_key);

    char * name_str = (char*)pthread_getspecific(thread_name_key);
    if (!name_str) {
        char *name = new char[12];
        sprintf(name, "%08lx", pthread_self());
        pthread_setspecific(thread_name_key, name);
        name_str = name;
    }

    return name_str;
}

static void vdbgprintf(bool plain, const char *fmt, va_list ap)
{
    ostringstream stream;
    if (!plain) {
        struct timeval now;
        gettimeofday(&now, NULL);
        stream << "[" << now.tv_sec << "." << setw(6) << setfill('0') << now.tv_usec << "]";
        stream << "[" << getpid() << "]";
        stream << "[";
#ifdef CMM_UNIT_TESTING
        stream << "(unit testing)";
#else
        stream << get_thread_name();
#endif
        stream << "] ";
    }

    string fmtstr(stream.str());
    fmtstr += fmt;
    
#ifdef ANDROID
    FILE *out = fopen(LIBPT_LOGFILE, "a");
    if (out) {
        vfprintf(out, fmtstr.c_str(), ap);
        fclose(out);
    } else {
        int e = errno;
        stream.str("");
        stream << "Failed opening libpowertutor log file: "
               << strerror(e) << " ** " << fmtstr;
        fmtstr = stream.str();
        
        __android_log_vprint(ANDROID_LOG_INFO, "libpowertutor", fmtstr.c_str(), ap);
    }
#else
    vfprintf(stderr, fmtstr.c_str(), ap);
#endif
}

void dbgprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdbgprintf(false, fmt, ap);
    va_end(ap);
}
