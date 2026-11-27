#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEprototype = 41;
constexpr int PosixEprotonosupport = 43;
constexpr int PosixEopnotsupp = 45;
constexpr int PosixEafnosupport = 47;

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;
constexpr int SockRaw = 3;
constexpr int SockRdm = 4;
constexpr int SockSeqpacket = 5;

constexpr int GuestCloseOnExec = 0x10000000;
constexpr int GuestNonBlocking = 0x20000000;

constexpr int FcntlGetFd = 1;
constexpr int FcntlGetFl = 3;
constexpr int FcntlFdCloexec = 1;
constexpr int FcntlONonBlock = 4;
constexpr int FcntlORdWr = 2;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int GetErrno() {
    return *__error_nid_postfix();
}

void TestSocketFamilyBounds() {
    for (const int family : {AfInet, AfInet6}) {
        const int streamFd = socket_nid_postfix(family, SockStream, 0);
        Require(streamFd >= 0);
        Require(close_nid_postfix(streamFd) == 0);

        const int dgramFd = socket_nid_postfix(family, SockDgram, 0);
        Require(dgramFd >= 0);
        Require(close_nid_postfix(dgramFd) == 0);
    }

    const int invalidFamilies[] = {-100, -1, 0, AfUnix, 3, 4, 10, 20, 27, 29, 99, 1000, 65535};
    for (const int family : invalidFamilies) {
        Require(socket_nid_postfix(family, SockStream, 0) == -1 && GetErrno() == PosixEafnosupport);
        Require(socket_nid_postfix(family, SockDgram, 0) == -1 && GetErrno() == PosixEafnosupport);
    }
}

void TestSocketTypeAndProtocolBounds() {
    for (const int family : {AfInet, AfInet6}) {
        for (const int proto : {-1, 0, 6, 17, 99}) {
            Require(socket_nid_postfix(family, 0, proto) == -1 && GetErrno() == PosixEprotonosupport);
        }

        const int validStreamZero = socket_nid_postfix(family, SockStream, 0);
        Require(validStreamZero >= 0);
        Require(close_nid_postfix(validStreamZero) == 0);

        const int validStreamTcp = socket_nid_postfix(family, SockStream, 6);
        Require(validStreamTcp >= 0);
        Require(close_nid_postfix(validStreamTcp) == 0);

        const int badStreamProtocols[] = {-50, -1, 1, 2, 17, 99, 255};
        for (const int proto : badStreamProtocols) {
            Require(socket_nid_postfix(family, SockStream, proto) == -1 && GetErrno() == PosixEprotonosupport);
        }

        const int validDgramZero = socket_nid_postfix(family, SockDgram, 0);
        Require(validDgramZero >= 0);
        Require(close_nid_postfix(validDgramZero) == 0);

        const int validDgramUdp = socket_nid_postfix(family, SockDgram, 17);
        Require(validDgramUdp >= 0);
        Require(close_nid_postfix(validDgramUdp) == 0);

        const int badDgramProtocols[] = {-50, -1, 1, 2, 6, 99, 255};
        for (const int proto : badDgramProtocols) {
            Require(socket_nid_postfix(family, SockDgram, proto) == -1 && GetErrno() == PosixEprotonosupport);
        }

        for (const int proto : {-1, 0, 1, 6, 17}) {
            Require(socket_nid_postfix(family, SockRaw, proto) == -1 && GetErrno() == PosixEprotonosupport);
        }

        Require(socket_nid_postfix(family, SockRdm, 0) == -1 && GetErrno() == PosixEprototype);
        Require(socket_nid_postfix(family, SockRdm, 6) == -1 && GetErrno() == PosixEprotonosupport);
        Require(socket_nid_postfix(family, SockRdm, 17) == -1 && GetErrno() == PosixEprotonosupport);

        Require(socket_nid_postfix(family, SockSeqpacket, 0) == -1 && GetErrno() == PosixEprototype);
        Require(socket_nid_postfix(family, SockSeqpacket, 6) == -1 && GetErrno() == PosixEprotonosupport);
        Require(socket_nid_postfix(family, SockSeqpacket, 17) == -1 && GetErrno() == PosixEprotonosupport);

        const int unsupportedTypes[] = {-5, -1, 6, 7, 10, 99, 1000};
        for (const int type : unsupportedTypes) {
            Require(socket_nid_postfix(family, type, 0) == -1 && GetErrno() == PosixEprototype);
            Require(socket_nid_postfix(family, type, 6) == -1 && GetErrno() == PosixEprotonosupport);
            Require(socket_nid_postfix(family, type, 17) == -1 && GetErrno() == PosixEprotonosupport);
        }
    }
}

