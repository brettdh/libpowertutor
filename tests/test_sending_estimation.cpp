#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <vector>
#include <functional>
using std::vector; using std::min;
#include "../libpowertutor.h"

static const short TEST_PORT = 4242;

#ifdef ANDROID
#define LOG_TAG "test_sending_estimation"
#include <cutils/log.h>
#else
#define LOGD printf
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

static int socks[2] = {-1, -1};
static int bandwidth_up[2] = {0, 0};
static int bandwidth_down[2] = {0, 0};
static int rtt_ms[2] = {0, 0};
static const char *net_types[2] = {"mobile", "wifi"};

#ifdef ANDROID
#define RESULTS_DIR "/sdcard/libpowertutor_testing"
//#define REMOTE_RESULTS_DIR RESULTS_DIR "/remote_results"
#else
#define RESULTS_DIR "/tmp/libpowertutor_testing"
#endif
#define RESULTS_FILENAME_MAX_LEN 75
static FILE *test_output = NULL;

static int setup_results_dir()
{
    if (access(RESULTS_DIR, F_OK) != 0) {
        if (mkdir(RESULTS_DIR, 0777) != 0) {
            LOGE("Cannot make dir %s (%s)\n", RESULTS_DIR, strerror(errno));
            return -1;
        }
    }
/*
#ifndef SERVER_ONLY
    if (access(REMOTE_RESULTS_DIR, F_OK) != 0) {
        if (mkdir(REMOTE_RESULTS_DIR, 0777) != 0) {
            LOGE("Cannot make dir %s (%s)\n", 
                 REMOTE_RESULTS_DIR, strerror(errno));
            return -1;
        }
    }
#endif
*/    
    struct timeval now;
    gettimeofday(&now, NULL);
    char filename[RESULTS_FILENAME_MAX_LEN];
    sprintf(filename, RESULTS_DIR "/%lu.log", now.tv_sec);
    test_output = fopen(filename, "w");
    if (!test_output) {
        LOGE("Failed to open %s (%s)\n", filename, strerror(errno));
        return -1;
    }
    return 0;
}

// not needed anymore; now we send the predictions in the data stream.
//  This way, we only rely on the timestamps on the handset when 
//  analyzing the test results.
/*
#ifdef SERVER_ONLY
#include <sys/sendfile.h>
static int send_test_results(int sock)
{
    LOGD("Sending test results back to handset\n");
    int len = ftell(test_output);
    if (len < 0) {
        LOGE("Error: failed to get file offset: %s\n", strerror(errno));
        return -1;
    }
    rewind(test_output);
    int fd = fileno(test_output);
    if (fd < 0) {
        LOGE("Failed to get file descriptor: %s\n", strerror(errno));
        return -1;
    }
    len = htonl(len);
    int rc = write(sock, &len, sizeof(len));
    if (rc != sizeof(len)) {
        LOGE("Failed to send test result file size: %s\n", strerror(errno));
        return -1;
    }
    len = ntohl(len);
    
    rc = sendfile(sock, fd, NULL, len);
    if (rc != len) {
        LOGE("Failed to send the test results file: %s\n", strerror(errno));
        return -1;
    }
    int ack = 0;
    rc = read(sock, &ack, sizeof(ack));
    if (rc != sizeof(ack)) {
        LOGE("Failed to get ack of test results file: %s\n", 
             strerror(errno));
        return -1;
    }
    LOGD("Remote side received %d bytes successfully.\n", ntohl(ack));
    return 0;
}
#else
// This must be the last thing we do with the socket
static int receive_test_results(int sock)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    char filename[RESULTS_FILENAME_MAX_LEN];
    sprintf(filename, REMOTE_RESULTS_DIR "/%lu.log", now.tv_sec);
    FILE *remote_test_output = fopen(filename, "w");
    if (!remote_test_output) {
        LOGE("Failed to open %s (%s)\n", filename, strerror(errno));
        return -1;
    }
    int fd = fileno(remote_test_output);
    if (fd < 0) {
        LOGE("Failed to get file descriptor: %s\n", strerror(errno));
        fclose(remote_test_output);
        return -1;
    }
    
    const int CHUNK = 4096;
    char data[CHUNK];
    int bytes_read = 0;
    int bytes_expected = 0;
    int rc = read(sock, &bytes_expected, sizeof(bytes_expected));
    if (rc != sizeof(bytes_expected)) {
        LOGE("Failed to read test file size: %s\n", strerror(errno));
        fclose(remote_test_output);
        return -1;
    }
    bytes_expected = ntohl(bytes_expected);
    
    while (bytes_read < bytes_expected) {
        rc = read(sock, data, CHUNK);
        if (rc <= 0) {
            break;
        }
        
        if (write(fd, data, rc) != rc) {
            LOGE("Failed to write %d bytes to file: %s\n", 
                 rc, strerror(errno));
            fclose(remote_test_output);
            return -1;
        }
        bytes_read += rc;
    }
    fclose(remote_test_output);
    if (bytes_read != bytes_expected) {
        LOGE("Expected %d bytes of remote test output; only got %d\n",
             bytes_expected, bytes_read);
        return -1;
    }
    bytes_read = htonl(bytes_read);
    if (write(sock, &bytes_read, sizeof(bytes_read)) != sizeof(bytes_read)) {
        LOGE("Failed to send ack of remote test output: %s\n", 
             strerror(errno));
        return -1;
    }
    return 0;
}
#endif
*/

