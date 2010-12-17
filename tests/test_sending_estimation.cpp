#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <vector>
#include <functional>
#include "timeops.h"
using std::vector; using std::min;

#define LOG_TAG "test_sending_estimation"
#include <cutils/log.h>
#include "../libpowertutor.h"
#include "../utils.h"
#include <libcmm_external_ipc.h>
#include <net_interface.h>

static int socks[2] = {-1, -1};
static int bandwidth_up[2] = {0, 0};
static const char *net_types[2] = {"mobile", "wifi"};

#ifdef ANDROID
#define RESULTS_DIR "/sdcard/libpowertutor_testing"
#else
#define RESULTS_DIR "/tmp/libpowertutor_testing"
#endif
#define RESULTS_FILENAME_MAX_LEN 60
static FILE *out = NULL;

static ssize_t
send_bytes(int sock, size_t datalen)
{
    //const size_t CHUNKSIZE = 4096;
    const size_t CHUNKSIZE = 4096;
    //char data[CHUNKSIZE];
    static char *data = new char[CHUNKSIZE];
    static bool init = false;
    if (!init) {
        memset(data, 'F', CHUNKSIZE);
        init = true;
    }
    
    size_t bytes_sent = 0;
    while (bytes_sent < datalen) {
        ssize_t bytes_now = min(CHUNKSIZE, datalen - bytes_sent);
        int rc = write(sock, data, bytes_now);
        if (rc != bytes_now) {
            LOGE("send_bytes: write: %s\n", strerror(errno));
            return -1;
        }
        bytes_sent += bytes_now;
    }
    return (ssize_t)bytes_sent;
}

static void
do_and_print_result(NetworkType type, size_t datalen)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int energy = estimate_energy_cost(type, datalen, bandwidth_up[type]);
    LOGD("%lu.%06lu  %s %zu bytes, %zu bytes/sec, %d mJ\n",
         now.tv_sec, now.tv_usec,
         net_types[type], datalen, bandwidth_up[type], energy);
    fprintf(out, "%lu.%06lu  %s %zu bytes, %zu bytes/sec, %d mJ\n",
            now.tv_sec, now.tv_usec,
            net_types[type], datalen, bandwidth_up[type], energy);

    // actually send the data on the right interface'
    //  so that PowerTutor can observe the energy consumption
    ssize_t rc = send_bytes(socks[type], datalen);
    gettimeofday(&now, NULL);
    if (rc != (ssize_t) datalen) {
        LOGE("%lu.%06lu  Failed to send %zu bytes (%s)\n", 
             now.tv_sec, now.tv_usec, datalen, strerror(errno));
        fprintf(out, "%lu.%06lu  Failed to send %zu bytes (%s)\n", 
                now.tv_sec, now.tv_usec, datalen, strerror(errno));
    } else {
        LOGD("%lu.%06lu  %s done\n",
             now.tv_sec, now.tv_usec, net_types[type]);
        fprintf(out, "%lu.%06lu  %s done\n",
                now.tv_sec, now.tv_usec, net_types[type]);
    }
}

static int
connect_sock(struct sockaddr *local_addr)
{
    struct sockaddr_in *local_inaddr = (struct sockaddr_in *) local_addr;
    const char *host = "141.212.110.132";
    const short port = 4242;
    
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("Failed to create socket (%s)\n", strerror(errno));
        return -1;
    }
    int val = 1;
    int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                        (char *) &val, sizeof(val));
    if (rc < 0) {
        LOGE("Cannot set SO_REUSEADDR (%s)\n", strerror(errno));
    }
    
    val = 0;
    rc = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&val, sizeof(val));
    if (rc < 0) {
        LOGE("Cannot set zero socket buffer (%s)\n", strerror(errno));
        close(sock);
        return -1;
    }
    
    socklen_t socklen = sizeof(struct sockaddr_in);
    rc = bind(sock, local_addr, socklen);
    if (rc < 0) {
        LOGE("Failed to bind socket to %s (%s)\n", 
             inet_ntoa(local_inaddr->sin_addr), strerror(errno));
        close(sock);
        return -1;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    inet_aton(host, &remote_addr.sin_addr);
    remote_addr.sin_port = htons(port);
    socklen = sizeof(remote_addr);
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    rc = connect(sock, (struct sockaddr *) &remote_addr, socklen);
    gettimeofday(&end, NULL);
    TIMEDIFF(begin, end, diff);
    LOGD("connect() from %s returned after %lu.%06lu seconds\n",
         inet_ntoa(local_inaddr->sin_addr), diff.tv_sec, diff.tv_usec);
    if (rc < 0) {
        LOGE("Failed to connect to %s:%d (%s)\n", host, port, strerror(errno));
        close(sock);
        return -1;
    }
    return sock;
}

