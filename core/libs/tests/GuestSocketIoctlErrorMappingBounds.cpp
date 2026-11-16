#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEnotty = 25;
constexpr int PosixEwouldblock = 35;
constexpr int PosixEnotsock = 38;
constexpr int PosixEaddrnotavail = 49;
constexpr int PosixEconnreset = 54;
constexpr int PosixEtimedout = 60;
constexpr int PosixEconnrefused = 61;
constexpr int PosixElast = 96;

constexpr std::uint64_t CmdFionread = 0x4004667fULL;
constexpr std::uint64_t CmdFionbio = 0x8004667eULL;
constexpr std::uint64_t CmdFioclex = 0x20006601ULL;
constexpr std::uint64_t CmdFionclex = 0x20006602ULL;
constexpr std::uint64_t CmdSiocgifconf = 0xc0106924ULL;
constexpr std::uint64_t CmdSiocgifflags = 0xc0206911ULL;
constexpr std::uint64_t CmdSiocgifaddr = 0xc0206921ULL;
constexpr std::uint64_t CmdSiocgifmtu = 0xc020697eULL;
constexpr std::uint64_t CmdMalformedInZero = 0x8000667fULL;
constexpr std::uint64_t CmdMalformedOutZero = 0x4000667fULL;
constexpr std::uint64_t CmdMalformedNoDir = 0x0000667fULL;

struct IfconfLayout {
    int ifcLen;
    int pad;
    void* ifcBuf;
};

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at %s:%u (errno %d)\n", location.file_name(), location.line(), *__error_nid_postfix());
        std::abort();
    }
}

void AssertErrnoInRange(int code) {
    Require(code >= 1 && code <= PosixElast);
}

void TestFaultPointerRejection() {
    const int socketFd = socket_nid_postfix(2, 2, 0);
    Require(socketFd >= 0);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socketFd, CmdFionbio, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);
    AssertErrnoInRange(*__error_nid_postfix());

    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socketFd, CmdFionbio, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socketFd, CmdSiocgifconf, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socketFd, CmdSiocgifconf, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    Require(close_nid_postfix(socketFd) == 0);
}

void TestMalformedRequests() {
    const int socketFd = socket_nid_postfix(2, 2, 0);
    Require(socketFd >= 0);

    int dummy = 0;

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socketFd, 0, &dummy) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);
    AssertErrnoInRange(*__error_nid_postfix());

    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socketFd, 0, &dummy) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socketFd, CmdMalformedInZero, &dummy) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(socketFd, CmdMalformedInZero, &dummy) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socketFd, CmdMalformedOutZero, &dummy) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(socketFd, CmdMalformedNoDir, &dummy) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    Require(close_nid_postfix(socketFd) == 0);
}

void TestFionbioSemantics() {
    const int socketFd = socket_nid_postfix(2, 2, 0);
    Require(socketFd >= 0);

    int enable = 1;
    Require(ioctl_nid_postfix(socketFd, CmdFionbio, &enable) == 0);

    char buffer[16]{};
    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(socketFd, buffer, sizeof(buffer), 0) == -1);
    Require(*__error_nid_postfix() == PosixEwouldblock || *__error_nid_postfix() == PosixEinval);
    AssertErrnoInRange(*__error_nid_postfix());

    enable = 0;
    Require(ioctl_nid_postfix(socketFd, CmdFionbio, &enable) == 0);

    enable = 1;
    Require(_ioctl_nid_postfix(socketFd, CmdFionbio, &enable) == 0);

    enable = 0;
    Require(_ioctl_nid_postfix(socketFd, CmdFionbio, &enable) == 0);

    Require(close_nid_postfix(socketFd) == 0);
}

void TestSiocgifconfAndInterfaceGroup() {
    const int inetSocket = socket_nid_postfix(2, 2, 0);
    Require(inetSocket >= 0);

    IfconfLayout conf{};
    conf.ifcLen = 0;
    conf.ifcBuf = nullptr;

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, CmdSiocgifconf, &conf) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);
    AssertErrnoInRange(*__error_nid_postfix());

    *__error_nid_postfix() = 0;
    Require(_ioctl_nid_postfix(inetSocket, CmdSiocgifconf, &conf) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, CmdSiocgifflags, &conf) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, CmdSiocgifaddr, &conf) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(inetSocket, CmdSiocgifmtu, &conf) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    Require(close_nid_postfix(inetSocket) == 0);

    int unixPair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, unixPair) == 0);

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(unixPair[0], CmdSiocgifconf, &conf) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    Require(close_nid_postfix(unixPair[0]) == 0);
    Require(close_nid_postfix(unixPair[1]) == 0);
}

void TestCloseOnExecIoctls() {
    const int socketFd = socket_nid_postfix(2, 2, 0);
    Require(socketFd >= 0);

    Require(ioctl_nid_postfix(socketFd, CmdFioclex, nullptr) == 0);
    Require(ioctl_nid_postfix(socketFd, CmdFionclex, nullptr) == 0);
    Require(_ioctl_nid_postfix(socketFd, CmdFioclex, nullptr) == 0);
    Require(_ioctl_nid_postfix(socketFd, CmdFionclex, nullptr) == 0);

    Require(close_nid_postfix(socketFd) == 0);
}

void TestPipeIoctlBounds() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    Require(pipeFds[0] >= 0 && pipeFds[1] >= 0);

    int available = -1;
    Require(ioctl_nid_postfix(pipeFds[0], CmdFionread, &available) == 0);
    Require(available == 0);

    Require(write_nid_postfix(pipeFds[1], "test", 4) == 4);

    Require(ioctl_nid_postfix(pipeFds[0], CmdFionread, &available) == 0);
    Require(available == 4);

    int nonblock = 1;
    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(pipeFds[0], CmdFionbio, &nonblock) == -1);
    Require(*__error_nid_postfix() == PosixEnotty);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestErrnoConstants() {
    static_assert(PosixEperm == 1);
    static_assert(PosixEbadf == 9);
    static_assert(PosixEfault == 14);
    static_assert(PosixEinval == 22);
    static_assert(PosixEnotty == 25);
    static_assert(PosixEwouldblock == 35);
    static_assert(PosixEnotsock == 38);
    static_assert(PosixEaddrnotavail == 49);
    static_assert(PosixEconnreset == 54);
    static_assert(PosixEtimedout == 60);
    static_assert(PosixEconnrefused == 61);
    static_assert(PosixElast == 96);

    AssertErrnoInRange(PosixEperm);
    AssertErrnoInRange(PosixEbadf);
    AssertErrnoInRange(PosixEfault);
    AssertErrnoInRange(PosixEinval);
    AssertErrnoInRange(PosixEnotty);
    AssertErrnoInRange(PosixEwouldblock);
    AssertErrnoInRange(PosixEnotsock);
    AssertErrnoInRange(PosixEaddrnotavail);
    AssertErrnoInRange(PosixEconnreset);
    AssertErrnoInRange(PosixEtimedout);
    AssertErrnoInRange(PosixEconnrefused);
    AssertErrnoInRange(PosixElast);
}

} // namespace

int main() {
    TestErrnoConstants();
    TestFaultPointerRejection();
    TestMalformedRequests();
    TestFionbioSemantics();
    TestSiocgifconfAndInterfaceGroup();
    TestCloseOnExecIoctls();
    TestPipeIoctlBounds();
    return 0;
}
