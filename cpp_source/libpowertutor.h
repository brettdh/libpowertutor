#ifndef LIBPOWERTUTOR_H_NYF9WXHP
#define LIBPOWERTUTOR_H_NYF9WXHP

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

enum NetworkType {
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
int estimate_energy_cost(NetworkType type, size_t datalen, 
                         size_t bandwidth, size_t rtt_ms);

int estimate_mobile_energy_cost(size_t datalen, size_t bandwidth, size_t rtt_ms);

// ignore the current power state of the radio and 
//  return the cost as though it were idle.
int estimate_mobile_energy_cost_from_idle(size_t datalen, size_t bandwidth, size_t rtt_ms);

int estimate_wifi_energy_cost(size_t datalen, size_t bandwidth, size_t rtt_ms);

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
void register_mobile_activity_callback(activity_callback_t callback);


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
void report_remote_mobile_activity(/* struct in_addr ip_addr, */
                                   struct remote_power_state state);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: LIBPOWERTUTOR_H_NYF9WXHP */
