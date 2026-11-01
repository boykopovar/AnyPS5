#pragma once
#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#endif

namespace GuestSockets {

constexpr int GuestErrorIo = 5;

constexpr int GuestErrorFromHost(int hostError) {
    switch (hostError) {
#ifdef _WIN32
        case WSAEINTR: return 4;
        case WSAEBADF: return 9;
        case WSAEACCES: return 13;
        case WSAEFAULT: return 14;
        case WSAEINVAL: return 22;
        case WSAEMFILE: return 24;
        case WSAEWOULDBLOCK: return 35;
        case WSAEINPROGRESS: return 36;
        case WSAEALREADY: return 37;
        case WSAENOTSOCK: return 38;
        case WSAEDESTADDRREQ: return 39;
        case WSAEMSGSIZE: return 40;
        case WSAEPROTOTYPE: return 41;
        case WSAENOPROTOOPT: return 42;
        case WSAEPROTONOSUPPORT: return 43;
        case WSAESOCKTNOSUPPORT: return 44;
        case WSAEOPNOTSUPP: return 45;
        case WSAEPFNOSUPPORT: return 46;
        case WSAEAFNOSUPPORT: return 47;
        case WSAEADDRINUSE: return 48;
        case WSAEADDRNOTAVAIL: return 49;
        case WSAENETDOWN: return 50;
        case WSAENETUNREACH: return 51;
        case WSAENETRESET: return 52;
        case WSAECONNABORTED: return 53;
        case WSAECONNRESET: return 54;
        case WSAENOBUFS: return 55;
        case WSAEISCONN: return 56;
        case WSAENOTCONN: return 57;
        case WSAESHUTDOWN: return 32;
        case WSAETOOMANYREFS: return 59;
        case WSAETIMEDOUT: return 60;
        case WSAECONNREFUSED: return 61;
        case WSAEHOSTDOWN: return 64;
        case WSAEHOSTUNREACH: return 65;
#else
        case EPERM: return 1;
        case EINTR: return 4;
        case EBADF: return 9;
        case ENOMEM: return 12;
        case EACCES: return 13;
        case EFAULT: return 14;
        case EINVAL: return 22;
        case ENFILE: return 23;
        case EMFILE: return 24;
        case EPIPE: return 32;
        case EAGAIN: return 35;
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK: return 35;
#endif
        case EINPROGRESS: return 36;
        case EALREADY: return 37;
        case ENOTSOCK: return 38;
        case EDESTADDRREQ: return 39;
        case EMSGSIZE: return 40;
        case EPROTOTYPE: return 41;
        case ENOPROTOOPT: return 42;
        case EPROTONOSUPPORT: return 43;
        case ESOCKTNOSUPPORT: return 44;
        case EOPNOTSUPP: return 45;
        case EPFNOSUPPORT: return 46;
        case EAFNOSUPPORT: return 47;
        case EADDRINUSE: return 48;
        case EADDRNOTAVAIL: return 49;
        case ENETDOWN: return 50;
        case ENETUNREACH: return 51;
        case ENETRESET: return 52;
        case ECONNABORTED: return 53;
        case ECONNRESET: return 54;
        case ENOBUFS: return 55;
        case EISCONN: return 56;
        case ENOTCONN: return 57;
        case ESHUTDOWN: return 58;
        case ETOOMANYREFS: return 59;
        case ETIMEDOUT: return 60;
        case ECONNREFUSED: return 61;
        case EHOSTDOWN: return 64;
        case EHOSTUNREACH: return 65;
        case EPROTO: return 92;
#endif
        default: return GuestErrorIo;
    }
}

}
