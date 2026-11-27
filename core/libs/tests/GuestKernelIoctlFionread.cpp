#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int Stream = 1;
constexpr int Datagram = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotTerminal = 25;
constexpr int ErrorAgain = 35;
constexpr int ErrorNotConnected = 57;
constexpr int DirectoryFlag = 0x00020000;
constexpr int ErrorAddressNotAvailable = 49;
constexpr short PollIn = 0x1;

constexpr std::uint64_t NonblockingRequest = 0x8004667e;
constexpr std::uint64_t ReadableRequest = 0x4004667f;
constexpr std::uint64_t MalformedRequest = 0;
constexpr std::uint64_t UnknownRequest = 0x12345678;
constexpr std::uint64_t TerminalGroupRequest = 0x20007401;
constexpr std::uint64_t WriteQueueRequest = 0x40046677;
constexpr std::uint64_t WriteSpaceRequest = 0x40046676;
constexpr std::uint64_t DeviceTypeRequest = 0x4004667a;
constexpr std::uint64_t ForeignGroupRequest = 0x80047801;
constexpr std::uint64_t ForeignWideRequest = 0x80087801;
constexpr std::uint64_t ForeignIntegerRequest = 0xa0047801;
constexpr std::uint64_t ForeignOutputRequest = 0x40047801;
constexpr std::uint64_t TerminalInputRequest = 0x80047401;

void Require(bool condition, const char* message, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d) at %s:%u\n", message, *__error_nid_postfix(), location.file_name(), location.line());
    std::abort();
}

std::array<std::uint8_t, 16> LoopbackAddress() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    return address;
}

std::array<std::uint8_t, 16> CreateBoundListener(int* listener) {
    *listener = socket_nid_postfix(2, Stream, 0);
    Require(*listener >= 0, "socket creation failed");
    auto address = LoopbackAddress();
    Require(bind_nid_postfix(*listener, address.data(), address.size()) == 0, "bind failed");
    Require(listen_nid_postfix(*listener, 4) == 0, "listen failed");
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(*listener, address.data(), &size) == 0, "getsockname failed");
    return address;
}