// datalen must be at least 5 bytes, so I can stuff the prediction and newline
static ssize_t
send_bytes(int sock, size_t datalen, int energy_prediction)
{
    assert(datalen >= (sizeof(int) + 1));
    
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
        if (bytes_sent == 0) {
            int *prediction = (int*) data;
            *prediction = htonl(energy_prediction);
        }
        
        size_t bytes_now = min(CHUNKSIZE, datalen - bytes_sent);
        char saved = data[bytes_now - 1];
        if (bytes_now == (datalen - bytes_sent)) {
            data[bytes_now - 1] = '\x0A';
        }
        int rc = write(sock, data, bytes_now);
        data[bytes_now - 1] = saved;
        if (bytes_sent == 0) {
            memset(data, 'F', sizeof(int));
        }
        
        if (rc != (ssize_t)bytes_now) {
            LOGE("send_bytes: write: %s\n", strerror(errno));
            return -1;
        }
        
        bytes_sent += bytes_now;
    }
    return (ssize_t)bytes_sent;
}

class TestFailureException {};

static void
do_and_print_result(NetworkType type, size_t datalen)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int energy = estimate_energy_cost(type, datalen, 
                                      bandwidth_up[type], rtt_ms[type]);
    LOGD("%lu.%06lu  %s %zu bytes, bw_up %zu bytes/sec up, rtt %d ms, %d mJ\n",
         now.tv_sec, now.tv_usec,
         net_types[type], datalen, bandwidth_up[type], rtt_ms[type], energy);
    fprintf(test_output, "%lu.%06lu  %s %zu bytes, %zu "
            "bytes/sec up, rtt %d ms, %d mJ\n",
            now.tv_sec, now.tv_usec,
            net_types[type], datalen, 
            bandwidth_up[type], rtt_ms[type], energy);

    // actually send the data on the right interface
    //  so that PowerTutor can observe the energy consumption
    ssize_t rc = send_bytes(socks[type], datalen, energy);
    gettimeofday(&now, NULL);
    if (rc != (ssize_t) datalen) {
        LOGE("%lu.%06lu  Failed to send %zu bytes (%s)\n", 
             now.tv_sec, now.tv_usec, datalen, strerror(errno));
        fprintf(test_output, "%lu.%06lu  Failed to send %zu bytes (%s)\n", 
                now.tv_sec, now.tv_usec, datalen, strerror(errno));
        throw TestFailureException();
    } else {
        LOGD("%lu.%06lu  %s done\n",
             now.tv_sec, now.tv_usec, net_types[type]);
        fprintf(test_output, "%lu.%06lu  %s done\n",
                now.tv_sec, now.tv_usec, net_types[type]);
    }

    char ack;
    rc = read(socks[type], &ack, 1);
    gettimeofday(&now, NULL);
    if (rc != 1) {
        LOGE("%lu.%06lu  Failed to recv ack (%s)\n", 
             now.tv_sec, now.tv_usec, strerror(errno));
        throw TestFailureException();
    } else {
        LOGD("%lu.%06lu  %s received ack\n", 
             now.tv_sec, now.tv_usec, net_types[type]);
    }
}

