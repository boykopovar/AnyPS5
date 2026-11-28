#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int guestAfUnix = 1;
constexpr int guestAfInet = 2;
constexpr int guestAfInet6 = 28;

constexpr int guestSockStream = 1;
constexpr int guestSockDgram = 2;
constexpr int guestSockRaw = 3;
constexpr int guestSockRdm = 4;

constexpr int guestSockCloexec = 0x10000000;
constexpr int guestSockNonblock = 0x20000000;

constexpr int posixEfault = 14;
constexpr int posixEagain = 35;
constexpr int posixEprototype = 41;
constexpr int posixEprotonosupport = 43;
constexpr int posixEopnotsupp = 45;
constexpr int posixEafnosupport = 47;

constexpr int getDescriptorFlags = 1;
constexpr int getFlags = 3;
constexpr int nonBlocking = 4;
constexpr int readWrite = 2;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

void AuditInvalidDomainAndProtocol() {
    int pair[2] = {-1, -1};

    constexpr int invalidDomains[] = {
        -100, -1, 0, 3, 4, 10, 16, 32, 100, 2147483647
    };
    for (const int domain : invalidDomains) {
        *__error_nid_postfix() = 0;
        RequireFailure(socketpair_nid_postfix(domain, guestSockStream, 0, pair), posixEafnosupport);
        *__error_nid_postfix() = 0;
        RequireFailure(socketpair_nid_postfix(domain, guestSockDgram, 0, pair), posixEafnosupport);
    }

    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfInet, guestSockStream, 0, pair), posixEopnotsupp);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfInet, guestSockDgram, 0, pair), posixEopnotsupp);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfInet6, guestSockStream, 0, pair), posixEopnotsupp);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfInet6, guestSockDgram, 0, pair), posixEopnotsupp);

    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfInet, 0, 0, pair), posixEprotonosupport);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfInet, guestSockRaw, 0, pair), posixEprototype);

    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, 0, 0, pair), posixEprotonosupport);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockRaw, 0, pair), posixEprototype);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockRdm, 0, pair), posixEprototype);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, 100, 0, pair), posixEprototype);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockStream | 0x01000000, 0, pair), posixEprototype);

    constexpr int invalidProtocols[] = {-1, 1, 6, 17, 50, 100};
    for (const int protocol : invalidProtocols) {
        *__error_nid_postfix() = 0;
        RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockStream, protocol, pair), posixEprotonosupport);
        *__error_nid_postfix() = 0;
        RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockDgram, protocol, pair), posixEprotonosupport);
    }
}

void AuditPointerFaults() {
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockStream, 0, nullptr), posixEfault);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockDgram, 0, nullptr), posixEfault);

    auto* unmappedLow = reinterpret_cast<int*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockStream, 0, unmappedLow), posixEfault);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockDgram, 0, unmappedLow), posixEfault);

    auto* unmappedHigh = reinterpret_cast<int*>(~std::uintptr_t{0xfff});
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockStream, 0, unmappedHigh), posixEfault);
    *__error_nid_postfix() = 0;
    RequireFailure(socketpair_nid_postfix(guestAfUnix, guestSockDgram, 0, unmappedHigh), posixEfault);
}

void AuditStreamSocketPair() {
    int streamFds[2] = {-1, -1};
    *__error_nid_postfix() = 0x5a5a;
    Require(socketpair_nid_postfix(guestAfUnix, guestSockStream, 0, streamFds) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);

    Require(streamFds[0] >= 0);
    Require(streamFds[1] >= 0);
    Require(streamFds[0] != streamFds[1]);

    Require(fcntl_nid_postfix(streamFds[0], getDescriptorFlags) == 0);
    Require(fcntl_nid_postfix(streamFds[1], getDescriptorFlags) == 0);

    const int flags0 = fcntl_nid_postfix(streamFds[0], getFlags);
    const int flags1 = fcntl_nid_postfix(streamFds[1], getFlags);
    Require((flags0 & readWrite) != 0);
    Require((flags0 & nonBlocking) == 0);
    Require((flags1 & readWrite) != 0);
    Require((flags1 & nonBlocking) == 0);

    const char msg1[] = "stream message from 0 to 1";
    Require(send_nid_postfix(streamFds[0], msg1, sizeof(msg1), 0) == sizeof(msg1));
    char recv1[64] = {};
    Require(recv_nid_postfix(streamFds[1], recv1, sizeof(recv1), 0) == sizeof(msg1));
    Require(std::strcmp(recv1, msg1) == 0);

    const char msg2[] = "stream message from 1 to 0";
    Require(send_nid_postfix(streamFds[1], msg2, sizeof(msg2), 0) == sizeof(msg2));
    char recv2[64] = {};
    Require(recv_nid_postfix(streamFds[0], recv2, sizeof(recv2), 0) == sizeof(msg2));
    Require(std::strcmp(recv2, msg2) == 0);

    Require(shutdown_nid_postfix(streamFds[0], 1) == 0);
    char eofBuf[16] = {};
    Require(recv_nid_postfix(streamFds[1], eofBuf, sizeof(eofBuf), 0) == 0);

    Require(close_nid_postfix(streamFds[0]) == 0);
    Require(close_nid_postfix(streamFds[1]) == 0);
}