void TestSocketCreationFlags() {
    for (const int family : {AfInet, AfInet6}) {
        for (const int type : {SockStream, SockDgram}) {
            {
                const int fd = socket_nid_postfix(family, type, 0);
                Require(fd >= 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFd) & FcntlFdCloexec) == 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlONonBlock) == 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlORdWr) == FcntlORdWr);
                Require(close_nid_postfix(fd) == 0);
            }
            {
                const int fd = socket_nid_postfix(family, type | GuestCloseOnExec, 0);
                Require(fd >= 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFd) & FcntlFdCloexec) == FcntlFdCloexec);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlONonBlock) == 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlORdWr) == FcntlORdWr);
                Require(close_nid_postfix(fd) == 0);
            }
            {
                const int fd = socket_nid_postfix(family, type | GuestNonBlocking, 0);
                Require(fd >= 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFd) & FcntlFdCloexec) == 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlONonBlock) == FcntlONonBlock);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlORdWr) == FcntlORdWr);
                Require(close_nid_postfix(fd) == 0);
            }
            {
                const int fd = socket_nid_postfix(family, type | GuestCloseOnExec | GuestNonBlocking, 0);
                Require(fd >= 0);
                Require((fcntl_nid_postfix(fd, FcntlGetFd) & FcntlFdCloexec) == FcntlFdCloexec);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlONonBlock) == FcntlONonBlock);
                Require((fcntl_nid_postfix(fd, FcntlGetFl) & FcntlORdWr) == FcntlORdWr);
                Require(close_nid_postfix(fd) == 0);
            }
        }
    }
}

void TestSysSocketex() {
    const std::string longName(512, 'a');
    const char* const testNames[] = {
        nullptr,
        "",
        "socket_kernel_probe",
        "system_networking_socket_0",
        longName.c_str()
    };

    for (const char* name : testNames) {
        for (const int family : {AfInet, AfInet6}) {
            const int streamFd = __sys_socketex_nid_postfix(name, family, SockStream, 0);
            Require(streamFd >= 0);
            Require((fcntl_nid_postfix(streamFd, FcntlGetFd) & FcntlFdCloexec) == 0);
            Require((fcntl_nid_postfix(streamFd, FcntlGetFl) & FcntlONonBlock) == 0);
            Require(close_nid_postfix(streamFd) == 0);

            const int dgramFd = __sys_socketex_nid_postfix(name, family, SockDgram | GuestCloseOnExec | GuestNonBlocking, 0);
            Require(dgramFd >= 0);
            Require((fcntl_nid_postfix(dgramFd, FcntlGetFd) & FcntlFdCloexec) == FcntlFdCloexec);
            Require((fcntl_nid_postfix(dgramFd, FcntlGetFl) & FcntlONonBlock) == FcntlONonBlock);
            Require(close_nid_postfix(dgramFd) == 0);

            Require(__sys_socketex_nid_postfix(name, family, SockStream, 17) == -1 && GetErrno() == PosixEprotonosupport);
            Require(__sys_socketex_nid_postfix(name, family, SockDgram, 6) == -1 && GetErrno() == PosixEprotonosupport);
            Require(__sys_socketex_nid_postfix(name, family, 0, 0) == -1 && GetErrno() == PosixEprotonosupport);
            Require(__sys_socketex_nid_postfix(name, family, SockRaw, 0) == -1 && GetErrno() == PosixEprotonosupport);
            Require(__sys_socketex_nid_postfix(name, family, SockRdm, 0) == -1 && GetErrno() == PosixEprototype);
        }

        Require(__sys_socketex_nid_postfix(name, AfUnix, SockStream, 0) == -1 && GetErrno() == PosixEafnosupport);
        Require(__sys_socketex_nid_postfix(name, 0, SockStream, 0) == -1 && GetErrno() == PosixEafnosupport);
        Require(__sys_socketex_nid_postfix(name, -1, SockStream, 0) == -1 && GetErrno() == PosixEafnosupport);
    }
}

