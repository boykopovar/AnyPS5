#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNotConnected = 57;

constexpr int FamilyInet = 2;
constexpr int FamilyInet6 = 28;
constexpr int TypeStream = 1;
constexpr int TypeDatagram = 2;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool Fails(long result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void TestUnboundSockets() {
    int s = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(s >= 0, "socket AF_INET SOCK_STREAM failed");
    std::array<std::uint8_t, 16> addr{};
    std::uint32_t len = addr.size();
    Require(getsockname_nid_postfix(s, addr.data(), &len) == 0, "getsockname on unbound IPv4 stream socket failed");
    Require(len == 16, "getsockname reported length must be 16");
    Require(addr[0] == 16, "sa_len must be 16");
    Require(addr[1] == FamilyInet, "sa_family must be AF_INET (2)");
    Require(addr[2] == 0 && addr[3] == 0, "port must be 0 for unbound socket");
    Require(addr[4] == 0 && addr[5] == 0 && addr[6] == 0 && addr[7] == 0, "ip must be 0.0.0.0 for unbound socket");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(s, addr.data(), &len), ErrorNotConnected), "getpeername on unbound IPv4 stream socket must fail with ENOTCONN (57)");
    Require(close_nid_postfix(s) == 0, "close failed");

    s = socket_nid_postfix(FamilyInet, TypeDatagram, 0);
    Require(s >= 0, "socket AF_INET SOCK_DGRAM failed");
    addr.fill(0);
    len = addr.size();
    Require(getsockname_nid_postfix(s, addr.data(), &len) == 0, "getsockname on unbound IPv4 dgram socket failed");
    Require(len == 16, "getsockname reported length must be 16");
    Require(addr[0] == 16 && addr[1] == FamilyInet, "sa_len 16 and sa_family AF_INET");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(s, addr.data(), &len), ErrorNotConnected), "getpeername on unbound IPv4 dgram socket must fail with ENOTCONN (57)");
    Require(close_nid_postfix(s) == 0, "close failed");

    s = socket_nid_postfix(FamilyInet6, TypeStream, 0);
    Require(s >= 0, "socket AF_INET6 SOCK_STREAM failed");
    std::array<std::uint8_t, 28> addr6{};
    len = addr6.size();
    Require(getsockname_nid_postfix(s, addr6.data(), &len) == 0, "getsockname on unbound IPv6 stream socket failed");
    Require(len == 28, "getsockname reported length must be 28");
    Require(addr6[0] == 28, "sa_len must be 28");
    Require(addr6[1] == FamilyInet6, "sa_family must be AF_INET6 (28)");
    len = addr6.size();
    Require(Fails(getpeername_nid_postfix(s, addr6.data(), &len), ErrorNotConnected), "getpeername on unbound IPv6 stream socket must fail with ENOTCONN (57)");
    Require(close_nid_postfix(s) == 0, "close failed");

    s = socket_nid_postfix(FamilyInet6, TypeDatagram, 0);
    Require(s >= 0, "socket AF_INET6 SOCK_DGRAM failed");
    addr6.fill(0);
    len = addr6.size();
    Require(getsockname_nid_postfix(s, addr6.data(), &len) == 0, "getsockname on unbound IPv6 dgram socket failed");
    Require(len == 28, "getsockname reported length must be 28");
    Require(addr6[0] == 28 && addr6[1] == FamilyInet6, "sa_len 28 and sa_family AF_INET6");
    len = addr6.size();
    Require(Fails(getpeername_nid_postfix(s, addr6.data(), &len), ErrorNotConnected), "getpeername on unbound IPv6 dgram socket must fail with ENOTCONN (57)");
    Require(close_nid_postfix(s) == 0, "close failed");
}

