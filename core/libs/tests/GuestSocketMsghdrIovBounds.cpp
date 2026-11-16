#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Datagram = 2;

constexpr int SolSocket = 0xffff;
constexpr int SoTimestamp = 0x400;
constexpr int SoBintime = 0x2000;

constexpr int MsgPeek = 0x2;
constexpr int MsgTrunc = 0x10;
constexpr int MsgCtrun = 0x20;
constexpr int MsgWaitAll = 0x40;
constexpr int MsgDontWait = 0x80;
constexpr int MsgNoSignal = 0x20000;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorWouldBlock = 35;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorMessageSize = 40;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorNotConnected = 57;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

} // namespace

int main() {
    const int probeSocket = socket_nid_postfix(Inet, Stream, 0);
    Require(probeSocket >= 0, "socket probe creation failed");

    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(probeSocket, nullptr, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "sendmsg with null message must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_sendmsg_nid_postfix(probeSocket, nullptr, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "_sendmsg with null message must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(recvmsg_nid_postfix(probeSocket, nullptr, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "recvmsg with null message must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_recvmsg_nid_postfix(probeSocket, nullptr, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "_recvmsg with null message must fail with EFAULT (14)");

    GuestMsghdr msg{};
    for (const int invalidIovLen : {-100, -1, 1025, 2000}) {
        msg.iovLength = invalidIovLen;
        msg.iov = nullptr;
        *__error_nid_postfix() = 0;
        Require(sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorMessageSize,
            "sendmsg with out-of-range iovLength must fail with EMSGSIZE (40)");
        *__error_nid_postfix() = 0;
        Require(_sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorMessageSize,
            "_sendmsg with out-of-range iovLength must fail with EMSGSIZE (40)");
        *__error_nid_postfix() = 0;
        Require(recvmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorMessageSize,
            "recvmsg with out-of-range iovLength must fail with EMSGSIZE (40)");
        *__error_nid_postfix() = 0;
        Require(_recvmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorMessageSize,
            "_recvmsg with out-of-range iovLength must fail with EMSGSIZE (40)");
    }

    msg.iovLength = 1;
    msg.iov = nullptr;
    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "sendmsg with positive iovLength and null iov must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "_sendmsg with positive iovLength and null iov must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(recvmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "recvmsg with positive iovLength and null iov must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_recvmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "_recvmsg with positive iovLength and null iov must fail with EFAULT (14)");

    KernelIovec nullBaseIov{nullptr, 16};
    msg.iov = &nullBaseIov;
    msg.iovLength = 1;
    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "sendmsg with null iov_base and non-zero length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorFault,
        "_sendmsg with null iov_base and non-zero length must fail with EFAULT (14)");

    KernelIovec hugeIov[2] = {
        {reinterpret_cast<void*>(0x1000), static_cast<std::size_t>(INT_MAX)},
        {reinterpret_cast<void*>(0x2000), 1}
    };
    msg.iov = hugeIov;
    msg.iovLength = 2;
    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "sendmsg with total iov length exceeding INT_MAX must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_sendmsg_nid_postfix(probeSocket, &msg, 0) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "_sendmsg with total iov length exceeding INT_MAX must fail with EINVAL (22)");

    char sampleData[] = "VALID";
    KernelIovec validIov{sampleData, sizeof(sampleData)};
    msg.iov = &validIov;
    msg.iovLength = 1;

    for (const int badSendFlag : {MsgPeek, MsgWaitAll, 0x1, 0x10, 0x100, 0x4000}) {
        *__error_nid_postfix() = 0;
        Require(sendmsg_nid_postfix(probeSocket, &msg, badSendFlag) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "sendmsg with invalid flags must fail with EOPNOTSUPP (45)");
        *__error_nid_postfix() = 0;
        Require(_sendmsg_nid_postfix(probeSocket, &msg, badSendFlag) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "_sendmsg with invalid flags must fail with EOPNOTSUPP (45)");
    }

    for (const int badRecvFlag : {MsgNoSignal, 0x1, 0x4, 0x8, 0x10, 0x20, 0x100}) {
        *__error_nid_postfix() = 0;
        Require(recvmsg_nid_postfix(probeSocket, &msg, badRecvFlag) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "recvmsg with invalid flags must fail with EOPNOTSUPP (45)");
        *__error_nid_postfix() = 0;
        Require(_recvmsg_nid_postfix(probeSocket, &msg, badRecvFlag) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "_recvmsg with invalid flags must fail with EOPNOTSUPP (45)");
    }

    for (const int badFd : {-100, -1, 9999}) {
        *__error_nid_postfix() = 0;
        Require(sendmsg_nid_postfix(badFd, &msg, 0) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "sendmsg on bad descriptor must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(_sendmsg_nid_postfix(badFd, &msg, 0) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "_sendmsg on bad descriptor must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(recvmsg_nid_postfix(badFd, &msg, 0) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "recvmsg on bad descriptor must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(_recvmsg_nid_postfix(badFd, &msg, 0) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "_recvmsg on bad descriptor must fail with EBADF (9)");
    }

    int pipeEndpoints[2] = {-1, -1};
    Require(pipe_nid_postfix(pipeEndpoints) == 0, "pipe creation failed");
    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(pipeEndpoints[1], &msg, 0) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "sendmsg on pipe must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(_sendmsg_nid_postfix(pipeEndpoints[1], &msg, 0) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "_sendmsg on pipe must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(recvmsg_nid_postfix(pipeEndpoints[0], &msg, 0) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "recvmsg on pipe must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(_recvmsg_nid_postfix(pipeEndpoints[0], &msg, 0) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "_recvmsg on pipe must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(pipeEndpoints[0]) == 0 && close_nid_postfix(pipeEndpoints[1]) == 0, "close pipe failed");
    Require(close_nid_postfix(probeSocket) == 0, "close probe socket failed");

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "tcp listener creation failed");
    std::array<std::uint8_t, 16> listenAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, listenAddr.data(), listenAddr.size()) == 0, "bind listener failed");
    std::uint32_t listenLen = listenAddr.size();
    Require(getsockname_nid_postfix(listener, listenAddr.data(), &listenLen) == 0 && listenLen == 16,
        "getsockname listener failed");
    Require(listen_nid_postfix(listener, 5) == 0, "listen failed");

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0, "tcp client creation failed");
    Require(connect_nid_postfix(client, listenAddr.data(), listenAddr.size()) == 0, "connect failed");
    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0, "accept failed");

    char sendChunk1[] = "ABCD";
    char sendChunk2[] = "EFGH";
    char sendChunk3[] = "IJKL";
    KernelIovec scatterIov[3] = {
        {sendChunk1, 4},
        {sendChunk2, 4},
        {sendChunk3, 4}
    };
    GuestMsghdr sendMsg{};
    sendMsg.iov = scatterIov;
    sendMsg.iovLength = 3;
    Require(sendmsg_nid_postfix(client, &sendMsg, 0) == 12, "sendmsg scatter across 3 iovecs failed");

    char recvChunk1[2]{};
    char recvChunk2[6]{};
    char recvChunk3[4]{};
    KernelIovec gatherIov[3] = {
        {recvChunk1, 2},
        {recvChunk2, 6},
        {recvChunk3, 4}
    };
    GuestMsghdr recvMsg{};
    recvMsg.iov = gatherIov;
    recvMsg.iovLength = 3;
    Require(recvmsg_nid_postfix(server, &recvMsg, MsgWaitAll) == 12, "recvmsg gather across 3 iovecs failed");
    Require(std::memcmp(recvChunk1, "AB", 2) == 0, "recvChunk1 content mismatch");
    Require(std::memcmp(recvChunk2, "CDEFGH", 6) == 0, "recvChunk2 content mismatch");
    Require(std::memcmp(recvChunk3, "IJKL", 4) == 0, "recvChunk3 content mismatch");

    char twinSend[] = "TWIN";
    KernelIovec twinSendIov{twinSend, 4};
    GuestMsghdr twinSendMsg{};
    twinSendMsg.iov = &twinSendIov;
    twinSendMsg.iovLength = 1;
    Require(_sendmsg_nid_postfix(server, &twinSendMsg, MsgNoSignal) == 4, "_sendmsg failed");

    char twinRecv[8]{};
    KernelIovec twinRecvIov{twinRecv, sizeof(twinRecv)};
    GuestMsghdr twinRecvMsg{};
    twinRecvMsg.iov = &twinRecvIov;
    twinRecvMsg.iovLength = 1;
    Require(_recvmsg_nid_postfix(client, &twinRecvMsg, 0) == 4, "_recvmsg failed");
    Require(std::memcmp(twinRecv, "TWIN", 4) == 0, "twinRecv content mismatch");

    Require(close_nid_postfix(server) == 0 && close_nid_postfix(client) == 0 &&
        close_nid_postfix(listener) == 0, "close tcp sockets failed");

    const int udpServer = socket_nid_postfix(Inet, Datagram, 0);
    const int udpClient = socket_nid_postfix(Inet, Datagram, 0);
    Require(udpServer >= 0 && udpClient >= 0, "udp socket creation failed");

    int optionOne = 1;
    Require(setsockopt_nid_postfix(udpServer, SolSocket, SoTimestamp, &optionOne, sizeof(optionOne)) == 0,
        "setsockopt SO_TIMESTAMP failed");
    Require(setsockopt_nid_postfix(udpServer, SolSocket, SoBintime, &optionOne, sizeof(optionOne)) == 0,
        "setsockopt SO_BINTIME failed");

    std::array<std::uint8_t, 16> udpBindAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(udpServer, udpBindAddr.data(), udpBindAddr.size()) == 0, "bind udpServer failed");
    std::uint32_t udpBindLen = udpBindAddr.size();
    Require(getsockname_nid_postfix(udpServer, udpBindAddr.data(), &udpBindLen) == 0 && udpBindLen == 16,
        "getsockname udpServer failed");

    char udpPacket[] = "TIMESTAMP_TEST_PACKET";
    KernelIovec udpSendIov{udpPacket, sizeof(udpPacket)};
    GuestMsghdr udpSendMsg{};
    udpSendMsg.name = udpBindAddr.data();
    udpSendMsg.nameLength = udpBindLen;
    udpSendMsg.iov = &udpSendIov;
    udpSendMsg.iovLength = 1;
    Require(sendmsg_nid_postfix(udpClient, &udpSendMsg, 0) == static_cast<std::int64_t>(sizeof(udpPacket)),
        "sendmsg udp packet failed");

    std::array<char, 64> dgramRecvBuf{};
    KernelIovec dgramRecvIov{dgramRecvBuf.data(), dgramRecvBuf.size()};
    std::array<char, 64> fullControlBuf{};
    GuestMsghdr fullRecvMsg{};
    fullRecvMsg.iov = &dgramRecvIov;
    fullRecvMsg.iovLength = 1;
    fullRecvMsg.control = fullControlBuf.data();
    fullRecvMsg.controlLength = static_cast<std::uint32_t>(fullControlBuf.size());
    Require(recvmsg_nid_postfix(udpServer, &fullRecvMsg, 0) == static_cast<std::int64_t>(sizeof(udpPacket)),
        "recvmsg udp with full control buffer failed");
    Require(fullRecvMsg.controlLength == 64, "full controlLength must be exactly 64 bytes for bintime + timestamp");
    Require((fullRecvMsg.flags & MsgCtrun) == 0, "full control buffer must not have MSG_CTRUNC set");

    Require(sendmsg_nid_postfix(udpClient, &udpSendMsg, 0) == static_cast<std::int64_t>(sizeof(udpPacket)),
        "sendmsg udp second packet failed");

    std::array<char, 32> truncatedControlBuf{};
    GuestMsghdr truncRecvMsg{};
    truncRecvMsg.iov = &dgramRecvIov;
    truncRecvMsg.iovLength = 1;
    truncRecvMsg.control = truncatedControlBuf.data();
    truncRecvMsg.controlLength = static_cast<std::uint32_t>(truncatedControlBuf.size());
    Require(recvmsg_nid_postfix(udpServer, &truncRecvMsg, 0) == static_cast<std::int64_t>(sizeof(udpPacket)),
        "recvmsg udp with truncated control buffer failed");
    Require(truncRecvMsg.controlLength == 32, "truncated controlLength must equal buffer capacity 32");
    Require((truncRecvMsg.flags & MsgCtrun) == MsgCtrun, "truncated control buffer must set MSG_CTRUNC (0x20)");

    Require(sendmsg_nid_postfix(udpClient, &udpSendMsg, 0) == static_cast<std::int64_t>(sizeof(udpPacket)),
        "sendmsg udp third packet failed");

    GuestMsghdr zeroCtrlRecvMsg{};
    zeroCtrlRecvMsg.iov = &dgramRecvIov;
    zeroCtrlRecvMsg.iovLength = 1;
    zeroCtrlRecvMsg.control = nullptr;
    zeroCtrlRecvMsg.controlLength = 0;
    Require(recvmsg_nid_postfix(udpServer, &zeroCtrlRecvMsg, 0) == static_cast<std::int64_t>(sizeof(udpPacket)),
        "recvmsg udp with zero control length failed");
    Require(zeroCtrlRecvMsg.controlLength == 0, "zero controlLength must remain 0");

    char largeDgramData[] = "LARGE_PAYLOAD_FOR_TRUNCATION_TEST";
    KernelIovec largeSendIov{largeDgramData, sizeof(largeDgramData)};
    GuestMsghdr largeSendMsg{};
    largeSendMsg.name = udpBindAddr.data();
    largeSendMsg.nameLength = udpBindLen;
    largeSendMsg.iov = &largeSendIov;
    largeSendMsg.iovLength = 1;
    Require(sendmsg_nid_postfix(udpClient, &largeSendMsg, 0) == static_cast<std::int64_t>(sizeof(largeDgramData)),
        "sendmsg large udp packet failed");

    char smallRecvBuf[8]{};
    KernelIovec smallRecvIov{smallRecvBuf, sizeof(smallRecvBuf)};
    GuestMsghdr truncDataRecvMsg{};
    truncDataRecvMsg.iov = &smallRecvIov;
    truncDataRecvMsg.iovLength = 1;
    Require(recvmsg_nid_postfix(udpServer, &truncDataRecvMsg, 0) == static_cast<std::int64_t>(sizeof(smallRecvBuf)),
        "recvmsg with short data buffer failed");
    Require((truncDataRecvMsg.flags & MsgTrunc) == MsgTrunc, "truncated datagram data must set MSG_TRUNC (0x10)");

    Require(close_nid_postfix(udpServer) == 0 && close_nid_postfix(udpClient) == 0, "close udp sockets failed");

    *__error_nid_postfix() = 0;
    Require(sendmsg_nid_postfix(udpClient, &udpSendMsg, 0) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "sendmsg on closed socket must fail with EBADF (9)");
    *__error_nid_postfix() = 0;
    Require(recvmsg_nid_postfix(udpServer, &zeroCtrlRecvMsg, 0) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "recvmsg on closed socket must fail with EBADF (9)");

    return 0;
}
