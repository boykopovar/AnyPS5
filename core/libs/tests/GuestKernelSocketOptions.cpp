#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SolSocket = 0xffff;
constexpr int SoReuseAddr = 0x4;
constexpr int SoKeepAlive = 0x8;
constexpr int SoLinger = 0x80;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorAgain = 35;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNoProtocolOption = 42;

constexpr std::uint64_t NonblockingRequest = 0x8004667e;
constexpr std::uint64_t ReadableRequest = 0x4004667f;

struct Linger {
    std::int32_t enabled;
    std::int32_t seconds;
};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

std::array<std::uint8_t, 16> Loopback(std::uint16_t portNetworkOrder) {
    std::array<std::uint8_t, 16> address{16, 2};
    std::memcpy(address.data() + 2, &portNetworkOrder, 2);
    address[4] = 127;
    address[7] = 1;
    return address;
}

std::array<std::uint8_t, 16> Listen(int* listener) {
    *listener = socket_nid_postfix(2, Stream, 0);
    Require(*listener >= 0, "socket failed");
    auto address = Loopback(0);
    Require(bind_nid_postfix(*listener, address.data(), address.size()) == 0, "bind failed");
    Require(listen_nid_postfix(*listener, 4) == 0, "listen failed");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(*listener, address.data(), &size) == 0, "getsockname failed");
    return address;
}

void AuditSocketOptionsParameterBounds() {
    const int sock = socket_nid_postfix(2, Stream, 0);
    Require(sock >= 0, "socket failed");

    int shortBuffer = 0;
    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, &shortBuffer, sizeof(shortBuffer)) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "SO_LINGER with short buffer must fail with EINVAL (22)");

    Linger badLingerNegative{1, -1};
    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, &badLingerNegative, sizeof(badLingerNegative)) == -1 && *__error_nid_postfix() == ErrorDomain,
        "SO_LINGER with negative seconds must fail with EDOM (33)");

    Linger badLingerOverflow{1, 70000};
    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, &badLingerOverflow, sizeof(badLingerOverflow)) == -1 && *__error_nid_postfix() == ErrorDomain,
        "SO_LINGER with seconds > 65535 must fail with EDOM (33)");

    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, nullptr, sizeof(Linger)) == -1 && *__error_nid_postfix() == ErrorFault,
        "SO_LINGER with nullptr must fail with EFAULT (14)");

    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, reinterpret_cast<void*>(std::uintptr_t{0x10}), sizeof(Linger)) == -1 && *__error_nid_postfix() == ErrorFault,
        "SO_LINGER with unmapped pointer must fail with EFAULT (14)");

    Linger validLinger{1, 10};
    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, &validLinger, sizeof(validLinger)) == 0, "setsockopt SO_LINGER enable failed");
    Linger readLinger{};
    std::uint32_t lingerLen = sizeof(readLinger);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoLinger, &readLinger, &lingerLen) == 0 && lingerLen == sizeof(readLinger), "getsockopt SO_LINGER failed");
    Require(readLinger.enabled == SoLinger && readLinger.seconds == 10, "SO_LINGER did not read back expected values");

    Linger disableLinger{0, 0};
    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, &disableLinger, sizeof(disableLinger)) == 0, "setsockopt SO_LINGER disable failed");
    lingerLen = sizeof(readLinger);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoLinger, &readLinger, &lingerLen) == 0 && readLinger.enabled == 0 && readLinger.seconds == 0,
        "SO_LINGER disabled did not read back zero");

    char smallFlag = 1;
    Require(setsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, &smallFlag, sizeof(smallFlag)) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "SO_REUSEADDR with short buffer must fail with EINVAL (22)");
    Require(setsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == ErrorFault,
        "SO_REUSEADDR with nullptr must fail with EFAULT (14)");
    Require(setsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, reinterpret_cast<void*>(std::uintptr_t{0x10}), sizeof(int)) == -1 && *__error_nid_postfix() == ErrorFault,
        "SO_REUSEADDR with unmapped pointer must fail with EFAULT (14)");

    int enableOption = 1;
    Require(setsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, &enableOption, sizeof(enableOption)) == 0, "setsockopt SO_REUSEADDR enable failed");
    int readOption = 0;
    std::uint32_t optionLen = sizeof(readOption);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, &readOption, &optionLen) == 0 && optionLen == sizeof(readOption), "getsockopt SO_REUSEADDR failed");
    Require(readOption == SoReuseAddr, "SO_REUSEADDR did not read back option bit");

    int disableOption = 0;
    Require(setsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, &disableOption, sizeof(disableOption)) == 0, "setsockopt SO_REUSEADDR disable failed");
    optionLen = sizeof(readOption);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoReuseAddr, &readOption, &optionLen) == 0 && optionLen == sizeof(readOption), "getsockopt SO_REUSEADDR failed");
    Require(readOption == 0, "SO_REUSEADDR disabled did not read back zero");

    Require(setsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, &smallFlag, sizeof(smallFlag)) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "SO_KEEPALIVE with short buffer must fail with EINVAL (22)");
    Require(setsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, nullptr, sizeof(int)) == -1 && *__error_nid_postfix() == ErrorFault,
        "SO_KEEPALIVE with nullptr must fail with EFAULT (14)");
    Require(setsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, reinterpret_cast<void*>(std::uintptr_t{0x10}), sizeof(int)) == -1 && *__error_nid_postfix() == ErrorFault,
        "SO_KEEPALIVE with unmapped pointer must fail with EFAULT (14)");

    Require(setsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, &enableOption, sizeof(enableOption)) == 0, "setsockopt SO_KEEPALIVE enable failed");
    optionLen = sizeof(readOption);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, &readOption, &optionLen) == 0 && optionLen == sizeof(readOption), "getsockopt SO_KEEPALIVE failed");
    Require(readOption == SoKeepAlive, "SO_KEEPALIVE did not read back option bit");

    Require(setsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, &disableOption, sizeof(disableOption)) == 0, "setsockopt SO_KEEPALIVE disable failed");
    optionLen = sizeof(readOption);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoKeepAlive, &readOption, &optionLen) == 0 && optionLen == sizeof(readOption), "getsockopt SO_KEEPALIVE failed");
    Require(readOption == 0, "SO_KEEPALIVE disabled did not read back zero");

    int optVal = 1;
    optionLen = sizeof(optVal);
    Require(setsockopt_nid_postfix(sock, 9999, 1, &optVal, sizeof(optVal)) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption,
        "setsockopt with invalid level must fail with ENOPROTOOPT (42)");
    Require(getsockopt_nid_postfix(sock, 9999, 1, &optVal, &optionLen) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption,
        "getsockopt with invalid level must fail with ENOPROTOOPT (42)");

    Require(setsockopt_nid_postfix(sock, SolSocket, 0x7fff, &optVal, sizeof(optVal)) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption,
        "setsockopt with unknown option must fail with ENOPROTOOPT (42)");
    Require(getsockopt_nid_postfix(sock, SolSocket, 0x7fff, &optVal, &optionLen) == -1 && *__error_nid_postfix() == ErrorNoProtocolOption,
        "getsockopt with unknown option must fail with ENOPROTOOPT (42)");

    Require(close_nid_postfix(sock) == 0, "close failed");
}