void TestNonConnectedAndConnectedSockets() {
    int listener = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(listener >= 0, "socket failed");
    std::array<std::uint8_t, 16> bindAddr{16, FamilyInet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, bindAddr.data(), bindAddr.size()) == 0, "bind failed");
    std::uint32_t len = bindAddr.size();
    Require(getsockname_nid_postfix(listener, bindAddr.data(), &len) == 0, "getsockname on bound listener failed");
    Require(len == 16, "length is 16");
    const std::uint16_t port = (static_cast<std::uint16_t>(bindAddr[2]) << 8) | bindAddr[3];
    Require(port != 0, "bound ephemeral port must be non-zero");
    Require(listen_nid_postfix(listener, 4) == 0, "listen failed");

    len = bindAddr.size();
    Require(Fails(getpeername_nid_postfix(listener, bindAddr.data(), &len), ErrorNotConnected), "getpeername on listening socket must fail with ENOTCONN (57)");

    int client = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(client >= 0, "client socket failed");
    len = bindAddr.size();
    Require(Fails(getpeername_nid_postfix(client, bindAddr.data(), &len), ErrorNotConnected), "getpeername before connect must fail with ENOTCONN (57)");
    Require(connect_nid_postfix(client, bindAddr.data(), bindAddr.size()) == 0, "connect failed");
    int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");

    std::array<std::uint8_t, 16> clientPeer{};
    len = clientPeer.size();
    Require(getpeername_nid_postfix(client, clientPeer.data(), &len) == 0, "getpeername on connected client failed");
    Require(len == 16, "peer length must be 16");
    Require(clientPeer[0] == 16 && clientPeer[1] == FamilyInet, "peer family AF_INET");
    Require(clientPeer[2] == bindAddr[2] && clientPeer[3] == bindAddr[3], "peer port must match listener port");
    Require(clientPeer[4] == 127 && clientPeer[5] == 0 && clientPeer[6] == 0 && clientPeer[7] == 1, "peer address must match loopback");

    std::array<std::uint8_t, 16> clientLocal{};
    len = clientLocal.size();
    Require(getsockname_nid_postfix(client, clientLocal.data(), &len) == 0, "getsockname on connected client failed");
    Require(len == 16, "client local length must be 16");
    Require(clientLocal[0] == 16 && clientLocal[1] == FamilyInet, "client local family AF_INET");
    const std::uint16_t clientPort = (static_cast<std::uint16_t>(clientLocal[2]) << 8) | clientLocal[3];
    Require(clientPort != 0, "client ephemeral port must be non-zero");

    std::array<std::uint8_t, 16> acceptedPeer{};
    len = acceptedPeer.size();
    Require(getpeername_nid_postfix(accepted, acceptedPeer.data(), &len) == 0, "getpeername on accepted socket failed");
    Require(len == 16, "accepted peer length must be 16");
    Require(acceptedPeer[2] == clientLocal[2] && acceptedPeer[3] == clientLocal[3], "accepted peer port must match client local port");

    std::array<std::uint8_t, 16> acceptedLocal{};
    len = acceptedLocal.size();
    Require(getsockname_nid_postfix(accepted, acceptedLocal.data(), &len) == 0, "getsockname on accepted socket failed");
    Require(len == 16, "accepted local length must be 16");
    Require(acceptedLocal[2] == bindAddr[2] && acceptedLocal[3] == bindAddr[3], "accepted local port must match listener port");

    Require(close_nid_postfix(accepted) == 0, "close accepted failed");
    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(listener) == 0, "close listener failed");

    int d1 = socket_nid_postfix(FamilyInet, TypeDatagram, 0);
    Require(d1 >= 0, "socket d1 failed");
    std::array<std::uint8_t, 16> d1Addr{16, FamilyInet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(d1, d1Addr.data(), d1Addr.size()) == 0, "bind d1 failed");
    len = d1Addr.size();
    Require(getsockname_nid_postfix(d1, d1Addr.data(), &len) == 0, "getsockname d1 failed");

    len = d1Addr.size();
    Require(Fails(getpeername_nid_postfix(d1, d1Addr.data(), &len), ErrorNotConnected), "getpeername on unconnected datagram socket must fail with ENOTCONN (57)");

    int d2 = socket_nid_postfix(FamilyInet, TypeDatagram, 0);
    Require(d2 >= 0, "socket d2 failed");
    Require(connect_nid_postfix(d2, d1Addr.data(), d1Addr.size()) == 0, "connect d2 to d1 failed");

    std::array<std::uint8_t, 16> d2Peer{};
    len = d2Peer.size();
    Require(getpeername_nid_postfix(d2, d2Peer.data(), &len) == 0, "getpeername on connected dgram socket failed");
    Require(len == 16, "length 16");
    Require(d2Peer[2] == d1Addr[2] && d2Peer[3] == d1Addr[3], "d2 peer port must match d1 bound port");

    Require(close_nid_postfix(d1) == 0, "close d1 failed");
    Require(close_nid_postfix(d2) == 0, "close d2 failed");
}

