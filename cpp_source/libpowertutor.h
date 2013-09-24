#ifndef LIBPOWERTUTOR_H_NYF9WXHP
#define LIBPOWERTUTOR_H_NYF9WXHP

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

#include <sys/types.h>

CDECL enum NetworkType {
    TYPE_MOBILE = 0,
    TYPE_WIFI   = 1
};

enum MobileState {
    MOBILE_POWER_STATE_IDLE=0,
    MOBILE_POWER_STATE_FACH,
    MOBILE_POWER_STATE_DCH
};

extern const char *mobile_state_str[];

/* Returns energy in mJ.
 * datalen is the length of data to be sent/received.
 * bandwidth is the estimated bandwidth in the direction of the transfer,
 *   in bytes/second.
 * XXX: this is not currently an application-facing abstraction;
 * XXX:  it's going to be used by Intentional Networking.
 * XXX:  Therefore, it's okay to err on the side of exposing too much detail,
 * XXX:  since I might need a lot of detail to make good decisions,
 * XXX:  and a simple abstraction might not be powerful enough.
 */
CDECL int estimate_energy_cost(NetworkType type, size_t datalen, 
                               size_t bandwidth, size_t rtt_ms);

CDECL int estimate_mobile_energy_cost(size_t datalen, size_t bandwidth, size_t rtt_ms);

// ignore the current power state of the radio and 
//  return the cost considering the average power state.
CDECL int estimate_mobile_energy_cost_average(size_t datalen, size_t avg_bandwidth, size_t avg_rtt_ms);

CDECL int estimate_wifi_energy_cost(size_t datalen, size_t bandwidth, size_t rtt_ms);

// Takes (duration in seconds) and returns an 
//  energy estimate in mJ for pegging the CPU (highest freq, 100% utilization)
//  for that duration.  Assumes the CPU would otherwise be idle.
CDECL int estimate_pegged_cpu_energy(double seconds);

#ifdef __cplusplus
#include <functional>
typedef std::function<int(size_t, size_t, size_t)> EnergyComputer;

// returns a functor that takes (datalen, bandwidth, rtt_ms) and returns
// an energy estimate in mJ.  Binds the current energy state to all invocations
//  of this functor, so that a tight loop can calculate those values once
//  outside the loop body and reuse them in the loop body.
//  This should hopefully save computation time.
EnergyComputer get_energy_computer(NetworkType type);
#endif

// returns estimated energy consumed by network interfaces since last reset, in mJ.
CDECL int energy_consumed_since_reset();

// return the number of bytes sent on a cellular interface since last reset.
CDECL int mobile_bytes_consumed_since_reset();

// returns average power consumption by network interfaces since last reset, in mW.
CDECL int average_power_consumption_since_reset();

CDECL void reset_stats();


struct remote_power_state {
    int mobile_state;
    int mobile_queue_len[2]; // down, up (handset perspecive)
    int wifi_packet_rate;
};

typedef void (*activity_callback_t)(struct remote_power_state);

/* ---------------------------------------------------------------------------
 * This function is used for registering a callback for
 * state changes in the power model.  Applications can use it
 * to be notified when there is activity on the 3G interface.
 * The callback also receives the current power state of the interface.
 * This is only applicable on the handset.
 * ---------------------------------------------------------------------------
*/
/* REQ: callbacks must not block or take a long time. */
CDECL void register_mobile_activity_callback(activity_callback_t callback);


#include <netinet/in.h>
/* This function is for use on the remote (non-handset) side.
 *  It provides a application-agnostic hook to update the power model.
 *  The application is free to exchange the needed data however it wishes.
 *  For example, Intentional Networking will piggyback headers on its messages
 *    encapsulating this data, while the simple test application will
 *    have a dedicated thread and socket for sending this information.
 *
 * TODO: implement this part. vvv
 * This function modifies data that is indexed by the argument - an IP address.
 *  By doing so, one server can maintain separate power models for many mobile
 *  clients.
 */
/* Upgrade the state of the 3G interface and update the last_activity tracker.
 *  This should not be used to indicate state transition due to timeout,
 *   since those timeouts will be detected automatically. This function should
 *   only be used to move the model to a higher state (or maintain its state
 *   and report another activity in that state).
 */
/* TODO: the ip_addr argument will be used later to maintain power models
 * for multiple remote handsets.
 */
CDECL void report_remote_mobile_activity(/* struct in_addr ip_addr, */
                                         struct remote_power_state state);


/* Mocking-related calls for simulation purposes */
CDECL void libpowertutor_init_mocking();
CDECL void update_energy_stats();
CDECL void add_bytes_down(NetworkType type, int bytes);
CDECL void add_bytes_up(NetworkType type, int bytes);
CDECL void add_packets_down(NetworkType type, int packets);
CDECL void add_packets_up(NetworkType type, int packets);

#endif /* end of include guard: LIBPOWERTUTOR_H_NYF9WXHP */
