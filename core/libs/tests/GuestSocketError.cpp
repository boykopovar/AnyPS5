#include "prx/libkernel/Socket/include/SocketError.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

struct ErrorCase {
    const char* name;
    int host;
    int guest;
};

#define HostError(prefix, name, guest) {#prefix #name, prefix##name, guest}

#ifdef _WIN32
static constexpr ErrorCase Cases[] = {
    HostError(WSA, EINTR, 4),
    HostError(WSA, EBADF, 9),
    HostError(WSA, EACCES, 13),
    HostError(WSA, EFAULT, 14),
    HostError(WSA, EINVAL, 22),
    HostError(WSA, EMFILE, 24),
    HostError(WSA, EWOULDBLOCK, 35),
    HostError(WSA, EINPROGRESS, 36),
    HostError(WSA, EALREADY, 37),
    HostError(WSA, ENOTSOCK, 38),
    HostError(WSA, EDESTADDRREQ, 39),
    HostError(WSA, EMSGSIZE, 40),
    HostError(WSA, EPROTOTYPE, 41),
    HostError(WSA, ENOPROTOOPT, 42),
    HostError(WSA, EPROTONOSUPPORT, 43),
    HostError(WSA, ESOCKTNOSUPPORT, 44),
    HostError(WSA, EOPNOTSUPP, 45),
    HostError(WSA, EPFNOSUPPORT, 46),
    HostError(WSA, EAFNOSUPPORT, 47),
    HostError(WSA, EADDRINUSE, 48),
    HostError(WSA, EADDRNOTAVAIL, 49),
    HostError(WSA, ENETDOWN, 50),
    HostError(WSA, ENETUNREACH, 51),
    HostError(WSA, ENETRESET, 52),
    HostError(WSA, ECONNABORTED, 53),
    HostError(WSA, ECONNRESET, 54),
    HostError(WSA, ENOBUFS, 55),
    HostError(WSA, EISCONN, 56),
    HostError(WSA, ENOTCONN, 57),
    HostError(WSA, ETOOMANYREFS, 59),
    HostError(WSA, ETIMEDOUT, 60),
    HostError(WSA, ECONNREFUSED, 61),
    HostError(WSA, EHOSTDOWN, 64),
    HostError(WSA, EHOSTUNREACH, 65),
};
static constexpr int UnmappedHost = WSAEDISCON;
static constexpr int ShutdownHost = WSAESHUTDOWN;
#else
static constexpr ErrorCase Cases[] = {
    HostError(, EPERM, 1),
    HostError(, EINTR, 4),
    HostError(, EBADF, 9),
    HostError(, ENOMEM, 12),
    HostError(, EACCES, 13),
    HostError(, EFAULT, 14),
    HostError(, EINVAL, 22),
    HostError(, ENFILE, 23),
    HostError(, EMFILE, 24),
    HostError(, EPIPE, 32),
    HostError(, EAGAIN, 35),
    HostError(, EWOULDBLOCK, 35),
    HostError(, EINPROGRESS, 36),
    HostError(, EALREADY, 37),
    HostError(, ENOTSOCK, 38),
    HostError(, EDESTADDRREQ, 39),
    HostError(, EMSGSIZE, 40),
    HostError(, EPROTOTYPE, 41),
    HostError(, ENOPROTOOPT, 42),
    HostError(, EPROTONOSUPPORT, 43),
    HostError(, ESOCKTNOSUPPORT, 44),
    HostError(, EOPNOTSUPP, 45),
    HostError(, EPFNOSUPPORT, 46),
    HostError(, EAFNOSUPPORT, 47),
    HostError(, EADDRINUSE, 48),
    HostError(, EADDRNOTAVAIL, 49),
    HostError(, ENETDOWN, 50),
    HostError(, ENETUNREACH, 51),
    HostError(, ENETRESET, 52),
    HostError(, ECONNABORTED, 53),
    HostError(, ECONNRESET, 54),
    HostError(, ENOBUFS, 55),
    HostError(, EISCONN, 56),
    HostError(, ENOTCONN, 57),
    HostError(, ESHUTDOWN, 58),
    HostError(, ETOOMANYREFS, 59),
    HostError(, ETIMEDOUT, 60),
    HostError(, ECONNREFUSED, 61),
    HostError(, EHOSTDOWN, 64),
    HostError(, EHOSTUNREACH, 65),
    HostError(, EPROTO, 92),
};
static constexpr int UnmappedHost = ENOENT;
#endif

int main() {
    int failures = 0;
    for (const auto& entry : Cases) {
        const int guest = GuestSockets::GuestErrorFromHost(entry.host);
        if (guest != entry.guest) {
            std::fprintf(stderr, "%s (%d): guest %d, FreeBSD %d\n", entry.name, entry.host, guest, entry.guest);
            ++failures;
        }
    }
    if (GuestSockets::GuestErrorFromHost(UnmappedHost) != 5) {
        std::fprintf(stderr, "a host error with no FreeBSD socket counterpart is not EIO\n");
        ++failures;
    }
    for (const int host : {0, -1}) {
        if (GuestSockets::GuestErrorFromHost(host) != 5) {
            std::fprintf(stderr, "host error %d is not EIO\n", host);
            ++failures;
        }
    }
#ifdef _WIN32
    if (GuestSockets::GuestErrorFromHost(ShutdownHost) != 32) {
        std::fprintf(stderr, "WSAESHUTDOWN is not EPIPE\n");
        ++failures;
    }
#endif
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
