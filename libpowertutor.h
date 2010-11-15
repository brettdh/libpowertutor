#ifndef LIBPOWERTUTOR_H_NYF9WXHP
#define LIBPOWERTUTOR_H_NYF9WXHP

enum NetworkType {
    TYPE_MOBILE = 0,
    TYPE_WIFI   = 1
};

int estimate_power_cost(NetworkType type, size_t datalen);

#endif /* end of include guard: LIBPOWERTUTOR_H_NYF9WXHP */