void AuditSocketOptionDescriptorErrors() {
    int val = 1;
    std::uint32_t len = sizeof(val);
    Linger lingerVal{1, 5};

    Require(setsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &val, sizeof(val)) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "setsockopt on -1 must fail with EBADF (9)");
    Require(getsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &val, &len) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "getsockopt on -1 must fail with EBADF (9)");
    Require(_setsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &val, sizeof(val)) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_setsockopt on -1 must fail with EBADF (9)");
    Require(_getsockopt_nid_postfix(-1, SolSocket, SoKeepAlive, &val, &len) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_getsockopt on -1 must fail with EBADF (9)");

    Require(setsockopt_nid_postfix(1 << 20, SolSocket, SoReuseAddr, &val, sizeof(val)) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "setsockopt on unopened descriptor must fail with EBADF (9)");
    Require(getsockopt_nid_postfix(1 << 20, SolSocket, SoReuseAddr, &val, &len) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "getsockopt on unopened descriptor must fail with EBADF (9)");

    const int closedSock = socket_nid_postfix(2, Stream, 0);
    Require(closedSock >= 0 && close_nid_postfix(closedSock) == 0, "socket or close failed");
    Require(setsockopt_nid_postfix(closedSock, SolSocket, SoLinger, &lingerVal, sizeof(lingerVal)) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "setsockopt on closed socket must fail with EBADF (9)");
    Require(getsockopt_nid_postfix(closedSock, SolSocket, SoLinger, &lingerVal, &len) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "getsockopt on closed socket must fail with EBADF (9)");

    int pipeEnds[2];
    Require(pipe_nid_postfix(pipeEnds) == 0, "pipe failed");
    Require(setsockopt_nid_postfix(pipeEnds[0], SolSocket, SoLinger, &lingerVal, sizeof(lingerVal)) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "setsockopt on pipe must fail with ENOTSOCK (38)");
    Require(getsockopt_nid_postfix(pipeEnds[0], SolSocket, SoLinger, &lingerVal, &len) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "getsockopt on pipe must fail with ENOTSOCK (38)");
    Require(setsockopt_nid_postfix(pipeEnds[0], SolSocket, SoReuseAddr, &val, sizeof(val)) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "setsockopt on pipe must fail with ENOTSOCK (38)");
    Require(getsockopt_nid_postfix(pipeEnds[0], SolSocket, SoReuseAddr, &val, &len) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "getsockopt on pipe must fail with ENOTSOCK (38)");
    Require(setsockopt_nid_postfix(pipeEnds[0], SolSocket, SoKeepAlive, &val, sizeof(val)) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "setsockopt on pipe must fail with ENOTSOCK (38)");
    Require(getsockopt_nid_postfix(pipeEnds[0], SolSocket, SoKeepAlive, &val, &len) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "getsockopt on pipe must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(pipeEnds[0]) == 0 && close_nid_postfix(pipeEnds[1]) == 0, "close pipe failed");

    const int activeSock = socket_nid_postfix(2, Stream, 0);
    Require(activeSock >= 0, "socket failed");
    Require(getsockopt_nid_postfix(activeSock, SolSocket, SoKeepAlive, &val, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "getsockopt with null length must fail with EFAULT (14)");
    Require(getsockopt_nid_postfix(activeSock, SolSocket, SoKeepAlive, &val, reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10})) == -1 && *__error_nid_postfix() == ErrorFault,
        "getsockopt with unmapped length must fail with EFAULT (14)");
    Require(getsockopt_nid_postfix(activeSock, SolSocket, SoKeepAlive, reinterpret_cast<void*>(std::uintptr_t{0x10}), &len) == -1 && *__error_nid_postfix() == ErrorFault,
        "getsockopt with unmapped value pointer must fail with EFAULT (14)");
    Require(close_nid_postfix(activeSock) == 0, "close failed");
}

