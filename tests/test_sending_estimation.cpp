#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <cutils/log.h>
#include "../libpowertutor.h"
#include "../utils.h"

static int socks[2] = {-1, -1};
static const char *net_types[2] = {"mobile", "wifi"};

static void
do_and_print_result(NetworkType type, size_t datalen, size_t bandwidth)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int energy = estimate_energy_cost(type, datalen, bandwidth);
    LOGD("[%lu.%06lu] %s: %zu bytes, %zu bytes/sec, %d mJ\n",
         now.tv_sec, now.tv_usec,
         net_types[type], datalen, bandwidth, energy);
    
    // TODO: actually send the data on the right interface'
    // TODO:  so that PowerTutor can observe the energy consumption
}

static int
connect_sock(struct sockaddr *local_addr)
{
    const char *host = "141.212.110.132";
    const int port = 4242;
    
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("socket: %s\n", strerror(errno));
        return -1;
    }
    int on = 1;
    int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &on, sizeof(on));
    if (rc < 0) {
        LOGE("Cannot reuse socket address: %s\n", strerror(errno));
    }
    socklen_t socklen = sizeof(struct sockaddr_in);
    rc = bind(sock, local_addr, socklen);
    if (rc < 0) {
        LOGE("bind: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    inet_aton(host, &remote_addr.sin_addr);
    remote_addr.sin_port = htons(port);
    socklen = sizeof(remote_addr);
    rc = connect(sock, (struct sockaddr *) &remote_addr, socklen);
    if (rc < 0) {
        LOGE("connect: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    return sock;
}

int main()
{
    sleep(2); //wait for first observation
    
    struct sockaddr_in wifi_addr, mobile_addr;
    mobile_addr.sin_family = AF_INET;
    wifi_addr.sin_family = AF_INET;
    int rc = get_ip_addr("rmnet0", &mobile_addr.sin_addr);
    if (rc == 0) {
        socks[TYPE_MOBILE] = connect_sock((struct sockaddr *) &mobile_addr);
    }
    rc = get_ip_addr("tiwlan0", &wifi_addr.sin_addr);
    if (rc == 0) {
        socks[TYPE_WIFI] = connect_sock((struct sockaddr *) &wifi_addr);
    }
    
    // TODO: get the bandwidth estimates from the scout via IPC.
    //  Maybe separate that bit off from libcmm into a separate library.
    do_and_print_result(TYPE_MOBILE, 25, 8192);
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 50, 8192);
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 100, 8192);
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 150, 8192);
    sleep(2);
    do_and_print_result(TYPE_MOBILE, 350, 8192);
    sleep(2);
    do_and_print_result(TYPE_MOBILE, 1000, 8192);
    sleep(1);
    
    return 0;
}
