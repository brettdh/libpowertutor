#ifndef LIBPOWERTUTOR_H_NYF9WXHP
#define LIBPOWERTUTOR_H_NYF9WXHP

enum NetworkType {
    TYPE_MOBILE = 0,
    TYPE_WIFI   = 1
};

// Returns energy in mJ.
// sock is a connected TCP socket, used to gather the TCP params needed 
//  to make the power calculation.
// XXX: this is not currently an application-facing abstraction;
// XXX:  it's going to be used by Intentional Networking.
// XXX:  Therefore, it's okay to err on the side of exposing too much detail,
// XXX:  since I might need a lot of detail to make good decisions,
// XXX:  and a simple abstraction might not be powerful enough.
int estimate_energy_cost(NetworkType type, int sock, size_t datalen);

#endif /* end of include guard: LIBPOWERTUTOR_H_NYF9WXHP */