void AuditIoctlParameterBoundsAndErrors() {
    int on = 1;
    int queued = -1;

    Require(ioctl_nid_postfix(-1, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on -1 must fail with EBADF (9)");
    Require(ioctl_nid_postfix(-1, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on -1 must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(-1, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONBIO on -1 must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(-1, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD on -1 must fail with EBADF (9)");

    Require(ioctl_nid_postfix(1 << 20, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on unopened descriptor must fail with EBADF (9)");
    Require(ioctl_nid_postfix(1 << 20, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on unopened descriptor must fail with EBADF (9)");

    const int sock = socket_nid_postfix(2, Stream, 0);
    Require(sock >= 0, "socket failed");

    Require(ioctl_nid_postfix(sock, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(sock, NonblockingRequest, reinterpret_cast<void*>(std::uintptr_t{0x10})) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO with unmapped pointer must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD with nullptr must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, reinterpret_cast<void*>(std::uintptr_t{0x10})) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD with unmapped pointer must fail with EFAULT (14)");

    Require(_ioctl_nid_postfix(sock, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(sock, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONREAD with nullptr must fail with EFAULT (14)");

    Require(close_nid_postfix(sock) == 0, "close failed");
    Require(ioctl_nid_postfix(sock, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on closed descriptor must fail with EBADF (9)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on closed descriptor must fail with EBADF (9)");
}

void AuditNonblockingEmptyRead() {
    int listener = -1;
    auto address = Listen(&listener);
    const int client = socket_nid_postfix(2, Stream, 0);
    Require(client >= 0, "client socket failed");
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0, "accept failed");

    int queuedBytes = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queuedBytes) == 0 && queuedBytes == 0,
        "initial FIONREAD on connected server must be 0");
    Require(_ioctl_nid_postfix(server, ReadableRequest, &queuedBytes) == 0 && queuedBytes == 0,
        "initial _ioctl FIONREAD on connected server must be 0");

    int nonblocking = 1;
    Require(ioctl_nid_postfix(server, NonblockingRequest, &nonblocking) == 0, "set FIONBIO nonblocking failed");

    char buffer[64]{};
    Require(read_nid_postfix(server, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == ErrorAgain,
        "empty read on nonblocking socket must return EWOULDBLOCK (35)");
    Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == -1 && *__error_nid_postfix() == ErrorAgain,
        "empty recv on nonblocking socket must return EWOULDBLOCK (35)");
    Require(read_nid_postfix(server, buffer, 0) == 0, "zero-length read must return 0");

    nonblocking = 0;
    Require(ioctl_nid_postfix(server, NonblockingRequest, &nonblocking) == 0, "disable FIONBIO nonblocking failed");

    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(server) == 0, "close server failed");
    Require(close_nid_postfix(listener) == 0, "close listener failed");

    const int dgram = socket_nid_postfix(2, Datagram, 0);
    Require(dgram >= 0, "dgram socket failed");
    queuedBytes = -1;
    Require(ioctl_nid_postfix(dgram, ReadableRequest, &queuedBytes) == 0 && queuedBytes == 0,
        "FIONREAD on datagram socket must be 0");
    nonblocking = 1;
    Require(ioctl_nid_postfix(dgram, NonblockingRequest, &nonblocking) == 0, "set FIONBIO nonblocking on dgram failed");
    Require(recv_nid_postfix(dgram, buffer, sizeof(buffer), 0) == -1 && *__error_nid_postfix() == ErrorAgain,
        "empty recv on nonblocking dgram socket must return EWOULDBLOCK (35)");
    Require(close_nid_postfix(dgram) == 0, "close dgram failed");
}

} // namespace

int main() {
    AuditSocketOptionsParameterBounds();
    AuditSocketOptionDescriptorErrors();
    AuditIoctlParameterBoundsAndErrors();
    AuditNonblockingEmptyRead();
    return 0;
}