static const char *TEST_FINISHED_MSG = "test finished\x0A";
static void finish_test(int sock)
{
    int rc = write(sock, TEST_FINISHED_MSG, strlen(TEST_FINISHED_MSG));
    if (rc != (int)strlen(TEST_FINISHED_MSG)) {
        LOGE("Failed to send test-finished msg\n");
        throw TestFailureException();
    }
    
    char ack;
    rc = read(sock, &ack, 1);
    if (rc != 1) {
        LOGE("Failed to read ack for test-finished msg\n");
        throw TestFailureException();
    }
}

static void run_sending_tests()
{
    LOGD("Waiting a bit for FACH timeout\n");
    sleep(8);
    
    struct timeval now;
    gettimeofday(&now, NULL);
    LOGD("%lu.%06lu Starting 3G power tests\n", now.tv_sec, now.tv_usec);
    fprintf(test_output, "%lu.%06lu Starting 3G power tests\n",
            now.tv_sec, now.tv_usec);
            
    // expected energy: 410 mW * 6 seconds = 2406 mJ
    do_and_print_result(TYPE_MOBILE, 5); // FACH
    sleep(14); // IDLE
    
    // expected energy: 410 mW * 6 seconds = 2406 mJ
    do_and_print_result(TYPE_MOBILE, 5); // FACH
    sleep(14); // IDLE
    
    // expected energy: 410 mW * 6 seconds = 2406 mJ
    do_and_print_result(TYPE_MOBILE, 5); // still FACH
    sleep(4);
    // expected energy: 410 mW * 4 seconds = 1604 mJ
    do_and_print_result(TYPE_MOBILE, 5); // still FACH
    sleep(4);
    
    // expected energy: 410 mW * 4 seconds = 1604 mJ
    do_and_print_result(TYPE_MOBILE, 30); // still FACH
    sleep(1);
    // expected energy: 410 mW * 1 second = 410 mJ
    do_and_print_result(TYPE_MOBILE, 30); // still FACH
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 30); // DCH now
    do_and_print_result(TYPE_MOBILE, 30);
    sleep(6);
    do_and_print_result(TYPE_MOBILE, 30); // FACH now
    sleep(1);
    do_and_print_result(TYPE_MOBILE, 150); // DCH now
    sleep(14); // IDLE again
    
    // test DCH timeout plus time to send bytes.
    do_and_print_result(TYPE_MOBILE, 40000); // DCH now
    LOGD("Done with 3G estimations; waiting 14 seconds for IDLE\n");
    sleep(14); // hopefully IDLE again
    gettimeofday(&now, NULL);
    LOGD("%lu.%06lu Finished 3G power tests\n", now.tv_sec, now.tv_usec);
    fprintf(test_output, "%lu.%06lu Finished 3G power tests\n",
            now.tv_sec, now.tv_usec);
            
    finish_test(socks[TYPE_MOBILE]);
    sleep(14);
    
    LOGD("Starting wifi power tests\n");
    do_and_print_result(TYPE_WIFI, 25);
    sleep(10);
    do_and_print_result(TYPE_WIFI, 50);
    sleep(10);
    do_and_print_result(TYPE_WIFI, 1000);
    sleep(10);
    do_and_print_result(TYPE_WIFI, 10000);
    sleep(10);
    do_and_print_result(TYPE_WIFI, 100000);
    sleep(10);
    do_and_print_result(TYPE_WIFI, 1000000);
    sleep(10);
    LOGD("Finished wifi power tests\n");
    
    finish_test(socks[TYPE_WIFI]);
}