void AuditInvalidDescriptors() {
    int on = 1;
    int queued = -1;

    Require(ioctl_nid_postfix(-1, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on -1 must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(-1, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONBIO on -1 must fail with EBADF (9)");
    Require(ioctl_nid_postfix(-1, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on -1 must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(-1, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD on -1 must fail with EBADF (9)");

    Require(ioctl_nid_postfix(-42, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on -42 must fail with EBADF (9)");
    Require(ioctl_nid_postfix(-42, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on -42 must fail with EBADF (9)");

    Require(ioctl_nid_postfix(1000000, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on unopened descriptor must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(1000000, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONBIO on unopened descriptor must fail with EBADF (9)");
    Require(ioctl_nid_postfix(1000000, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on unopened descriptor must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(1000000, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD on unopened descriptor must fail with EBADF (9)");

    const int sock = socket_nid_postfix(2, Stream, 0);
    Require(sock >= 0, "socket creation failed");
    Require(close_nid_postfix(sock) == 0, "close socket failed");

    Require(ioctl_nid_postfix(sock, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on closed socket must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(sock, NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONBIO on closed socket must fail with EBADF (9)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on closed socket must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(sock, ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD on closed socket must fail with EBADF (9)");

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");
    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");

    Require(ioctl_nid_postfix(pipeFds[0], NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on closed pipe read-end must fail with EBADF (9)");
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on closed pipe read-end must fail with EBADF (9)");
    Require(ioctl_nid_postfix(pipeFds[1], NonblockingRequest, &on) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONBIO on closed pipe write-end must fail with EBADF (9)");
    Require(ioctl_nid_postfix(pipeFds[1], ReadableRequest, &queued) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD on closed pipe write-end must fail with EBADF (9)");
}

void AuditNullAndFaultPointers() {
    const int sock = socket_nid_postfix(2, Stream, 0);
    Require(sock >= 0, "socket creation failed");

    Require(ioctl_nid_postfix(sock, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(sock, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(sock, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONREAD with nullptr must fail with EFAULT (14)");

    void* const unmapped = reinterpret_cast<void*>(std::uintptr_t{0x10});
    Require(ioctl_nid_postfix(sock, NonblockingRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO with unmapped pointer must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(sock, NonblockingRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO with unmapped pointer must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONREAD with unmapped pointer must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(sock, ReadableRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONREAD with unmapped pointer must fail with EFAULT (14)");

    Require(close_nid_postfix(sock) == 0, "close socket failed");

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(sock, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO null pointer on closed descriptor must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(sock, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO null pointer on closed descriptor must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(sock, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD null pointer on closed descriptor must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(sock, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD null pointer on closed descriptor must fail with EBADF (9)");

    *__error_nid_postfix() = 0;
    Require(ioctl_nid_postfix(-1, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "ioctl FIONBIO null pointer on -1 descriptor must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(-1, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_ioctl FIONBIO null pointer on -1 descriptor must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(-1, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "ioctl FIONREAD null pointer on -1 descriptor must fail with EBADF (9)");
    Require(_ioctl_nid_postfix(-1, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_ioctl FIONREAD null pointer on -1 descriptor must fail with EBADF (9)");

    const int dgram = socket_nid_postfix(2, Datagram, 0);
    Require(dgram >= 0, "datagram socket creation failed");

    Require(ioctl_nid_postfix(dgram, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "dgram ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(dgram, NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "dgram _ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(dgram, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "dgram ioctl FIONREAD with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(dgram, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "dgram _ioctl FIONREAD with nullptr must fail with EFAULT (14)");

    Require(close_nid_postfix(dgram) == 0, "close dgram socket failed");

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");

    Require(ioctl_nid_postfix(pipeFds[0], NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe read-end ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(pipeFds[0], NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe read-end _ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe read-end ioctl FIONREAD with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(pipeFds[0], ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe read-end _ioctl FIONREAD with nullptr must fail with EFAULT (14)");

    Require(ioctl_nid_postfix(pipeFds[1], NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe write-end ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(pipeFds[1], NonblockingRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe write-end _ioctl FIONBIO with nullptr must fail with EFAULT (14)");
    Require(ioctl_nid_postfix(pipeFds[1], ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe write-end ioctl FIONREAD with nullptr must fail with EFAULT (14)");
    Require(_ioctl_nid_postfix(pipeFds[1], ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "pipe write-end _ioctl FIONREAD with nullptr must fail with EFAULT (14)");

    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");
}

void AuditInvalidRequestCodes() {
    const int sock = socket_nid_postfix(2, Stream, 0);
    Require(sock >= 0, "socket creation failed");

    int nbytes = 0;
    Require(ioctl_nid_postfix(sock, MalformedRequest, &nbytes) == -1, "malformed request on socket must fail");
    const int sockMalformedErr = *__error_nid_postfix();
    Require(sockMalformedErr == ErrorNotTerminal || sockMalformedErr == ErrorInvalid,
        "malformed request on socket must yield ENOTTY (25) or EINVAL (22)");

    Require(_ioctl_nid_postfix(sock, MalformedRequest, &nbytes) == -1, "_ioctl malformed request on socket must fail");
    const int sockUnderscoreErr = *__error_nid_postfix();
    Require(sockUnderscoreErr == ErrorNotTerminal || sockUnderscoreErr == ErrorInvalid,
        "_ioctl malformed request on socket must yield ENOTTY (25) or EINVAL (22)");

    Require(ioctl_nid_postfix(sock, UnknownRequest, &nbytes) == -1, "unknown request on socket must fail");
    const int sockUnknownErr = *__error_nid_postfix();
    Require(sockUnknownErr == ErrorNotTerminal || sockUnknownErr == ErrorInvalid,
        "unknown request on socket must yield ENOTTY (25) or EINVAL (22)");

    Require(close_nid_postfix(sock) == 0, "close socket failed");

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");

    Require(ioctl_nid_postfix(pipeFds[0], MalformedRequest, &nbytes) == -1, "malformed request on pipe must fail");
    const int pipeMalformedErr = *__error_nid_postfix();
    Require(pipeMalformedErr == ErrorNotTerminal || pipeMalformedErr == ErrorInvalid,
        "malformed request on pipe must yield ENOTTY (25) or EINVAL (22)");

    Require(_ioctl_nid_postfix(pipeFds[0], MalformedRequest, &nbytes) == -1, "_ioctl malformed request on pipe must fail");
    const int pipeUnderscoreErr = *__error_nid_postfix();
    Require(pipeUnderscoreErr == ErrorNotTerminal || pipeUnderscoreErr == ErrorInvalid,
        "_ioctl malformed request on pipe must yield ENOTTY (25) or EINVAL (22)");

    Require(ioctl_nid_postfix(pipeFds[0], UnknownRequest, &nbytes) == -1, "unknown request on pipe must fail");
    const int pipeUnknownErr = *__error_nid_postfix();
    Require(pipeUnknownErr == ErrorNotTerminal || pipeUnknownErr == ErrorInvalid,
        "unknown request on pipe must yield ENOTTY (25) or EINVAL (22)");

    Require(ioctl_nid_postfix(pipeFds[0], TerminalGroupRequest, &nbytes) == -1, "terminal request on pipe must fail");
    const int pipeTerminalErr = *__error_nid_postfix();
    Require(pipeTerminalErr == ErrorNotTerminal || pipeTerminalErr == ErrorInvalid,
        "terminal request on pipe must yield ENOTTY (25) or EINVAL (22)");

    for (const int end : pipeFds) {
        for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
            for (const std::uint64_t request : {WriteQueueRequest, WriteSpaceRequest, DeviceTypeRequest, ForeignGroupRequest}) {
                int argument = 0x5a5a5a5a;
                Require(call(end, request, &argument) == -1 && *__error_nid_postfix() == ErrorNotTerminal && argument == 0x5a5a5a5a,
                    "a request pipe_ioctl does not handle must fail with ENOTTY (25) and leave its argument alone");
            }
            Require(call(end, ForeignGroupRequest, reinterpret_cast<void*>(std::uintptr_t{0x10})) == -1 && *__error_nid_postfix() == ErrorFault,
                "an unreadable input argument must fail with EFAULT (14), as sys_ioctl copies it in before pipe_ioctl");
            Require(call(end, WriteQueueRequest, reinterpret_cast<void*>(std::uintptr_t{0x10})) == -1 && *__error_nid_postfix() == ErrorNotTerminal,
                "an output request pipe_ioctl does not handle must fail with ENOTTY (25) without touching its argument");
            for (void* integer : {static_cast<void*>(nullptr), reinterpret_cast<void*>(std::uintptr_t{0x10})}) {
                Require(call(end, ForeignIntegerRequest, integer) == -1 && *__error_nid_postfix() == ErrorNotTerminal,
                    "an IOC_VOID request passes its argument by value, so nothing is copied in before ENOTTY (25), as in sys_ioctl");
            }
        }
    }

    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");
}

void AuditEmptySocketReadableQuery() {
    int listener = -1;
    const auto address = CreateBoundListener(&listener);

    const int client = socket_nid_postfix(2, Stream, 0);
    Require(client >= 0, "client socket creation failed");
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0, "accept failed");

    int clientReadable = -1;
    Require(ioctl_nid_postfix(client, ReadableRequest, &clientReadable) == 0 && clientReadable == 0,
        "empty client socket FIONREAD must return 0");
    clientReadable = -1;
    Require(_ioctl_nid_postfix(client, ReadableRequest, &clientReadable) == 0 && clientReadable == 0,
        "empty client socket _ioctl FIONREAD must return 0");

    int serverReadable = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &serverReadable) == 0 && serverReadable == 0,
        "empty server socket FIONREAD must return 0");
    serverReadable = -1;
    Require(_ioctl_nid_postfix(server, ReadableRequest, &serverReadable) == 0 && serverReadable == 0,
        "empty server socket _ioctl FIONREAD must return 0");

    const char payload[] = "12345678";
    constexpr std::int64_t payloadSize = 8;
    Require(send_nid_postfix(client, payload, payloadSize, 0) == payloadSize, "send payload failed");

    GuestPollDescriptor arrival{server, PollIn, 0};
    Require(poll_nid_postfix(&arrival, 1, 5000) == 1, "the payload did not arrive at the server socket");

    int queuedBytes = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queuedBytes) == 0 && queuedBytes == payloadSize,
        "server socket FIONREAD must return written byte count");

    char recvBuffer[16]{};
    Require(recv_nid_postfix(server, recvBuffer, 3, 0) == 3, "partial recv payload failed");

    queuedBytes = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queuedBytes) == 0 && queuedBytes == payloadSize - 3,
        "server socket FIONREAD after partial read must return remaining byte count");

    Require(recv_nid_postfix(server, recvBuffer + 3, payloadSize - 3, 0) == payloadSize - 3, "remaining recv payload failed");
    Require(std::memcmp(recvBuffer, payload, payloadSize) == 0, "payload mismatch");

    queuedBytes = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queuedBytes) == 0 && queuedBytes == 0,
        "drained server socket FIONREAD must return 0");

    Require(shutdown_nid_postfix(server, 0) == 0, "shutdown server read failed");
    queuedBytes = -1;
    Require(ioctl_nid_postfix(server, ReadableRequest, &queuedBytes) == 0 && queuedBytes == 0,
        "shutdown server socket FIONREAD must return 0");

    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(server) == 0, "close server failed");
    Require(close_nid_postfix(listener) == 0, "close listener failed");

    const int dgram = socket_nid_postfix(2, Datagram, 0);
    Require(dgram >= 0, "dgram socket creation failed");

    int dgramReadable = -1;
    Require(ioctl_nid_postfix(dgram, ReadableRequest, &dgramReadable) == 0 && dgramReadable == 0,
        "empty dgram socket FIONREAD must return 0");
    dgramReadable = -1;
    Require(_ioctl_nid_postfix(dgram, ReadableRequest, &dgramReadable) == 0 && dgramReadable == 0,
        "empty dgram socket _ioctl FIONREAD must return 0");

    Require(close_nid_postfix(dgram) == 0, "close dgram failed");

    const int udpReceiver = socket_nid_postfix(2, Datagram, 0);
    Require(udpReceiver >= 0, "udp receiver creation failed");
    std::array<std::uint8_t, 16> udpAddr{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(udpReceiver, udpAddr.data(), udpAddr.size()) == 0, "bind udp receiver failed");
    std::uint32_t udpAddrLen = udpAddr.size();
    Require(getsockname_nid_postfix(udpReceiver, udpAddr.data(), &udpAddrLen) == 0, "getsockname udp receiver failed");

    const int udpSender = socket_nid_postfix(2, Datagram, 0);
    Require(udpSender >= 0, "udp sender creation failed");

    const char udpPayload[] = "udp123";
    constexpr std::int64_t udpPayloadSize = 6;
    Require(sendto_nid_postfix(udpSender, udpPayload, udpPayloadSize, 0, udpAddr.data(), udpAddr.size()) == udpPayloadSize,
        "sendto udp payload failed");

    GuestPollDescriptor udpArrival{udpReceiver, PollIn, 0};
    Require(poll_nid_postfix(&udpArrival, 1, 5000) == 1, "udp payload did not arrive");

    int udpQueued = -1;
    Require(ioctl_nid_postfix(udpReceiver, ReadableRequest, &udpQueued) == 0 && udpQueued == udpPayloadSize,
        "udp receiver FIONREAD must return queued datagram byte count");

    char udpRecvBuf[16]{};
    Require(recvfrom_nid_postfix(udpReceiver, udpRecvBuf, sizeof(udpRecvBuf), 0, nullptr, nullptr) == udpPayloadSize,
        "recvfrom udp payload failed");
    Require(std::memcmp(udpRecvBuf, udpPayload, udpPayloadSize) == 0, "udp payload mismatch");

    udpQueued = -1;
    Require(ioctl_nid_postfix(udpReceiver, ReadableRequest, &udpQueued) == 0 && udpQueued == 0,
        "drained udp receiver FIONREAD must return 0");

    Require(close_nid_postfix(udpReceiver) == 0, "close udp receiver failed");
    Require(close_nid_postfix(udpSender) == 0, "close udp sender failed");
}

void AuditEmptyPipeReadableQuery() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");

    int readEndBytes = -1;
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &readEndBytes) == 0 && readEndBytes == 0,
        "empty pipe read-end FIONREAD must return 0");
    readEndBytes = -1;
    Require(_ioctl_nid_postfix(pipeFds[0], ReadableRequest, &readEndBytes) == 0 && readEndBytes == 0,
        "empty pipe read-end _ioctl FIONREAD must return 0");

    int writeEndBytes = -1;
    Require(ioctl_nid_postfix(pipeFds[1], ReadableRequest, &writeEndBytes) == 0 && writeEndBytes == 0,
        "pipe write-end FIONREAD must return 0");

    const char pipeMessage[] = "fionread_pipe_data";
    const auto messageLength = static_cast<std::int64_t>(sizeof(pipeMessage) - 1);
    Require(write_nid_postfix(pipeFds[1], pipeMessage, messageLength) == messageLength, "write to pipe failed");

    readEndBytes = -1;
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &readEndBytes) == 0 && readEndBytes == messageLength,
        "pipe read-end FIONREAD must return buffered byte count");
    readEndBytes = -1;
    Require(_ioctl_nid_postfix(pipeFds[0], ReadableRequest, &readEndBytes) == 0 && readEndBytes == messageLength,
        "pipe read-end _ioctl FIONREAD must return buffered byte count");

    char readBuffer[64]{};
    Require(read_nid_postfix(pipeFds[0], readBuffer, sizeof(readBuffer)) == messageLength, "read from pipe failed");
    Require(std::memcmp(readBuffer, pipeMessage, messageLength) == 0, "pipe buffer mismatch");

    readEndBytes = -1;
    Require(ioctl_nid_postfix(pipeFds[0], ReadableRequest, &readEndBytes) == 0 && readEndBytes == 0,
        "drained pipe read-end FIONREAD must return 0");

    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");
}

void AuditDirectoryReadableQuery() {
    constexpr char Path[] = "anyps5-ioctl-fionread-directory";
    rmdir_nid_postfix(Path);
    Require(mkdir_nid_postfix(Path, 0755) == 0, "directory creation failed");

    const int directory = open_nid_postfix(Path, DirectoryFlag, 0);
    Require(directory >= 0, "directory open failed");

    int readable = -1;
    Require(ioctl_nid_postfix(directory, ReadableRequest, &readable) == 0 && readable == 65536,
        "directory FIONREAD must return the console's 65536-byte value");
    readable = -1;
    Require(_ioctl_nid_postfix(directory, ReadableRequest, &readable) == 0 && readable == 65536,
        "directory _ioctl FIONREAD must return the console's 65536-byte value");

    Require(close_nid_postfix(directory) == 0, "directory close failed");
    Require(rmdir_nid_postfix(Path) == 0, "directory cleanup failed");
}

void AuditNonblockingModeQueries() {
    int listener = -1;
    const auto address = CreateBoundListener(&listener);

    const int client = socket_nid_postfix(2, Stream, 0);
    Require(client >= 0, "client socket creation failed");
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0, "accept failed");

    int nonblocking = 1;
    Require(ioctl_nid_postfix(server, NonblockingRequest, &nonblocking) == 0, "set FIONBIO nonblocking failed");

    char buffer[32]{};
    Require(read_nid_postfix(server, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == ErrorAgain,
        "empty read on nonblocking socket must return EWOULDBLOCK (35)");
    Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == -1 && *__error_nid_postfix() == ErrorAgain,
        "empty recv on nonblocking socket must return EWOULDBLOCK (35)");

    int customNonzero = 100;
    Require(_ioctl_nid_postfix(server, NonblockingRequest, &customNonzero) == 0,
        "set FIONBIO with nonzero value via _ioctl failed");

    int largeFionbio = 0x12345678;
    Require(ioctl_nid_postfix(server, NonblockingRequest, &largeFionbio) == 0,
        "set FIONBIO with large value failed");

    int blocking = 0;
    Require(ioctl_nid_postfix(server, NonblockingRequest, &blocking) == 0, "clear FIONBIO nonblocking failed");

    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(server) == 0, "close server failed");
    Require(close_nid_postfix(listener) == 0, "close listener failed");

    constexpr int Inet6 = 28;
    const int v6Stream = socket_nid_postfix(Inet6, Stream, 0);
    if (v6Stream >= 0) {
        int v6Large = 42;
        Require(ioctl_nid_postfix(v6Stream, NonblockingRequest, &v6Large) == 0, "set FIONBIO on v6 stream failed");
        Require(read_nid_postfix(v6Stream, buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == ErrorNotConnected,
            "read on unconnected nonblocking v6 stream must return ENOTCONN (57)");
        int v6Clear = 0;
        Require(ioctl_nid_postfix(v6Stream, NonblockingRequest, &v6Clear) == 0, "clear FIONBIO on v6 stream failed");
        Require(close_nid_postfix(v6Stream) == 0, "close v6 stream failed");
    }

    const int v6Dgram = socket_nid_postfix(Inet6, Datagram, 0);
    if (v6Dgram >= 0) {
        std::array<std::uint8_t, 28> any6{28, 28};
        Require(bind_nid_postfix(v6Dgram, any6.data(), any6.size()) == 0, "bind v6 dgram to [::]:0 failed");
        int v6Large = 42;
        Require(ioctl_nid_postfix(v6Dgram, NonblockingRequest, &v6Large) == 0, "set FIONBIO on v6 dgram failed");
        const auto received = recv_nid_postfix(v6Dgram, buffer, sizeof(buffer), 0);
        const int error = *__error_nid_postfix();
        Require(received == -1 && error == ErrorAgain, "recv on nonblocking v6 dgram must fail with EWOULDBLOCK");
        int v6Clear = 0;
        Require(ioctl_nid_postfix(v6Dgram, NonblockingRequest, &v6Clear) == 0, "clear FIONBIO on v6 dgram failed");
        Require(close_nid_postfix(v6Dgram) == 0, "close v6 dgram failed");
    }

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");

    nonblocking = 1;
    Require(ioctl_nid_postfix(pipeFds[0], NonblockingRequest, &nonblocking) == 0,
        "set pipe read-end FIONBIO nonblocking failed");
    Require(_ioctl_nid_postfix(pipeFds[0], NonblockingRequest, &nonblocking) == 0,
        "set pipe read-end FIONBIO nonblocking via _ioctl failed");

    Require(ioctl_nid_postfix(pipeFds[1], NonblockingRequest, &nonblocking) == 0,
        "set pipe write-end FIONBIO nonblocking failed");

    Require(read_nid_postfix(pipeFds[0], buffer, sizeof(buffer)) == -1 && *__error_nid_postfix() == ErrorAgain,
        "empty read on nonblocking pipe must return EWOULDBLOCK (35)");

    blocking = 0;
    Require(ioctl_nid_postfix(pipeFds[0], NonblockingRequest, &blocking) == 0,
        "clear pipe read-end FIONBIO nonblocking failed");
    Require(ioctl_nid_postfix(pipeFds[1], NonblockingRequest, &blocking) == 0,
        "clear pipe write-end FIONBIO nonblocking failed");

    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");
}

void AuditInputCopiedBeforeDispatch() {
    void* const unmapped = reinterpret_cast<void*>(std::uintptr_t{0x10});
    constexpr std::size_t Page = 0x4000;
    auto* const pages = static_cast<unsigned char*>(mmap_nid_postfix(nullptr, 2 * Page, 3, 0x1002, -1, 0));
    Require(pages != reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(-1)) && mprotect_nid_postfix(pages + Page, Page, 0) == 0,
        "guard page setup failed");
    for (const auto call : {ioctl_nid_postfix, _ioctl_nid_postfix}) {
        Require(call(9999, NonblockingRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
            "an unreadable input argument must fail with EFAULT (14) before the descriptor is looked up, as sys_ioctl copies it in first");
        Require(call(9999, ForeignWideRequest, pages + Page - 4) == -1 && *__error_nid_postfix() == ErrorFault,
            "an input argument whose last bytes are unreadable must fail with EFAULT (14), as sys_ioctl copies the whole length in");
        Require(call(9999, ReadableRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "an output request on a closed descriptor must fail with EBADF (9), as nothing is copied before kern_ioctl");
        const int queue = kqueue_nid_postfix();
        Require(queue >= 0, "kqueue creation failed");
        Require(call(queue, ForeignGroupRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
            "an unreadable input argument on a kqueue must fail with EFAULT (14) before ENOTTY");
        Require(close_nid_postfix(queue) == 0, "close kqueue failed");
        int pipeFds[2]{-1, -1};
        Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");
        Require(call(pipeFds[0], TerminalInputRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
            "an unreadable termios input argument must fail with EFAULT (14) before ENOTTY");
        Require(close_nid_postfix(pipeFds[0]) == 0 && close_nid_postfix(pipeFds[1]) == 0, "close pipe failed");
        const int sock = socket_nid_postfix(2, Datagram, 0);
        Require(sock >= 0, "socket creation failed");
        Require(call(sock, ForeignGroupRequest, unmapped) == -1 && *__error_nid_postfix() == ErrorFault,
            "an unreadable input argument on a socket must fail with EFAULT (14) before the request is refused");
        Require(call(sock, ForeignOutputRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorAddressNotAvailable,
            "an output request in_control refuses must fail with EADDRNOTAVAIL (49) with a null argument, as nothing is copied out on an error");
        Require(call(sock, ForeignIntegerRequest, nullptr) == -1 && *__error_nid_postfix() == ErrorAddressNotAvailable,
            "an IOC_VOID request passes its argument by value, so a null argument gets the EADDRNOTAVAIL (49) of in_control");
        Require(close_nid_postfix(sock) == 0, "close socket failed");
    }
    Require(munmap_nid_postfix(pages, 2 * Page) == 0, "guard page release failed");
}

} // namespace

int main() {
    AuditInvalidDescriptors();
    AuditNullAndFaultPointers();
    AuditInputCopiedBeforeDispatch();
    AuditInvalidRequestCodes();
    AuditEmptySocketReadableQuery();
    AuditEmptyPipeReadableQuery();
    AuditDirectoryReadableQuery();
    AuditNonblockingModeQueries();
    return 0;
}
