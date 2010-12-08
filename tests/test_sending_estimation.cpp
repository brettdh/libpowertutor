#include <cutils/log.h>
#include "../libpowertutor.h"

static int socks[2] = {-1, -1};

static void
do_and_print_result(NetworkType type, size_t datalen, size_t bandwidth)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int energy = estimate_energy_cost(type, datalen, bandwidth);
    LOGD("[%lu.%06lu] %s: %zu bytes, %zu bytes/sec, %d mJ\n",
         net_types[type], datalen, bandwidth, energy);
}

static int
connect_sock(struct in_addr *wifi_addr)
{
    const char *host = "141.212.110.132";
    const int port = 4242;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    //TODO: pick up here.
}

int main()
{
    sleep(2); //wait for first observation
    
    struct in_addr wifi_addr, 3g_addr;
    int rc = get_ip_addr("rmnet0", &3g_addr);
    if (rc == 0) {
        socks[TYPE_MOBILE] = connect_sock(&3g_addr);
    }
    rc = get_ip_addr("tiwlan0", &wifi_addr);
    if (rc == 0) {
        socks[TYPE_WIFI] = connect_sock(&wifi_addr);
    }
    
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
