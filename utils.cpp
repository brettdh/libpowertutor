#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "utils.h"

int 
get_ip_addr(const char *ifname, struct in_addr *ip_addr)
{
    if (strlen(ifname) > IF_NAMESIZE) {
        LOGE("Error: ifname too long (longer than %d)\n", IF_NAMESIZE);
        return -1;
    }

    struct ifreq ifr;
    strcpy(ifr.ifr_name, ifname);
    ifr.ifr_addr.sa_family = AF_INET;

    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOGE("socket: %s\n", strerror(errno));
        return sock;
    }
    int rc = ioctl(sock, SIOCGIFADDR, &ifr);
    if (rc == 0) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&ifr.ifr_addr;
        memcpy(ip_addr, &inaddr->sin_addr, sizeof(ip_addr));
    } else {
        LOGE("getting ip addr: ioctl: %s\n", strerror(errno));
    }

    close(sock);
    return rc;
}