void TestInvalidDescriptors() {
    std::array<std::uint8_t, 16> addr{};
    std::uint32_t len = addr.size();
    Require(Fails(getsockname_nid_postfix(-1, addr.data(), &len), ErrorBadDescriptor), "getsockname(-1) must fail with EBADF (9)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(-1, addr.data(), &len), ErrorBadDescriptor), "getpeername(-1) must fail with EBADF (9)");

    len = addr.size();
    Require(Fails(getsockname_nid_postfix(-100, addr.data(), &len), ErrorBadDescriptor), "getsockname(-100) must fail with EBADF (9)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(-100, addr.data(), &len), ErrorBadDescriptor), "getpeername(-100) must fail with EBADF (9)");

    len = addr.size();
    Require(Fails(getsockname_nid_postfix(1 << 20, addr.data(), &len), ErrorBadDescriptor), "getsockname on unopened descriptor must fail with EBADF (9)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(1 << 20, addr.data(), &len), ErrorBadDescriptor), "getpeername on unopened descriptor must fail with EBADF (9)");

    int s = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(s >= 0, "socket failed");
    Require(close_nid_postfix(s) == 0, "close failed");
    len = addr.size();
    Require(Fails(getsockname_nid_postfix(s, addr.data(), &len), ErrorBadDescriptor), "getsockname on closed descriptor must fail with EBADF (9)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(s, addr.data(), &len), ErrorBadDescriptor), "getpeername on closed descriptor must fail with EBADF (9)");
}

void TestNonSocketDescriptors() {
    int fds[2];
    Require(pipe_nid_postfix(fds) == 0, "pipe failed");
    std::array<std::uint8_t, 16> addr{};
    std::uint32_t len = addr.size();
    Require(Fails(getsockname_nid_postfix(fds[0], addr.data(), &len), ErrorNotSocket), "getsockname on pipe read end must fail with ENOTSOCK (38)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(fds[0], addr.data(), &len), ErrorNotSocket), "getpeername on pipe read end must fail with ENOTSOCK (38)");
    len = addr.size();
    Require(Fails(getsockname_nid_postfix(fds[1], addr.data(), &len), ErrorNotSocket), "getsockname on pipe write end must fail with ENOTSOCK (38)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(fds[1], addr.data(), &len), ErrorNotSocket), "getpeername on pipe write end must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(fds[0]) == 0 && close_nid_postfix(fds[1]) == 0, "close pipe failed");

    len = addr.size();
    Require(Fails(getsockname_nid_postfix(fds[0], addr.data(), &len), ErrorBadDescriptor), "getsockname on closed pipe fd must fail with EBADF (9)");
    len = addr.size();
    Require(Fails(getpeername_nid_postfix(fds[0], addr.data(), &len), ErrorBadDescriptor), "getpeername on closed pipe fd must fail with EBADF (9)");
}

void TestTruncatedSockaddrLengthBounds() {
    int s = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(s >= 0, "socket failed");
    std::array<std::uint8_t, 16> fullAddr{16, FamilyInet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(s, fullAddr.data(), fullAddr.size()) == 0, "bind failed");
    std::uint32_t fullLen = fullAddr.size();
    Require(getsockname_nid_postfix(s, fullAddr.data(), &fullLen) == 0, "getsockname failed");
    Require(fullLen == 16, "full length is 16");

    std::array<std::uint8_t, 16> testBuf{};
    std::memset(testBuf.data(), 0xAA, testBuf.size());
    std::uint32_t len = 0;
    Require(getsockname_nid_postfix(s, testBuf.data(), &len) == 0, "getsockname with length 0 must succeed");
    Require(len == 0, "length must be 0 after truncation");
    Require(testBuf[0] == 0xAA, "buffer must remain untouched when length is 0");

    std::memset(testBuf.data(), 0xAA, testBuf.size());
    len = 1;
    Require(getsockname_nid_postfix(s, testBuf.data(), &len) == 0, "getsockname with length 1 must succeed");
    Require(len == 1, "length must be 1 after truncation");
    Require(testBuf[0] == 16, "sa_len byte must be written");
    Require(testBuf[1] == 0xAA, "remaining buffer untouched");

    std::memset(testBuf.data(), 0xAA, testBuf.size());
    len = 2;
    Require(getsockname_nid_postfix(s, testBuf.data(), &len) == 0, "getsockname with length 2 must succeed");
    Require(len == 2, "length must be 2 after truncation");
    Require(testBuf[0] == 16 && testBuf[1] == FamilyInet, "sa_len and sa_family written");
    Require(testBuf[2] == 0xAA, "remaining buffer untouched");

    std::memset(testBuf.data(), 0xAA, testBuf.size());
    len = 4;
    Require(getsockname_nid_postfix(s, testBuf.data(), &len) == 0, "getsockname with length 4 must succeed");
    Require(len == 4, "length must be 4 after truncation");
    Require(testBuf[0] == 16 && testBuf[1] == FamilyInet, "sa_len and sa_family written");
    Require(testBuf[2] == fullAddr[2] && testBuf[3] == fullAddr[3], "port written correctly");
    Require(testBuf[4] == 0xAA, "ip address part untouched");

    std::memset(testBuf.data(), 0xAA, testBuf.size());
    len = 8;
    Require(getsockname_nid_postfix(s, testBuf.data(), &len) == 0, "getsockname with length 8 must succeed");
    Require(len == 8, "length must be 8 after truncation");
    Require(std::memcmp(testBuf.data(), fullAddr.data(), 8) == 0, "first 8 bytes must match");
    Require(testBuf[8] == 0xAA, "past 8 bytes untouched");

    std::memset(testBuf.data(), 0xAA, testBuf.size());
    len = 15;
    Require(getsockname_nid_postfix(s, testBuf.data(), &len) == 0, "getsockname with length 15 must succeed");
    Require(len == 15, "length must be 15 after truncation");
    Require(std::memcmp(testBuf.data(), fullAddr.data(), 15) == 0, "first 15 bytes match");
    Require(testBuf[15] == 0xAA, "byte 15 untouched");

    std::array<std::uint8_t, 64> oversized{};
    oversized.fill(0x55);
    len = oversized.size();
    Require(getsockname_nid_postfix(s, oversized.data(), &len) == 0, "getsockname with oversized length must succeed");
    Require(len == 16, "oversized length must be clamped to 16 for IPv4");
    Require(std::memcmp(oversized.data(), fullAddr.data(), 16) == 0, "first 16 bytes match");
    Require(oversized[16] == 0x55, "bytes beyond 16 untouched");

    Require(listen_nid_postfix(s, 1) == 0, "listen failed");
    int c = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(c >= 0, "client socket failed");
    Require(connect_nid_postfix(c, fullAddr.data(), fullAddr.size()) == 0, "connect failed");
    int a = accept_nid_postfix(s, nullptr, nullptr);
    Require(a >= 0, "accept failed");

    std::array<std::uint8_t, 16> peerTrunc{};
    std::memset(peerTrunc.data(), 0xBB, peerTrunc.size());
    len = 4;
    Require(getpeername_nid_postfix(c, peerTrunc.data(), &len) == 0, "getpeername with truncated length 4 must succeed");
    Require(len == 4, "getpeername length must be 4");
    Require(peerTrunc[0] == 16 && peerTrunc[1] == FamilyInet, "peer sa_len and family match");
    Require(peerTrunc[2] == fullAddr[2] && peerTrunc[3] == fullAddr[3], "peer port matches listener port");
    Require(peerTrunc[4] == 0xBB, "peer past byte 4 untouched");

    std::memset(peerTrunc.data(), 0xBB, peerTrunc.size());
    len = 8;
    Require(getpeername_nid_postfix(c, peerTrunc.data(), &len) == 0, "getpeername with truncated length 8 must succeed");
    Require(len == 8, "getpeername length must be 8");
    Require(std::memcmp(peerTrunc.data(), fullAddr.data(), 8) == 0, "first 8 bytes match fullAddr");
    Require(peerTrunc[8] == 0xBB, "peer past byte 8 untouched");

    oversized.fill(0x77);
    len = oversized.size();
    Require(getpeername_nid_postfix(c, oversized.data(), &len) == 0, "getpeername with oversized length must succeed");
    Require(len == 16, "oversized length clamped to 16");
    Require(std::memcmp(oversized.data(), fullAddr.data(), 16) == 0, "full 16 bytes match fullAddr");
    Require(oversized[16] == 0x77, "bytes beyond 16 untouched");

    Require(close_nid_postfix(a) == 0, "close a failed");
    Require(close_nid_postfix(c) == 0, "close c failed");
    Require(close_nid_postfix(s) == 0, "close s failed");
}

void TestPointerFaultValidation() {
    int s = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(s >= 0, "socket failed");
    std::array<std::uint8_t, 16> addr{};

    Require(Fails(getsockname_nid_postfix(s, addr.data(), nullptr), ErrorFault), "getsockname with null length pointer must fail with EFAULT (14)");
    Require(Fails(getpeername_nid_postfix(s, addr.data(), nullptr), ErrorFault), "getpeername with null length pointer must fail with EFAULT (14)");

    Require(Fails(getsockname_nid_postfix(-1, addr.data(), nullptr), ErrorFault), "getsockname(-1, addr, nullptr) must fail with EFAULT (14) before descriptor check");
    Require(Fails(getpeername_nid_postfix(-1, addr.data(), nullptr), ErrorFault), "getpeername(-1, addr, nullptr) must fail with EFAULT (14) before descriptor check");

    std::uint32_t len = 16;
    Require(Fails(getsockname_nid_postfix(s, nullptr, &len), ErrorFault), "getsockname with null address and non-zero length must fail with EFAULT (14)");

    Require(close_nid_postfix(s) == 0, "close failed");
}

} // namespace

int main() {
    TestUnboundSockets();
    TestNonConnectedAndConnectedSockets();
    TestInvalidDescriptors();
    TestNonSocketDescriptors();
    TestTruncatedSockaddrLengthBounds();
    TestPointerFaultValidation();
    return 0;
}
