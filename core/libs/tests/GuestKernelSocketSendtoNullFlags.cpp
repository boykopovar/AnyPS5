#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Dgram = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorWouldBlock = 35;
constexpr int ErrorDestAddressRequired = 39;
constexpr int ErrorMessageTooLong = 40;
constexpr int ErrorOpNotSupported = 45;

constexpr int MsgOob = 0x1;
constexpr int MsgPeek = 0x2;
constexpr int MsgDontRoute = 0x4;
constexpr int MsgWaitAll = 0x40;
constexpr int MsgDontWait = 0x80;
constexpr int MsgNoSignal = 0x20000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
    std::abort();
}

bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

}

int main() {
    char scratchBuffer[64]{};

    Require(Fails(sendto_nid_postfix(-1, "a", 1, 0, nullptr, 0), ErrorBadDescriptor), "sendto on bad fd must fail with EBADF");
    Require(Fails(_sendto_nid_postfix(-1, "a", 1, 0, nullptr, 0), ErrorBadDescriptor), "_sendto on bad fd must fail with EBADF");
    Require(Fails(recvfrom_nid_postfix(-1, scratchBuffer, 1, 0, nullptr, nullptr), ErrorBadDescriptor), "recvfrom on bad fd must fail with EBADF");
    Require(Fails(_recvfrom_nid_postfix(-1, scratchBuffer, 1, 0, nullptr, nullptr), ErrorBadDescriptor), "_recvfrom on bad fd must fail with EBADF");

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket listener failed");
    std::array<std::uint8_t, 16> streamAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, streamAddr.data(), streamAddr.size()) == 0, "bind listener failed");
    Require(listen_nid_postfix(listener, 4) == 0, "listen failed");

    std::uint32_t streamAddrSize = streamAddr.size();
    Require(getsockname_nid_postfix(listener, streamAddr.data(), &streamAddrSize) == 0, "getsockname listener failed");

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0, "socket client failed");
    Require(connect_nid_postfix(client, streamAddr.data(), streamAddr.size()) == 0, "connect failed");

    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");

    const int udpServer = socket_nid_postfix(Inet, Dgram, 0);
    Require(udpServer >= 0, "socket udpServer failed");
    std::array<std::uint8_t, 16> udpAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(udpServer, udpAddr.data(), udpAddr.size()) == 0, "bind udpServer failed");
    std::uint32_t udpAddrSize = udpAddr.size();
    Require(getsockname_nid_postfix(udpServer, udpAddr.data(), &udpAddrSize) == 0, "getsockname udpServer failed");

    const int udpClient = socket_nid_postfix(Inet, Dgram, 0);
    Require(udpClient >= 0, "socket udpClient failed");

    Require(Fails(sendto_nid_postfix(client, "a", static_cast<std::uint64_t>(-1), 0, nullptr, 0), ErrorMessageTooLong), "sendto with negative length must fail with EMSGSIZE");
    Require(Fails(sendto_nid_postfix(client, "a", static_cast<std::uint64_t>(-100), 0, nullptr, 0), ErrorMessageTooLong), "sendto with -100 length must fail with EMSGSIZE");
    Require(Fails(_sendto_nid_postfix(client, "a", static_cast<std::uint64_t>(-1), 0, nullptr, 0), ErrorMessageTooLong), "_sendto with negative length must fail with EMSGSIZE");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, static_cast<std::uint64_t>(-1), 0, nullptr, nullptr), ErrorMessageTooLong), "recvfrom with negative length must fail with EMSGSIZE");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, static_cast<std::uint64_t>(-100), 0, nullptr, nullptr), ErrorMessageTooLong), "recvfrom with -100 length must fail with EMSGSIZE");
    Require(Fails(_recvfrom_nid_postfix(client, scratchBuffer, static_cast<std::uint64_t>(-1), 0, nullptr, nullptr), ErrorMessageTooLong), "_recvfrom with negative length must fail with EMSGSIZE");
    Require(Fails(sendto_nid_postfix(udpClient, "a", static_cast<std::uint64_t>(-1), 0, udpAddr.data(), udpAddrSize), ErrorMessageTooLong), "udp sendto with negative length must fail with EMSGSIZE");
    Require(Fails(recvfrom_nid_postfix(udpServer, scratchBuffer, static_cast<std::uint64_t>(-1), 0, nullptr, nullptr), ErrorMessageTooLong), "udp recvfrom with negative length must fail with EMSGSIZE");

    Require(Fails(sendto_nid_postfix(client, "a", 1, MsgPeek, nullptr, 0), ErrorOpNotSupported), "sendto with MSG_PEEK must fail with EOPNOTSUPP");
    Require(Fails(sendto_nid_postfix(client, "a", 1, MsgWaitAll, nullptr, 0), ErrorOpNotSupported), "sendto with MSG_WAITALL must fail with EOPNOTSUPP");
    Require(Fails(sendto_nid_postfix(client, "a", 1, MsgOob, nullptr, 0), ErrorOpNotSupported), "sendto with MSG_OOB must fail with EOPNOTSUPP");
    Require(Fails(sendto_nid_postfix(client, "a", 1, MsgDontRoute, nullptr, 0), ErrorOpNotSupported), "sendto with MSG_DONTROUTE must fail with EOPNOTSUPP");
    Require(Fails(sendto_nid_postfix(client, "a", 1, 0x100, nullptr, 0), ErrorOpNotSupported), "sendto with 0x100 must fail with EOPNOTSUPP");
    Require(Fails(sendto_nid_postfix(client, "a", 1, -1, nullptr, 0), ErrorOpNotSupported), "sendto with -1 flags must fail with EOPNOTSUPP");
    Require(Fails(_sendto_nid_postfix(client, "a", 1, MsgPeek, nullptr, 0), ErrorOpNotSupported), "_sendto with MSG_PEEK must fail with EOPNOTSUPP");
    Require(Fails(_sendto_nid_postfix(client, "a", 1, -1, nullptr, 0), ErrorOpNotSupported), "_sendto with -1 flags must fail with EOPNOTSUPP");

    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), MsgNoSignal, nullptr, nullptr), ErrorOpNotSupported), "recvfrom with MSG_NOSIGNAL must fail with EOPNOTSUPP");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), MsgOob, nullptr, nullptr), ErrorOpNotSupported), "recvfrom with MSG_OOB must fail with EOPNOTSUPP");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), MsgDontRoute, nullptr, nullptr), ErrorOpNotSupported), "recvfrom with MSG_DONTROUTE must fail with EOPNOTSUPP");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), 0x100, nullptr, nullptr), ErrorOpNotSupported), "recvfrom with 0x100 must fail with EOPNOTSUPP");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), -1, nullptr, nullptr), ErrorOpNotSupported), "recvfrom with -1 flags must fail with EOPNOTSUPP");
    Require(Fails(_recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), MsgNoSignal, nullptr, nullptr), ErrorOpNotSupported), "_recvfrom with MSG_NOSIGNAL must fail with EOPNOTSUPP");
    Require(Fails(_recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), -1, nullptr, nullptr), ErrorOpNotSupported), "_recvfrom with -1 flags must fail with EOPNOTSUPP");

    Require(Fails(sendto_nid_postfix(client, nullptr, 1, 0, nullptr, 0), ErrorFault), "sendto with null buffer must fail with EFAULT");
    Require(Fails(_sendto_nid_postfix(client, nullptr, 1, 0, nullptr, 0), ErrorFault), "_sendto with null buffer must fail with EFAULT");
    Require(Fails(sendto_nid_postfix(client, reinterpret_cast<const void*>(0x10), 1, 0, nullptr, 0), ErrorFault), "sendto with unmapped buffer must fail with EFAULT");
    Require(Fails(sendto_nid_postfix(client, "a", 1, 0, nullptr, 16), ErrorInvalid), "sendto with null address and non-zero length must fail with EINVAL");
    Require(Fails(_sendto_nid_postfix(client, "a", 1, 0, nullptr, 16), ErrorInvalid), "_sendto with null address and non-zero length must fail with EINVAL");
    Require(Fails(sendto_nid_postfix(udpClient, "a", 1, 0, udpAddr.data(), 0), ErrorInvalid), "udp sendto with address length 0 must fail with EINVAL");
    Require(Fails(sendto_nid_postfix(udpClient, "a", 1, 0, udpAddr.data(), 1), ErrorInvalid), "udp sendto with address length 1 must fail with EINVAL");
    Require(Fails(sendto_nid_postfix(udpClient, "a", 1, 0, reinterpret_cast<const void*>(0x10), 16), ErrorFault), "udp sendto with unmapped address must fail with EFAULT");
    Require(Fails(sendto_nid_postfix(udpClient, "a", 1, 0, nullptr, 0), ErrorDestAddressRequired), "unconnected udp sendto without address must fail with EDESTADDRREQ");
    Require(sendto_nid_postfix(client, nullptr, 0, 0, nullptr, 0) == 0, "sendto with 0 length and null buffer must return 0");

    Require(Fails(recvfrom_nid_postfix(client, nullptr, 1, 0, nullptr, nullptr), ErrorFault), "recvfrom with null buffer must fail with EFAULT");
    Require(Fails(_recvfrom_nid_postfix(client, nullptr, 1, 0, nullptr, nullptr), ErrorFault), "_recvfrom with null buffer must fail with EFAULT");
    Require(Fails(recvfrom_nid_postfix(client, reinterpret_cast<void*>(0x10), 1, 0, nullptr, nullptr), ErrorFault), "recvfrom with unmapped buffer must fail with EFAULT");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), 0, udpAddr.data(), nullptr), ErrorFault), "recvfrom with address but null length pointer must fail with EFAULT");
    Require(Fails(_recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), 0, udpAddr.data(), nullptr), ErrorFault), "_recvfrom with address but null length pointer must fail with EFAULT");
    Require(Fails(recvfrom_nid_postfix(client, scratchBuffer, sizeof(scratchBuffer), 0, udpAddr.data(), reinterpret_cast<std::uint32_t*>(0x10)), ErrorFault), "recvfrom with unmapped length pointer must fail with EFAULT");

    Require(Fails(recvfrom_nid_postfix(accepted, scratchBuffer, sizeof(scratchBuffer), MsgDontWait, nullptr, nullptr), ErrorWouldBlock), "empty stream recvfrom with MSG_DONTWAIT must return EWOULDBLOCK");
    Require(Fails(_recvfrom_nid_postfix(accepted, scratchBuffer, sizeof(scratchBuffer), MsgDontWait, nullptr, nullptr), ErrorWouldBlock), "empty stream _recvfrom with MSG_DONTWAIT must return EWOULDBLOCK");
    Require(Fails(recvfrom_nid_postfix(udpServer, scratchBuffer, sizeof(scratchBuffer), MsgDontWait, nullptr, nullptr), ErrorWouldBlock), "empty udp recvfrom with MSG_DONTWAIT must return EWOULDBLOCK");
    Require(Fails(_recvfrom_nid_postfix(udpServer, scratchBuffer, sizeof(scratchBuffer), MsgDontWait, nullptr, nullptr), ErrorWouldBlock), "empty udp _recvfrom with MSG_DONTWAIT must return EWOULDBLOCK");

    const char streamMsg[] = "stream_payload_audit";
    Require(sendto_nid_postfix(client, streamMsg, sizeof(streamMsg), MsgDontWait, nullptr, 0) == sizeof(streamMsg), "stream sendto with MSG_DONTWAIT must succeed");

    char streamPeek[32]{};
    Require(recvfrom_nid_postfix(accepted, streamPeek, sizeof(streamPeek), MsgPeek, nullptr, nullptr) == sizeof(streamMsg), "stream recvfrom with MSG_PEEK must succeed");
    Require(std::strcmp(streamPeek, streamMsg) == 0, "stream peeked content must match original");

    char streamNormal[32]{};
    Require(recvfrom_nid_postfix(accepted, streamNormal, sizeof(streamNormal), 0, nullptr, nullptr) == sizeof(streamMsg), "stream recvfrom without flags must read peeked data");
    Require(std::strcmp(streamNormal, streamMsg) == 0, "stream consumed content must match original");

    Require(Fails(recvfrom_nid_postfix(accepted, streamPeek, sizeof(streamPeek), MsgPeek | MsgDontWait, nullptr, nullptr), ErrorWouldBlock), "empty stream recvfrom with MSG_PEEK | MSG_DONTWAIT must return EWOULDBLOCK");

    const char udpMsg[] = "udp_payload_audit";
    Require(_sendto_nid_postfix(udpClient, udpMsg, sizeof(udpMsg), MsgDontWait, udpAddr.data(), udpAddrSize) == sizeof(udpMsg), "udp _sendto with MSG_DONTWAIT must succeed");

    char udpPeek[32]{};
    std::array<std::uint8_t, 16> senderAddr{};
    std::uint32_t senderAddrSize = senderAddr.size();
    Require(_recvfrom_nid_postfix(udpServer, udpPeek, sizeof(udpPeek), MsgPeek, senderAddr.data(), &senderAddrSize) == sizeof(udpMsg), "udp _recvfrom with MSG_PEEK must succeed");
    Require(std::strcmp(udpPeek, udpMsg) == 0, "udp peeked content must match original");

    char udpNormal[32]{};
    senderAddrSize = senderAddr.size();
    Require(recvfrom_nid_postfix(udpServer, udpNormal, sizeof(udpNormal), 0, senderAddr.data(), &senderAddrSize) == sizeof(udpMsg), "udp recvfrom without flags must consume datagram");
    Require(std::strcmp(udpNormal, udpMsg) == 0, "udp consumed content must match original");

    Require(Fails(recvfrom_nid_postfix(udpServer, udpPeek, sizeof(udpPeek), MsgDontWait, nullptr, nullptr), ErrorWouldBlock), "empty udp recvfrom after receive must return EWOULDBLOCK");

    for (const int fd : {client, accepted, listener, udpClient, udpServer}) {
        Require(close_nid_postfix(fd) == 0, "close socket failed");
    }

    return 0;
}
