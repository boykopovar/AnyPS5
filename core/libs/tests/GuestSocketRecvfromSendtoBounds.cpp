#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Datagram = 2;

constexpr int MsgPeek = 0x2;
constexpr int MsgWaitAll = 0x40;
constexpr int MsgDontWait = 0x80;
constexpr int MsgNoSignal = 0x20000;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorWouldBlock = 35;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorDestAddrReq = 39;
constexpr int ErrorMessageSize = 40;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorNotConnected = 57;
constexpr int ErrorNameTooLong = 63;

void Require(bool condition, const char* what, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Require failed at line %u: %s (errno %d)\n", static_cast<unsigned>(location.line()), what, *__error_nid_postfix());
    std::fflush(stderr);
    std::abort();
}

} // namespace

int main() {
    const int probeSocket = socket_nid_postfix(Inet, Stream, 0);
    Require(probeSocket >= 0, "socket probe failed");

    std::array<char, 16> sampleBuffer{};
    for (const int invalidSendFlag : {MsgPeek, MsgWaitAll, 0x1, 0x10, 0x100, 0x4000, 0x10000}) {
        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), invalidSendFlag, nullptr, 0) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "sendto with unsupported flags must fail with EOPNOTSUPP (45)");
        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), invalidSendFlag, nullptr, 0) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "_sendto with unsupported flags must fail with EOPNOTSUPP (45)");
    }

    for (const int invalidRecvFlag : {MsgNoSignal, 0x1, 0x4, 0x8, 0x10, 0x20, 0x100, 0x4000}) {
        *__error_nid_postfix() = 0;
        Require(recvfrom_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), invalidRecvFlag, nullptr, nullptr) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "recvfrom with unsupported flags must fail with EOPNOTSUPP (45)");
        *__error_nid_postfix() = 0;
        Require(_recvfrom_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), invalidRecvFlag, nullptr, nullptr) == -1 &&
            *__error_nid_postfix() == ErrorNotSupported, "_recvfrom with unsupported flags must fail with EOPNOTSUPP (45)");
    }

    const std::uint64_t overLimitLength = static_cast<std::uint64_t>(INT_MAX) + 1ull;
    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, sampleBuffer.data(), overLimitLength, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorMessageSize, "sendto with length above INT_MAX must fail with EMSGSIZE (40)");
    *__error_nid_postfix() = 0;
    Require(_sendto_nid_postfix(probeSocket, sampleBuffer.data(), overLimitLength, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorMessageSize, "_sendto with length above INT_MAX must fail with EMSGSIZE (40)");
    *__error_nid_postfix() = 0;
    Require(recvfrom_nid_postfix(probeSocket, sampleBuffer.data(), overLimitLength, 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorMessageSize, "recvfrom with length above INT_MAX must fail with EMSGSIZE (40)");
    *__error_nid_postfix() = 0;
    Require(_recvfrom_nid_postfix(probeSocket, sampleBuffer.data(), overLimitLength, 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorMessageSize, "_recvfrom with length above INT_MAX must fail with EMSGSIZE (40)");

    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, nullptr, 8, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorNotConnected, "sendto with null buffer on unconnected stream must fail with ENOTCONN (57)");
    *__error_nid_postfix() = 0;
    Require(_sendto_nid_postfix(probeSocket, nullptr, 8, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorNotConnected, "_sendto with null buffer on unconnected stream must fail with ENOTCONN (57)");

    std::array<std::uint8_t, 16> loopbackAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, nullptr, 8, 0, loopbackAddr.data(), loopbackAddr.size()) == -1 &&
        *__error_nid_postfix() == ErrorFault, "sendto with null buffer and non-zero length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_sendto_nid_postfix(probeSocket, nullptr, 8, 0, loopbackAddr.data(), loopbackAddr.size()) == -1 &&
        *__error_nid_postfix() == ErrorFault, "_sendto with null buffer and non-zero length must fail with EFAULT (14)");

    *__error_nid_postfix() = 0;
    Require(recvfrom_nid_postfix(probeSocket, nullptr, 8, 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorFault, "recvfrom with null buffer and non-zero length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_recvfrom_nid_postfix(probeSocket, nullptr, 8, 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorFault, "_recvfrom with null buffer and non-zero length must fail with EFAULT (14)");


    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), 0, loopbackAddr.data(), 1) == -1 &&
        *__error_nid_postfix() == ErrorInvalid, "sendto with addressLength below 2 must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), 0, loopbackAddr.data(), 1) == -1 &&
        *__error_nid_postfix() == ErrorInvalid, "_sendto with addressLength below 2 must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), 0, loopbackAddr.data(), 8) == -1 &&
        *__error_nid_postfix() == ErrorInvalid, "sendto with IPv4 address shorter than 16 bytes must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), 0, loopbackAddr.data(), 256) == -1 &&
        *__error_nid_postfix() == ErrorNameTooLong, "sendto with addressLength above 255 must fail with ENAMETOOLONG (63)");

    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorNotConnected, "sendto on unconnected stream socket must fail with ENOTCONN (57)");
    *__error_nid_postfix() = 0;
    Require(_sendto_nid_postfix(probeSocket, sampleBuffer.data(), sampleBuffer.size(), 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorNotConnected, "_sendto on unconnected stream socket must fail with ENOTCONN (57)");

    for (const int badDescriptor : {-100, -1, 9999}) {
        *__error_nid_postfix() = 0;
        Require(sendto_nid_postfix(badDescriptor, sampleBuffer.data(), 4, 0, nullptr, 0) == -1 &&
            *__error_nid_postfix() == ErrorBadDescriptor, "sendto on bad fd must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(_sendto_nid_postfix(badDescriptor, sampleBuffer.data(), 4, 0, nullptr, 0) == -1 &&
            *__error_nid_postfix() == ErrorBadDescriptor, "_sendto on bad fd must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(recvfrom_nid_postfix(badDescriptor, sampleBuffer.data(), 4, 0, nullptr, nullptr) == -1 &&
            *__error_nid_postfix() == ErrorBadDescriptor, "recvfrom on bad fd must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(_recvfrom_nid_postfix(badDescriptor, sampleBuffer.data(), 4, 0, nullptr, nullptr) == -1 &&
            *__error_nid_postfix() == ErrorBadDescriptor, "_recvfrom on bad fd must fail with EBADF (9)");
    }

    int pipeEndpoints[2] = {-1, -1};
    Require(pipe_nid_postfix(pipeEndpoints) == 0, "pipe failed");
    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(pipeEndpoints[1], sampleBuffer.data(), 4, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorNotSocket, "sendto on pipe must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(_sendto_nid_postfix(pipeEndpoints[1], sampleBuffer.data(), 4, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorNotSocket, "_sendto on pipe must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(recvfrom_nid_postfix(pipeEndpoints[0], sampleBuffer.data(), 4, 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorNotSocket, "recvfrom on pipe must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(_recvfrom_nid_postfix(pipeEndpoints[0], sampleBuffer.data(), 4, 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorNotSocket, "_recvfrom on pipe must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(pipeEndpoints[0]) == 0 && close_nid_postfix(pipeEndpoints[1]) == 0, "close pipe failed");
    Require(close_nid_postfix(probeSocket) == 0, "close probe socket failed");

    const int udpServer = socket_nid_postfix(Inet, Datagram, 0);
    const int udpClient = socket_nid_postfix(Inet, Datagram, 0);
    Require(udpServer >= 0 && udpClient >= 0, "udp socket creation failed");

    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(udpClient, sampleBuffer.data(), 4, 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorDestAddrReq, "sendto on unconnected UDP socket without address must fail with EDESTADDRREQ (39)");

    std::array<std::uint8_t, 16> serverBindAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(udpServer, serverBindAddr.data(), serverBindAddr.size()) == 0, "bind udpServer failed");
    std::uint32_t serverBindLen = serverBindAddr.size();
    Require(getsockname_nid_postfix(udpServer, serverBindAddr.data(), &serverBindLen) == 0 && serverBindLen == 16,
        "getsockname udpServer failed");

    std::array<std::uint8_t, 16> clientBindAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(udpClient, clientBindAddr.data(), clientBindAddr.size()) == 0, "bind udpClient failed");
    std::uint32_t clientBindLen = clientBindAddr.size();
    Require(getsockname_nid_postfix(udpClient, clientBindAddr.data(), &clientBindLen) == 0 && clientBindLen == 16,
        "getsockname udpClient failed");

    const std::array<char, 4> packetData{'U', 'D', 'P', '1'};
    Require(sendto_nid_postfix(udpClient, packetData.data(), packetData.size(), 0, serverBindAddr.data(),
        serverBindAddr.size()) == static_cast<std::int64_t>(packetData.size()), "udp sendto packet 1 failed");

    std::array<char, 8> receivePacket{};
    std::array<std::uint8_t, 16> senderAddr{};
    std::uint32_t senderLen = senderAddr.size();
    Require(recvfrom_nid_postfix(udpServer, receivePacket.data(), receivePacket.size(), MsgPeek,
        senderAddr.data(), &senderLen) == static_cast<std::int64_t>(packetData.size()), "udp recvfrom peek failed");
    Require(std::memcmp(receivePacket.data(), packetData.data(), packetData.size()) == 0, "peeked udp data mismatch");
    Require(senderLen == 16, "peeked senderLen must be 16");

    receivePacket.fill(0);
    std::array<std::uint8_t, 16> shortSenderAddr{};
    shortSenderAddr.fill(0xEE);
    std::uint32_t shortSenderLen = 6;
    Require(recvfrom_nid_postfix(udpServer, receivePacket.data(), receivePacket.size(), 0,
        shortSenderAddr.data(), &shortSenderLen) == static_cast<std::int64_t>(packetData.size()), "udp recvfrom consume failed");
    Require(std::memcmp(receivePacket.data(), packetData.data(), packetData.size()) == 0, "consumed udp data mismatch");
    Require(shortSenderLen == 6, "truncated sender length must be 6");
    Require(shortSenderAddr[0] == 16 && shortSenderAddr[1] == Inet, "short address must have correct header");
    Require(shortSenderAddr[6] == 0xEE && shortSenderAddr[15] == 0xEE, "bytes beyond short length must be preserved");

    const std::array<char, 4> packet2{'U', 'D', 'P', '2'};
    Require(_sendto_nid_postfix(udpClient, packet2.data(), packet2.size(), MsgDontWait, serverBindAddr.data(),
        serverBindAddr.size()) == static_cast<std::int64_t>(packet2.size()), "udp _sendto packet 2 failed");

    receivePacket.fill(0);
    std::array<std::uint8_t, 16> zeroSenderAddr{};
    zeroSenderAddr.fill(0xCC);
    std::uint32_t zeroSenderLen = 0;
    Require(_recvfrom_nid_postfix(udpServer, receivePacket.data(), receivePacket.size(), 0,
        zeroSenderAddr.data(), &zeroSenderLen) == static_cast<std::int64_t>(packet2.size()), "udp _recvfrom with zero addressLen failed");
    Require(std::memcmp(receivePacket.data(), packet2.data(), packet2.size()) == 0, "zero addressLen packet data mismatch");
    Require(zeroSenderLen == 0, "zero addressLen must remain 0");
    Require(zeroSenderAddr[0] == 0xCC && zeroSenderAddr[15] == 0xCC, "zero address buffer must not be overwritten");

    const std::array<char, 4> packet3{'U', 'D', 'P', '3'};
    Require(sendto_nid_postfix(udpClient, packet3.data(), packet3.size(), MsgNoSignal, serverBindAddr.data(),
        serverBindAddr.size()) == static_cast<std::int64_t>(packet3.size()), "udp sendto packet 3 failed");

    receivePacket.fill(0);
    senderAddr.fill(0);
    senderLen = senderAddr.size();
    Require(recvfrom_nid_postfix(udpServer, receivePacket.data(), receivePacket.size(), 0,
        senderAddr.data(), &senderLen) == static_cast<std::int64_t>(packet3.size()), "udp recvfrom full address failed");
    Require(senderLen == 16, "full sender address length must be 16");
    Require(senderAddr[0] == 16 && senderAddr[1] == Inet, "full sender address header mismatch");
    Require(senderAddr[2] == clientBindAddr[2] && senderAddr[3] == clientBindAddr[3], "sender port must match client port");
    Require(senderAddr[4] == 127 && senderAddr[5] == 0 && senderAddr[6] == 0 && senderAddr[7] == 1,
        "sender IP must be 127.0.0.1");

    *__error_nid_postfix() = 0;
    Require(recvfrom_nid_postfix(udpServer, receivePacket.data(), receivePacket.size(), MsgDontWait, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorWouldBlock, "recvfrom with MSG_DONTWAIT on empty queue must fail with EWOULDBLOCK (35)");
    *__error_nid_postfix() = 0;
    Require(_recvfrom_nid_postfix(udpServer, receivePacket.data(), receivePacket.size(), MsgDontWait, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorWouldBlock, "_recvfrom with MSG_DONTWAIT on empty queue must fail with EWOULDBLOCK (35)");

    Require(close_nid_postfix(udpServer) == 0 && close_nid_postfix(udpClient) == 0, "close udp sockets failed");

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "tcp listener creation failed");
    std::array<std::uint8_t, 16> tcpListenAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, tcpListenAddr.data(), tcpListenAddr.size()) == 0, "bind tcp listener failed");
    std::uint32_t tcpListenLen = tcpListenAddr.size();
    Require(getsockname_nid_postfix(listener, tcpListenAddr.data(), &tcpListenLen) == 0 && tcpListenLen == 16,
        "getsockname tcp listener failed");
    Require(listen_nid_postfix(listener, 5) == 0, "listen tcp listener failed");

    const int streamClient = socket_nid_postfix(Inet, Stream, 0);
    Require(streamClient >= 0, "tcp client creation failed");
    Require(connect_nid_postfix(streamClient, tcpListenAddr.data(), tcpListenAddr.size()) == 0, "tcp connect failed");
    const int streamServer = accept_nid_postfix(listener, nullptr, nullptr);
    Require(streamServer >= 0, "tcp accept failed");

    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(streamClient, sampleBuffer.data(), 4, 0, nullptr, 16) == -1 &&
        *__error_nid_postfix() == ErrorInvalid, "sendto with null address and non-zero length on stream must fail with EINVAL (22)");

    const std::array<char, 8> streamData{'S', 'T', 'R', 'E', 'A', 'M', '0', '1'};
    Require(sendto_nid_postfix(streamClient, streamData.data(), streamData.size(), 0, nullptr, 0) ==
        static_cast<std::int64_t>(streamData.size()), "stream sendto without address failed");

    std::array<char, 8> streamReceive{};
    Require(recvfrom_nid_postfix(streamServer, streamReceive.data(), streamReceive.size(), MsgPeek, nullptr, nullptr) ==
        static_cast<std::int64_t>(streamData.size()), "stream recvfrom MsgPeek failed");
    Require(std::memcmp(streamReceive.data(), streamData.data(), streamData.size()) == 0, "stream peeked data mismatch");

    streamReceive.fill(0);
    Require(recvfrom_nid_postfix(streamServer, streamReceive.data(), streamReceive.size(), MsgWaitAll, nullptr, nullptr) ==
        static_cast<std::int64_t>(streamData.size()), "stream recvfrom MsgWaitAll failed");
    Require(std::memcmp(streamReceive.data(), streamData.data(), streamData.size()) == 0, "stream received data mismatch");

    const std::array<char, 4> serverReply{'P', 'O', 'N', 'G'};
    Require(_sendto_nid_postfix(streamServer, serverReply.data(), serverReply.size(), MsgNoSignal, nullptr, 0) ==
        static_cast<std::int64_t>(serverReply.size()), "stream _sendto server reply failed");

    std::array<char, 4> clientReceive{};
    Require(_recvfrom_nid_postfix(streamClient, clientReceive.data(), clientReceive.size(), 0, nullptr, nullptr) ==
        static_cast<std::int64_t>(serverReply.size()), "stream _recvfrom client reply failed");
    Require(std::memcmp(clientReceive.data(), serverReply.data(), serverReply.size()) == 0, "stream client reply mismatch");

    *__error_nid_postfix() = 0;
    Require(recvfrom_nid_postfix(streamServer, streamReceive.data(), streamReceive.size(), MsgDontWait, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorWouldBlock, "stream recvfrom with MSG_DONTWAIT on empty queue must fail with EWOULDBLOCK (35)");
    *__error_nid_postfix() = 0;
    Require(_recvfrom_nid_postfix(streamClient, clientReceive.data(), clientReceive.size(), MsgDontWait, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorWouldBlock, "stream _recvfrom with MSG_DONTWAIT on empty queue must fail with EWOULDBLOCK (35)");

    Require(close_nid_postfix(streamServer) == 0 && close_nid_postfix(streamClient) == 0 &&
        close_nid_postfix(listener) == 0, "close tcp sockets failed");

    *__error_nid_postfix() = 0;
    Require(sendto_nid_postfix(streamClient, streamData.data(), streamData.size(), 0, nullptr, 0) == -1 &&
        *__error_nid_postfix() == ErrorBadDescriptor, "sendto on closed stream socket must fail with EBADF (9)");
    *__error_nid_postfix() = 0;
    Require(recvfrom_nid_postfix(streamServer, streamReceive.data(), streamReceive.size(), 0, nullptr, nullptr) == -1 &&
        *__error_nid_postfix() == ErrorBadDescriptor, "recvfrom on closed stream socket must fail with EBADF (9)");

    return 0;
}
