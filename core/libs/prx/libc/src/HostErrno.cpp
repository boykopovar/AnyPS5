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

struct HostErrnoMapping {
    int host;
    int guest;
};

constexpr HostErrnoMapping HostErrnoMap[] = {
    {EAGAIN, GuestEagain},
#ifdef EDEADLK
    {EDEADLK, GuestEdeadlk},
#endif
    {ENAMETOOLONG, GuestEnametoolong},
    {ENOTEMPTY, GuestEnotempty},
#ifdef ELOOP
    {ELOOP, GuestEloop},
#endif
#ifdef EDQUOT
    {EDQUOT, GuestEdquot},
#endif
#ifdef EOVERFLOW
    {EOVERFLOW, GuestEoverflow},
#endif
#ifdef ETXTBSY
    {ETXTBSY, 26},
#endif
#ifdef EWOULDBLOCK
    {EWOULDBLOCK, 35},
#endif
#ifdef EINPROGRESS
    {EINPROGRESS, 36},
#endif
#ifdef EALREADY
    {EALREADY, 37},
#endif
#ifdef ENOTSOCK
    {ENOTSOCK, 38},
#endif
#ifdef EDESTADDRREQ
    {EDESTADDRREQ, 39},
#endif
#ifdef EMSGSIZE
    {EMSGSIZE, 40},
#endif
#ifdef EPROTOTYPE
    {EPROTOTYPE, 41},
#endif
#ifdef ENOPROTOOPT
    {ENOPROTOOPT, 42},
#endif
#ifdef EPROTONOSUPPORT
    {EPROTONOSUPPORT, 43},
#endif
#ifdef ESOCKTNOSUPPORT
    {ESOCKTNOSUPPORT, 44},
#endif
#ifdef EOPNOTSUPP
    {EOPNOTSUPP, 45},
#endif
#ifdef ENOTSUP
    {ENOTSUP, 45},
#endif
#ifdef EPFNOSUPPORT
    {EPFNOSUPPORT, 46},
#endif
#ifdef EAFNOSUPPORT
    {EAFNOSUPPORT, 47},
#endif
#ifdef EADDRINUSE
    {EADDRINUSE, 48},
#endif
#ifdef EADDRNOTAVAIL
    {EADDRNOTAVAIL, 49},
#endif
#ifdef ENETDOWN
    {ENETDOWN, 50},
#endif
#ifdef ENETUNREACH
    {ENETUNREACH, 51},
#endif
#ifdef ENETRESET
    {ENETRESET, 52},
#endif
#ifdef ECONNABORTED
    {ECONNABORTED, 53},
#endif
#ifdef ECONNRESET
    {ECONNRESET, 54},
#endif
#ifdef ENOBUFS
    {ENOBUFS, 55},
#endif
#ifdef EISCONN
    {EISCONN, 56},
#endif
#ifdef ENOTCONN
    {ENOTCONN, 57},
#endif
#ifdef ESHUTDOWN
    {ESHUTDOWN, 58},
#endif
#ifdef ETOOMANYREFS
    {ETOOMANYREFS, 59},
#endif
#ifdef ETIMEDOUT
    {ETIMEDOUT, 60},
#endif
#ifdef ECONNREFUSED
    {ECONNREFUSED, 61},
#endif
#ifdef EHOSTDOWN
    {EHOSTDOWN, 64},
#endif
#ifdef EHOSTUNREACH
    {EHOSTUNREACH, 65},
#endif
#ifdef ESTALE
    {ESTALE, 70},
#endif
#ifdef ENOLCK
    {ENOLCK, 77},
#endif
#ifdef ENOSYS
    {ENOSYS, 78},
#endif
#ifdef EIDRM
    {EIDRM, 82},
#endif
#ifdef ENOMSG
    {ENOMSG, 83},
#endif
#ifdef ECANCELED
    {ECANCELED, 85},
#endif
#ifdef EILSEQ
    {EILSEQ, 86},
#endif
#ifdef EBADMSG
    {EBADMSG, 89},
#endif
#ifdef EMULTIHOP
    {EMULTIHOP, 90},
#endif
#ifdef ENOLINK
    {ENOLINK, 91},
#endif
#ifdef EPROTO
    {EPROTO, 92},
#endif
#ifdef ENOTRECOVERABLE
    {ENOTRECOVERABLE, 95},
#endif
#ifdef EOWNERDEAD
    {EOWNERDEAD, 96},
#endif
};

}

extern "C" int GuestErrnoFromHost_nid_no_patch(int error) {
    for (const auto& entry : HostErrnoMap) {
        if (error == entry.host) {
            return entry.guest;
        }
    }
    return error > 0 && error <= 34 ? error : GuestEio;
}