void TestSocketpairBounds() {
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, nullptr) == -1 && GetErrno() == PosixEfault);
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, reinterpret_cast<int*>(0x10)) == -1 && GetErrno() == PosixEfault);

    int fds[2]{-1, -1};

    Require(socketpair_nid_postfix(AfInet, SockStream, 0, fds) == -1 && GetErrno() == PosixEopnotsupp);
    Require(socketpair_nid_postfix(AfInet, SockDgram, 0, fds) == -1 && GetErrno() == PosixEopnotsupp);
    Require(socketpair_nid_postfix(AfInet, SockStream, 17, fds) == -1 && GetErrno() == PosixEprotonosupport);
    Require(socketpair_nid_postfix(AfInet, SockDgram, 6, fds) == -1 && GetErrno() == PosixEprotonosupport);

    Require(socketpair_nid_postfix(AfInet6, SockStream, 0, fds) == -1 && GetErrno() == PosixEopnotsupp);
    Require(socketpair_nid_postfix(AfInet6, SockDgram, 0, fds) == -1 && GetErrno() == PosixEopnotsupp);
    Require(socketpair_nid_postfix(AfInet6, SockStream, 17, fds) == -1 && GetErrno() == PosixEprotonosupport);
    Require(socketpair_nid_postfix(AfInet6, SockDgram, 6, fds) == -1 && GetErrno() == PosixEprotonosupport);

    const int badFamilies[] = {-100, -1, 0, 3, 4, 10, 20, 99};
    for (const int family : badFamilies) {
        Require(socketpair_nid_postfix(family, SockStream, 0, fds) == -1 && GetErrno() == PosixEafnosupport);
    }

    Require(socketpair_nid_postfix(AfUnix, 0, 0, fds) == -1 && GetErrno() == PosixEprotonosupport);
    Require(socketpair_nid_postfix(AfUnix, SockRaw, 0, fds) == -1 && GetErrno() == PosixEprototype);
    Require(socketpair_nid_postfix(AfUnix, SockRdm, 0, fds) == -1 && GetErrno() == PosixEprototype);

    const int badProtocols[] = {-1, 1, 6, 17, 99};
    for (const int proto : badProtocols) {
        Require(socketpair_nid_postfix(AfUnix, SockStream, proto, fds) == -1 && GetErrno() == PosixEprotonosupport);
        Require(socketpair_nid_postfix(AfUnix, SockDgram, proto, fds) == -1 && GetErrno() == PosixEprotonosupport);
#if !defined(_WIN32) && !defined(__APPLE__)
        Require(socketpair_nid_postfix(AfUnix, SockSeqpacket, proto, fds) == -1 && GetErrno() == PosixEprotonosupport);
#endif
    }

#if !defined(_WIN32) && !defined(__APPLE__)
    const int validTypes[] = {SockStream, SockDgram, SockSeqpacket};
#else
    const int validTypes[] = {SockStream, SockDgram};
#endif
    for (const int type : validTypes) {
        {
            Require(socketpair_nid_postfix(AfUnix, type, 0, fds) == 0);
            Require(fds[0] >= 0 && fds[1] >= 0 && fds[0] != fds[1]);
            Require((fcntl_nid_postfix(fds[0], FcntlGetFd) & FcntlFdCloexec) == 0);
            Require((fcntl_nid_postfix(fds[1], FcntlGetFd) & FcntlFdCloexec) == 0);
            Require((fcntl_nid_postfix(fds[0], FcntlGetFl) & FcntlONonBlock) == 0);
            Require((fcntl_nid_postfix(fds[1], FcntlGetFl) & FcntlONonBlock) == 0);

            const char ping = 'A';
            Require(send_nid_postfix(fds[0], &ping, 1, 0) == 1);
            char pong = 0;
            Require(recv_nid_postfix(fds[1], &pong, 1, 0) == 1 && pong == 'A');

            Require(close_nid_postfix(fds[0]) == 0);
            Require(close_nid_postfix(fds[1]) == 0);
        }
        {
            Require(socketpair_nid_postfix(AfUnix, type | GuestCloseOnExec | GuestNonBlocking, 0, fds) == 0);
            Require(fds[0] >= 0 && fds[1] >= 0 && fds[0] != fds[1]);
            Require((fcntl_nid_postfix(fds[0], FcntlGetFd) & FcntlFdCloexec) == FcntlFdCloexec);
            Require((fcntl_nid_postfix(fds[1], FcntlGetFd) & FcntlFdCloexec) == FcntlFdCloexec);
            Require((fcntl_nid_postfix(fds[0], FcntlGetFl) & FcntlONonBlock) == FcntlONonBlock);
            Require((fcntl_nid_postfix(fds[1], FcntlGetFl) & FcntlONonBlock) == FcntlONonBlock);

            Require(close_nid_postfix(fds[0]) == 0);
            Require(close_nid_postfix(fds[1]) == 0);
        }
    }
}

} // namespace

int main() {
    TestSocketFamilyBounds();
    TestSocketTypeAndProtocolBounds();
    TestSocketCreationFlags();
    TestSysSocketex();
    TestSocketpairBounds();
    return 0;
}