static void receive_test_data(NetworkType type)
{
    int sock = socks[type];
    
    const size_t chunksize = 1024*1024;
    char *data = new char[chunksize];
    memset(data, 0, chunksize);
    size_t data_recvd = 0;
    
    while (1) {
        // use select here to detect the arrival of bytes,
        //  which should be pretty close to the time that the
        //  wireless interface becomes active.
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        int rc = select(sock + 1, &readfds, NULL, NULL, NULL);
        if (rc < 0) {
            perror("select");
            break;
        }
        struct timeval begin, now;
        gettimeofday(&begin, NULL);
        LOGD("%lu.%06lu  %s started receiving bytes\n", 
             begin.tv_sec, begin.tv_usec, net_types[type]);
        
        size_t total_bytes = 0;
        int energy_prediction = 0;

        while (data_recvd == 0 || data[data_recvd - 1] != '\x0A') {
            data_recvd = 0;
            rc = read(sock, data, chunksize);
            if (rc <= 0) {
                if (rc < 0) {
                    perror("read");
                }
                break;
            }
            if (total_bytes == 0) {
                // first 4 bytes is the energy prediction
                int *pred = (int *) data;
                energy_prediction = ntohl(*pred);
                
                // overwrite the binary data so I won't 
                //  interpret any of the bytes as newlines
                memset(data, 'F', sizeof(int));
            }
            data_recvd += rc;
            total_bytes += rc;
        }
        if (data_recvd == 0) {
            delete [] data;
            throw TestFailureException();
        }
        gettimeofday(&now, NULL);
        LOGD("%lu.%06lu  %s done; %zu bytes, %zu bytes/sec down, "
             "rtt %d ms, %d mJ\n",
             now.tv_sec, now.tv_usec, net_types[type], total_bytes, 
             bandwidth_down[type], rtt_ms[type], energy_prediction);
             
        fprintf(test_output, "%lu.%06lu  %s %zu bytes, "
                "%zu bytes/sec down, rtt %d ms, %d mJ\n"
                "%lu.%06lu  %s done\n",
                begin.tv_sec, begin.tv_usec,
                net_types[type], total_bytes, 
                bandwidth_down[type], rtt_ms[type], energy_prediction,
                now.tv_sec, now.tv_usec, net_types[type]);
        
        // received whole 'line'; send ack
        char ack = 'Q';
        rc = write(sock, &ack, 1);
        if (rc != 1) {
            perror("write");
            delete [] data;
            throw TestFailureException();
        }
        
        if (!strncmp(data, TEST_FINISHED_MSG, strlen(TEST_FINISHED_MSG))) {
            LOGD("Done receiving.\n");
            break;
        }
    }
    delete [] data;
}

struct test_params {
    int type; // TYPE_MOBILE or TYPE_WIFI
    int bandwidth_down; // handset's downstream; remote's upstream
    int bandwidth_up;   // handset's upstream; remote's downstream
    int rtt_ms;
    short handset_sending;
    short handset_receiving;
};

#ifndef SERVER_ONLY
#include "timeops.h"
#include "../utils.h"
#include <libcmm_external_ipc.h>
#include <net_interface.h>

static int
connect_sock(struct sockaddr *local_addr, const char *remote_host = NULL)
{
    struct sockaddr_in *local_inaddr = (struct sockaddr_in *) local_addr;
    const char *default_host = "141.212.110.132";
    
    if (!remote_host) {
        remote_host = default_host;
    }
    
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
    
    val = 1;
    rc = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
                    (char *) &val, sizeof(val));
    if (rc < 0) {
        LOGE("Cannot make socket TCP_NODELAY\n");
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
    inet_aton(remote_host, &remote_addr.sin_addr);
    remote_addr.sin_port = htons(TEST_PORT);
    socklen = sizeof(remote_addr);
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    rc = connect(sock, (struct sockaddr *) &remote_addr, socklen);
    gettimeofday(&end, NULL);
    TIMEDIFF(begin, end, diff);
    LOGD("connect() from %s returned after %lu.%06lu seconds\n",
         inet_ntoa(local_inaddr->sin_addr), diff.tv_sec, diff.tv_usec);
    if (rc < 0) {
        LOGE("Failed to connect to %s:%d (%s)\n", 
             remote_host, TEST_PORT, strerror(errno));
        close(sock);
        return -1;
    }
    return sock;
}

