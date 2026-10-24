#include "prx/libc/include/HostErrno.hpp"

#include <cerrno>

namespace {

constexpr int GuestEio = 5;
constexpr int GuestEdeadlk = 11;
constexpr int GuestEagain = 35;
constexpr int GuestEloop = 62;
constexpr int GuestEnametoolong = 63;
constexpr int GuestEnotempty = 66;
constexpr int GuestEdquot = 69;
constexpr int GuestEoverflow = 84;

}

extern "C" int GuestErrnoFromHost_nid_no_patch(int error) {
    if (error == EAGAIN) return GuestEagain;
#ifdef EDEADLK
    if (error == EDEADLK) return GuestEdeadlk;
#endif
    if (error == ENAMETOOLONG) return GuestEnametoolong;
    if (error == ENOTEMPTY) return GuestEnotempty;
#ifdef ELOOP
    if (error == ELOOP) return GuestEloop;
#endif
#ifdef EDQUOT
    if (error == EDQUOT) return GuestEdquot;
#endif
#ifdef EOVERFLOW
    if (error == EOVERFLOW) return GuestEoverflow;
#endif
    return error > 0 && error <= 34 ? error : GuestEio;
}
