#include "../libpowertutor.h"
#include "../timeops.h"

static void print_time(const char *name, int total, int num_calls, 
                       struct timeval duration)
{
    fprintf(stderr, "  %s: total %d, num_calls %d, duration %lu.%06lu\n",
            name, total, num_calls, duration.tv_sec, duration.tv_usec);
}

int main()
{
    size_t datalen = 1500, bandwidth = 8000, rtt_ms = 200;
    const char *type_strs[] = {"3G", "wifi"};
    for (NetworkType type : {TYPE_MOBILE, TYPE_WIFI}) {
        fprintf(stderr, "type: %s\n", type_strs[type]);
        int cost = estimate_energy_cost(type, datalen, bandwidth, rtt_ms);
        auto estimator = get_energy_computer(type);
        int functor_cost = estimator(datalen, bandwidth, rtt_ms);
        fprintf(stderr, "  cost: %d mJ    functor-cost: %d mJ\n",
                cost, functor_cost);

        const int NUM_CALLS = 300;
        struct timeval begin, end, normal_diff, functor_diff;
        int total = 0;
        TIME(begin);
        for (int i = 0; i < NUM_CALLS; ++i) {
            total += estimate_energy_cost(type, datalen, bandwidth, rtt_ms);
        }
        TIME(end);
        TIMEDIFF(begin, end, normal_diff);
        print_time("normal", total, NUM_CALLS, normal_diff);

        total = 0;
        TIME(begin);
        estimator = get_energy_computer(type);
        for (int i = 0; i < NUM_CALLS; ++i) {
            total += estimator(datalen, bandwidth, rtt_ms);
        }
        TIME(end);
        TIMEDIFF(begin, end, functor_diff);
        print_time("functor", total, NUM_CALLS, functor_diff);
    }
    return 0;
}