int main()
{
    if (access(RESULTS_DIR, F_OK) != 0) {
        if (mkdir(RESULTS_DIR, 0777) != 0) {
            LOGE("Cannot make dir %s (%s)\n", RESULTS_DIR, strerror(errno));
            return -1;
        }
    }
    
    struct timeval now;
    gettimeofday(&now, NULL);
    char filename[RESULTS_FILENAME_MAX_LEN];
    sprintf(filename, RESULTS_DIR "/%lu.log", now.tv_sec);
    out = fopen(filename, "w");
    if (!out) {
        LOGE("Failed to open %s (%s)\n", filename, strerror(errno));
        return -1;
    }
    vector<struct net_interface> ifaces;
    if (!get_local_interfaces(ifaces)) {
        LOGE("Failed to get network interfaces from scout\n");
        return -1;
    }
    
    LOGD("Waiting for first power observations...\n");
    sleep(2);
    
    struct sockaddr_in wifi_addr, mobile_addr;
    mobile_addr.sin_family = AF_INET;
    wifi_addr.sin_family = AF_INET;
    int rc = get_ip_addr("rmnet0", &mobile_addr.sin_addr);
    if (rc == 0) {
        socks[TYPE_MOBILE] = connect_sock((struct sockaddr *) &mobile_addr);
        if (socks[TYPE_MOBILE] < 0) {
            return -1;
        }
    } else {
        return -1;
    }
    
    rc = get_ip_addr("tiwlan0", &wifi_addr.sin_addr);
    if (rc == 0) {
        socks[TYPE_WIFI] = connect_sock((struct sockaddr *) &wifi_addr);
        if (socks[TYPE_WIFI] < 0) {
            return -1;
        }
    } else {
        return -1;
    }
    
    for (size_t i = 0; i < ifaces.size(); ++i) {
        if (ifaces[i].ip_addr.s_addr == mobile_addr.sin_addr.s_addr) {
            bandwidth_up[TYPE_MOBILE] = ifaces[i].bandwidth_up;
        } else if (ifaces[i].ip_addr.s_addr == wifi_addr.sin_addr.s_addr) {
            bandwidth_up[TYPE_WIFI] = ifaces[i].bandwidth_up;
        } else {
            LOGD("Ack!  I don't have an iface with IP %s\n", 
                 inet_ntoa(ifaces[i].ip_addr));
        }
    }
    
    LOGD("Waiting a bit for FACH timeout\n");
    sleep(8);
    
    gettimeofday(&now, NULL);
    LOGD("%lu.%06lu Starting 3G power tests\n", now.tv_sec, now.tv_usec);
    fprintf(out, "%lu.%06lu Starting 3G power tests\n",
            now.tv_sec, now.tv_usec);
    do_and_print_result(TYPE_MOBILE, 5); // still FACH
    sleep(4);
    do_and_print_result(TYPE_MOBILE, 5); // still FACH
    sleep(4);
    
    do_and_print_result(TYPE_MOBILE, 30); // still FACH
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 30); // still FACH
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 30); // DCH now
    do_and_print_result(TYPE_MOBILE, 30);
    sleep(6);
    do_and_print_result(TYPE_MOBILE, 30); // FACH now
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 150); // DCH now
    sleep(12); // IDLE again
    
    // test DCH timeout plus time to send bytes.
    do_and_print_result(TYPE_MOBILE, 40000); // DCH now
    LOGD("Done with 3G estimations; waiting 12 seconds for IDLE\n");
    sleep(12); // hopefully IDLE again
    gettimeofday(&now, NULL);
    LOGD("%lu.%06lu Finished 3G power tests\n", now.tv_sec, now.tv_usec);
    fprintf(out, "%lu.%06lu Finished 3G power tests\n",
            now.tv_sec, now.tv_usec);
    
    LOGD("Starting wifi power tests\n");
    do_and_print_result(TYPE_WIFI, 25);
    sleep(1);
    do_and_print_result(TYPE_WIFI, 50);
    sleep(1);
    do_and_print_result(TYPE_WIFI, 100);
    sleep(1);
    do_and_print_result(TYPE_WIFI, 150);
    sleep(2);
    do_and_print_result(TYPE_WIFI, 350);
    sleep(2);
    do_and_print_result(TYPE_WIFI, 1000);
    sleep(1);
    do_and_print_result(TYPE_WIFI, 10000);
    sleep(1);
    do_and_print_result(TYPE_WIFI, 100000);
    sleep(1);
    do_and_print_result(TYPE_WIFI, 1000000);
    
    return 0;
}