static int send_test_params(NetworkType type, bool sender, bool receiver)
{
    struct test_params params;
    params.type = htons(type);
    params.bandwidth_down = htonl(bandwidth_down[type]); // remote's upstream
    params.bandwidth_up = htonl(bandwidth_up[type]); // remote's downstream
    params.rtt_ms = htonl(rtt_ms[type]);
    params.handset_sending = (sender ? 0xffff : 0);
    params.handset_receiving = (receiver ? 0xffff : 0);
    
    int rc = write(socks[type], &params, sizeof(params));
    if (rc != sizeof(params)) {
        LOGE("Failed to send %s test params to server\n",
             net_types[type]);
        return -1;
    }
    
    char ack;
    rc = read(socks[type], &ack, 1);
    if (rc != 1) {
        LOGE("Failed to read ack of test params\n");
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    int rc, ch;
    const char *remote_host = NULL;
    bool sender = true, receiver = true;
    
    while ((ch = getopt(argc, argv, "c:sr")) != -1) {
        switch (ch) {
            case 'c': {
                remote_host = optarg;
                for (const char *c = remote_host; *c != '\0'; ++c) {
                    if (isalpha(*c)) {
                        LOGE("Error: specify remote host by IP\n");
                        return -1;
                    }
                }
                break;
            }
            case 's': {
                // sender only
                receiver = false;
                break;
            }
            case 'r': {
                // receiver only
                sender = false;
                break;
            }
        }
    }
    if (!sender && !receiver) {
        LOGE("Error: sender-only and receiver-only both specified\n");
        return -1;
    }
    
    if (setup_results_dir() < 0) {
        return -1;
    }
    
    struct sockaddr_in wifi_addr, mobile_addr;
    mobile_addr.sin_family = AF_INET;
    wifi_addr.sin_family = AF_INET;

    vector<struct net_interface> ifaces;
    if (!get_local_interfaces(ifaces)) {
        LOGE("Failed to get network interfaces from scout\n");
        return -1;
    }
    
    rc = get_ip_addr("rmnet0", &mobile_addr.sin_addr);
    rc += get_ip_addr("tiwlan0", &wifi_addr.sin_addr);
    if (rc != 0) {
        return -1;
    }

    socks[TYPE_MOBILE] = connect_sock((struct sockaddr *) &mobile_addr,
                                      remote_host);
    if (socks[TYPE_MOBILE] < 0) {
        return -1;
    }

    socks[TYPE_WIFI] = connect_sock((struct sockaddr *) &wifi_addr,
                                    remote_host);
    if (socks[TYPE_WIFI] < 0) {
        return -1;
    }
    
    for (size_t i = 0; i < ifaces.size(); ++i) {
        if (ifaces[i].ip_addr.s_addr == mobile_addr.sin_addr.s_addr) {
            bandwidth_up[TYPE_MOBILE] = ifaces[i].bandwidth_up;
            bandwidth_down[TYPE_MOBILE] = ifaces[i].bandwidth_down;
            rtt_ms[TYPE_MOBILE] = ifaces[i].RTT;
        } else if (ifaces[i].ip_addr.s_addr == wifi_addr.sin_addr.s_addr) {
            bandwidth_up[TYPE_WIFI] = ifaces[i].bandwidth_up;
            bandwidth_down[TYPE_WIFI] = ifaces[i].bandwidth_down;
            rtt_ms[TYPE_WIFI] = ifaces[i].RTT;
        } else {
            LOGD("Ack!  I don't have an iface with IP %s\n", 
                 inet_ntoa(ifaces[i].ip_addr));
        }
    }
    
    rc = send_test_params(TYPE_MOBILE, sender, receiver);
    rc += send_test_params(TYPE_WIFI, sender, receiver);
    if (rc < 0) {
        return -1;
    }
    
    LOGD("Waiting for first power observations...\n");
    sleep(2);
    try {
        if (sender) {
            run_sending_tests();
        }
        
        char ch = 'Q';
        rc = write(socks[TYPE_WIFI], &ch, 1);
        if (rc == 1) {
            if (receiver) {
                struct timeval now;
                gettimeofday(&now, NULL);
                fprintf(test_output, "%lu.%06lu Starting receiver tests\n",
                        now.tv_sec, now.tv_usec);
                        
                LOGD("Recieving 3G test data\n");
                receive_test_data(TYPE_MOBILE);
                LOGD("Recieving wifi test data\n");
                receive_test_data(TYPE_WIFI);
                
                gettimeofday(&now, NULL);
                fprintf(test_output, "%lu.%06lu Finished receiver tests\n",
                        now.tv_sec, now.tv_usec);
            }
        }
        LOGD("Testing finished.\n");
    } catch (TestFailureException e) {
        LOGE("Testing failed\n");
    }
    
    /*
    if (receive_test_results(socks[TYPE_WIFI]) != 0) {
        LOGE("Failed to receive remote test results\n");
    }
    */
    close(socks[TYPE_MOBILE]);
    close(socks[TYPE_WIFI]);
    
    fclose(test_output);
    
    return 0;
}
#else /* ifndef SERVER_ONLY */
void handle_error(bool fail_condition, const char *str)
{
    if (fail_condition) {
        perror(str);
        exit(-1);
    }
}

static int recv_test_params(int sock, 
                            bool *handset_sending, bool *handset_receiving)
{
    struct test_params params;
    int rc = read(sock, &params, sizeof(params));
    if (rc != sizeof(params)) {
        LOGD("Failed receiving test params from handset\n");
        return -1;
    }
    
    short type = ntohs(params.type);
    if (type != TYPE_MOBILE && type != TYPE_WIFI) {
        LOGD("Error in received test params: unexpected type %d\n", type);
        return -1;
    }
    socks[type] = sock;
    bandwidth_up[type] = ntohl(params.bandwidth_down); // handset's upstream
    bandwidth_down[type] = ntohl(params.bandwidth_up); // handset's downstream
    rtt_ms[type] = ntohl(params.rtt_ms);
    *handset_sending = params.handset_sending;
    *handset_receiving = params.handset_receiving;
    
    char ack = 'Q';
    rc = write(sock, &ack, 1);
    if (rc != 1) {
        LOGE("Failed to ack test params\n");
        return -1;
    }
    
    return 0;
}

int main()
{
    int rc = setup_results_dir();
    if (rc < 0) {
        return -1;
    }
    
    int listener = socket(PF_INET, SOCK_STREAM, 0);
    handle_error(listener < 0, "socket");
    
    int val = 1;
    rc = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &val, sizeof(val));
    if (rc < 0) {
        fprintf(stderr, "Cannot set SO_REUSEADDR (%s)\n", strerror(errno));
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TEST_PORT);
    socklen_t addrlen = sizeof(addr);
    rc = bind(listener, (struct sockaddr *)&addr, addrlen);
    handle_error(rc < 0, "bind");
    rc = listen(listener, 2);
    handle_error(rc < 0, "listen");
    
    LOGD("Listening for connections...\n");
    try {
        while (1) {
            bool handset_sending, handset_receiving;
            while (socks[TYPE_WIFI] == -1 || socks[TYPE_MOBILE] == -1) {
                int sock = accept(listener, NULL, NULL);
                if (sock < 0) {
                    if (errno == EINTR) {
                        throw -1;
                    } else {
                        break;
                    }
                }
                int val = 1;
                rc = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
                                (char *) &val, sizeof(val));
                if (rc < 0) {
                    fprintf(stderr, "Cannot make socket TCP_NODELAY\n");
                }

                if (socks[TYPE_MOBILE] == -1) {
                    rc = recv_test_params(sock, 
                                          &handset_sending, 
                                          &handset_receiving);
                    if (rc != 0) {
                        close(sock);
                        socks[TYPE_MOBILE] = -1;
                    }
                    continue;
                }
                if (socks[TYPE_WIFI] == -1) {
                    rc = recv_test_params(sock,
                                          &handset_sending, 
                                          &handset_receiving);
                    if (rc != 0) {
                        close(sock);
                        socks[TYPE_WIFI] = -1;
                    }
                    continue;
                }
            }
            
            if (socks[TYPE_MOBILE] != -1 && socks[TYPE_WIFI] != -1) {
                try {
                    if (handset_sending) {
                        LOGD("Receiving 3G test data\n");
                        receive_test_data(TYPE_MOBILE);
                        LOGD("Receiving wifi test data\n");
                        receive_test_data(TYPE_WIFI);
                    }
                    
                    char ch;
                    rc = read(socks[TYPE_WIFI], &ch, 1);
                    if (rc == 1) {
                        if (handset_receiving) {
                            run_sending_tests();
                        }
                    }
                } catch (TestFailureException e) {
                    LOGE("Testing failed; closing sockets\n");
                }
            }
                
            if (socks[TYPE_MOBILE] != -1) {
                close(socks[TYPE_MOBILE]);
            }
            if (socks[TYPE_WIFI] != -1) {
                /*
                if (send_test_results(socks[TYPE_WIFI])) {
                    LOGE("Failed to send test results to handset\n");
                }
                */
                close(socks[TYPE_WIFI]);
            }
            LOGD("Done with reciever tests.\n");
            
            socks[TYPE_MOBILE] = -1;
            socks[TYPE_WIFI] = -1;
        }
    } catch (int exiting) {
        // do nothing; about to exit
    }

    return 0;
}
#endif /* !ifndef SERVER_ONLY */
