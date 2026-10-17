#include "prx/libkernel/File/include/HostErrno.hpp"

#include <cerrno>

namespace {

constexpr int GuestEio = 5;
constexpr int GuestEdeadlk = 11;
constexpr int GuestEwouldblock = 35;
constexpr int GuestEloop = 62;
constexpr int GuestEnametoolong = 63;
constexpr int GuestEnotempty = 66;
constexpr int GuestEdquot = 69;
constexpr int GuestEoverflow = 84;

}

namespace File {

int SceErrorFromHostErrno(int error) {
    int guest = error > 0 && error <= 34 ? error : error == GuestEnotempty ? error : GuestEio;
    if (error == EAGAIN) guest = GuestEwouldblock;
#ifdef EDEADLK
    else if (error == EDEADLK) guest = GuestEdeadlk;
#endif
    else if (error == ENAMETOOLONG) guest = GuestEnametoolong;
    else if (error == ENOTEMPTY) guest = GuestEnotempty;
#ifdef ELOOP
    else if (error == ELOOP) guest = GuestEloop;
#endif
#ifdef EDQUOT
    else if (error == EDQUOT) guest = GuestEdquot;
#endif
#ifdef EOVERFLOW
    else if (error == EOVERFLOW) guest = GuestEoverflow;
#endif
    return static_cast<int>(0x80020000u | static_cast<unsigned>(guest));
}

bool IsPathLookupError(const std::error_code& error) {
    return error == std::errc::filename_too_long || error == std::errc::too_many_symbolic_link_levels;
}

}