void AuditDatagramSocketPair() {
    int dgramFds[2] = {-1, -1};
    *__error_nid_postfix() = 0x5a5a;
    Require(socketpair_nid_postfix(guestAfUnix, guestSockDgram, 0, dgramFds) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);

    Require(dgramFds[0] >= 0);
    Require(dgramFds[1] >= 0);
    Require(dgramFds[0] != dgramFds[1]);

    const char d1[] = "datagram-1";
    Require(send_nid_postfix(dgramFds[0], d1, sizeof(d1), 0) == sizeof(d1));
    const char d2[] = "datagram-2";
    Require(send_nid_postfix(dgramFds[0], d2, sizeof(d2), 0) == sizeof(d2));

    char recvD1[64] = {};
    Require(recv_nid_postfix(dgramFds[1], recvD1, sizeof(recvD1), 0) == sizeof(d1));
    Require(std::strcmp(recvD1, d1) == 0);

    char recvD2[64] = {};
    Require(recv_nid_postfix(dgramFds[1], recvD2, sizeof(recvD2), 0) == sizeof(d2));
    Require(std::strcmp(recvD2, d2) == 0);

    Require(close_nid_postfix(dgramFds[0]) == 0);
    Require(close_nid_postfix(dgramFds[1]) == 0);
}

void AuditFlagsInheritanceAndNonblocking() {
    int cloexecFds[2] = {-1, -1};
    Require(socketpair_nid_postfix(guestAfUnix, guestSockStream | guestSockCloexec, 0, cloexecFds) == 0);
    Require(fcntl_nid_postfix(cloexecFds[0], getDescriptorFlags) == 1);
    Require(fcntl_nid_postfix(cloexecFds[1], getDescriptorFlags) == 1);
    Require(close_nid_postfix(cloexecFds[0]) == 0);
    Require(close_nid_postfix(cloexecFds[1]) == 0);

    int nonblockFds[2] = {-1, -1};
    Require(socketpair_nid_postfix(guestAfUnix, guestSockStream | guestSockNonblock, 0, nonblockFds) == 0);
    Require((fcntl_nid_postfix(nonblockFds[0], getFlags) & nonBlocking) != 0);
    Require((fcntl_nid_postfix(nonblockFds[1], getFlags) & nonBlocking) != 0);

    char buf[16] = {};
    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(nonblockFds[0], buf, sizeof(buf), 0) == -1);
    Require(*__error_nid_postfix() == posixEagain);

    Require(close_nid_postfix(nonblockFds[0]) == 0);
    Require(close_nid_postfix(nonblockFds[1]) == 0);

    int bothFds[2] = {-1, -1};
    Require(socketpair_nid_postfix(guestAfUnix, guestSockDgram | guestSockCloexec | guestSockNonblock, 0, bothFds) == 0);
    Require(fcntl_nid_postfix(bothFds[0], getDescriptorFlags) == 1);
    Require(fcntl_nid_postfix(bothFds[1], getDescriptorFlags) == 1);
    Require((fcntl_nid_postfix(bothFds[0], getFlags) & nonBlocking) != 0);
    Require((fcntl_nid_postfix(bothFds[1], getFlags) & nonBlocking) != 0);

    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(bothFds[0], buf, sizeof(buf), 0) == -1);
    Require(*__error_nid_postfix() == posixEagain);

    Require(close_nid_postfix(bothFds[0]) == 0);
    Require(close_nid_postfix(bothFds[1]) == 0);
}

} // namespace

int main() {
    AuditInvalidDomainAndProtocol();
    AuditPointerFaults();
    AuditStreamSocketPair();
    AuditDatagramSocketPair();
    AuditFlagsInheritanceAndNonblocking();
    return 0;
}
