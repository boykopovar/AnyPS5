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
constexpr int Inet6 = 28;
constexpr int Stream = 1;
constexpr int Datagram = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorWouldBlock = 35;
constexpr int ErrorNotSocket = 38;

constexpr int SocketCloseOnExec = 0x10000000;
constexpr int SocketNonBlocking = 0x20000000;
constexpr int GetDescriptorFlags = 1;
constexpr int CloseOnExec = 1;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

} // namespace

int main() {
    std::array<std::uint8_t, 16> peerBuffer{};
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(-1, peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept with address and null length must fail with EFAULT (14) before descriptor check");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(9999, peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept on out-of-range fd with address and null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(-1, peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_accept with address and null length must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(9999, peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_accept on out-of-range fd with address and null length must fail with EFAULT (14)");

    int pipeEndpoints[2] = {-1, -1};
    Require(pipe_nid_postfix(pipeEndpoints) == 0, "pipe creation failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(pipeEndpoints[0], peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept on pipe with address and null length must fail with EFAULT before ENOTSOCK");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(pipeEndpoints[0], peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_accept on pipe with address and null length must fail with EFAULT before ENOTSOCK");

    const int unlistenedStream = socket_nid_postfix(Inet, Stream, 0);
    Require(unlistenedStream >= 0, "socket creation failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(unlistenedStream, peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "accept on unlistened stream with address and null length must fail with EFAULT before EINVAL");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(unlistenedStream, peerBuffer.data(), nullptr) == -1 && *__error_nid_postfix() == ErrorFault,
        "_accept on unlistened stream with address and null length must fail with EFAULT before EINVAL");

    for (const int badDescriptor : {-100, -1, 9999}) {
        *__error_nid_postfix() = 0;
        Require(accept_nid_postfix(badDescriptor, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "accept on bad descriptor without address must fail with EBADF (9)");
        *__error_nid_postfix() = 0;
        Require(_accept_nid_postfix(badDescriptor, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
            "_accept on bad descriptor without address must fail with EBADF (9)");
    }

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(pipeEndpoints[0], nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "accept on pipe read end must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(pipeEndpoints[1], nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "accept on pipe write end must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(pipeEndpoints[0], nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "_accept on pipe read end must fail with ENOTSOCK (38)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(pipeEndpoints[1], nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorNotSocket,
        "_accept on pipe write end must fail with ENOTSOCK (38)");
    Require(close_nid_postfix(pipeEndpoints[0]) == 0 && close_nid_postfix(pipeEndpoints[1]) == 0, "close pipe failed");

    const int udpIpv4 = socket_nid_postfix(Inet, Datagram, 0);
    Require(udpIpv4 >= 0, "socket udp ipv4 creation failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(udpIpv4, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "accept on UDP IPv4 datagram socket must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(udpIpv4, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "_accept on UDP IPv4 datagram socket must fail with EINVAL (22)");
    Require(close_nid_postfix(udpIpv4) == 0, "close udp ipv4 failed");

    const int udpIpv6 = socket_nid_postfix(Inet6, Datagram, 0);
    Require(udpIpv6 >= 0, "socket udp ipv6 creation failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(udpIpv6, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "accept on UDP IPv6 datagram socket must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(udpIpv6, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "_accept on UDP IPv6 datagram socket must fail with EINVAL (22)");
    Require(close_nid_postfix(udpIpv6) == 0, "close udp ipv6 failed");

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(unlistenedStream, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "accept on unbound stream socket must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(unlistenedStream, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "_accept on unbound stream socket must fail with EINVAL (22)");

    std::array<std::uint8_t, 16> loopbackBind{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(unlistenedStream, loopbackBind.data(), loopbackBind.size()) == 0, "bind stream socket failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(unlistenedStream, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "accept on bound non-listening stream socket must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(unlistenedStream, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "_accept on bound non-listening stream socket must fail with EINVAL (22)");
    Require(close_nid_postfix(unlistenedStream) == 0, "close unlistened stream failed");

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket listener failed");
    Require(bind_nid_postfix(listener, loopbackBind.data(), loopbackBind.size()) == 0, "bind listener failed");
    std::uint32_t boundLen = loopbackBind.size();
    Require(getsockname_nid_postfix(listener, loopbackBind.data(), &boundLen) == 0 && boundLen == 16, "getsockname listener failed");
    Require(listen_nid_postfix(listener, 10) == 0, "listen failed");

    const int client1 = socket_nid_postfix(Inet, Stream, 0);
    Require(client1 >= 0, "client1 socket failed");
    Require(connect_nid_postfix(client1, loopbackBind.data(), loopbackBind.size()) == 0, "client1 connect failed");

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(client1, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "accept on connected client stream socket must fail with EINVAL (22)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(client1, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorInvalid,
        "_accept on connected client stream socket must fail with EINVAL (22)");

    const int serverFd1 = accept_nid_postfix(listener, nullptr, nullptr);
    Require(serverFd1 >= 0 && serverFd1 != listener && serverFd1 != client1, "accept with null address and null length failed");
    Require(close_nid_postfix(serverFd1) == 0 && close_nid_postfix(client1) == 0, "close serverFd1/client1 failed");

    const int client2 = socket_nid_postfix(Inet, Stream, 0);
    Require(client2 >= 0, "client2 socket failed");
    Require(connect_nid_postfix(client2, loopbackBind.data(), loopbackBind.size()) == 0, "client2 connect failed");
    std::uint32_t lengthParam = 16;
    const int serverFd2 = accept_nid_postfix(listener, nullptr, &lengthParam);
    Require(serverFd2 >= 0 && serverFd2 != listener && serverFd2 != client2, "accept with null address and non-null length failed");
    Require(close_nid_postfix(serverFd2) == 0 && close_nid_postfix(client2) == 0, "close serverFd2/client2 failed");

    const int client3 = socket_nid_postfix(Inet, Stream, 0);
    Require(client3 >= 0, "client3 socket failed");
    Require(connect_nid_postfix(client3, loopbackBind.data(), loopbackBind.size()) == 0, "client3 connect failed");
    std::array<std::uint8_t, 16> clientAddress{};
    std::uint32_t clientAddressLen = clientAddress.size();
    Require(getsockname_nid_postfix(client3, clientAddress.data(), &clientAddressLen) == 0 && clientAddressLen == 16,
        "getsockname client3 failed");
    std::array<std::uint8_t, 16> peerAddress{};
    std::uint32_t peerAddressLen = peerAddress.size();
    const int serverFd3 = _accept_nid_postfix(listener, peerAddress.data(), &peerAddressLen);
    Require(serverFd3 >= 0, "_accept with peer address failed");
    Require(peerAddressLen == 16, "peer address length must be 16");
    Require(peerAddress[0] == 16 && peerAddress[1] == Inet, "peer address must have length 16 and AF_INET family");
    Require(peerAddress[2] == clientAddress[2] && peerAddress[3] == clientAddress[3], "peer port must match client port");
    Require(peerAddress[4] == 127 && peerAddress[5] == 0 && peerAddress[6] == 0 && peerAddress[7] == 1,
        "peer IPv4 address must match 127.0.0.1");
    Require(close_nid_postfix(serverFd3) == 0 && close_nid_postfix(client3) == 0, "close serverFd3/client3 failed");

    const int client4 = socket_nid_postfix(Inet, Stream, 0);
    Require(client4 >= 0, "client4 socket failed");
    Require(connect_nid_postfix(client4, loopbackBind.data(), loopbackBind.size()) == 0, "client4 connect failed");
    std::array<std::uint8_t, 16> shortPeer{};
    shortPeer.fill(0xAA);
    std::uint32_t shortLen = 6;
    const int serverFd4 = accept_nid_postfix(listener, shortPeer.data(), &shortLen);
    Require(serverFd4 >= 0, "accept with short length failed");
    Require(shortLen == 6, "accept must truncate and return short length 6");
    Require(shortPeer[0] == 16 && shortPeer[1] == Inet, "short peer address must retain copied bytes");
    Require(shortPeer[6] == 0xAA && shortPeer[7] == 0xAA && shortPeer[15] == 0xAA,
        "bytes beyond short length must remain untouched");
    Require(close_nid_postfix(serverFd4) == 0 && close_nid_postfix(client4) == 0, "close serverFd4/client4 failed");

    const int client5 = socket_nid_postfix(Inet, Stream, 0);
    Require(client5 >= 0, "client5 socket failed");
    Require(connect_nid_postfix(client5, loopbackBind.data(), loopbackBind.size()) == 0, "client5 connect failed");
    std::array<std::uint8_t, 16> zeroPeer{};
    zeroPeer.fill(0x55);
    std::uint32_t zeroLen = 0;
    const int serverFd5 = _accept_nid_postfix(listener, zeroPeer.data(), &zeroLen);
    Require(serverFd5 >= 0, "_accept with zero length failed");
    Require(zeroLen == 0, "zero length must remain 0");
    Require(zeroPeer[0] == 0x55 && zeroPeer[15] == 0x55, "zero length must not write into address buffer");
    Require(close_nid_postfix(serverFd5) == 0 && close_nid_postfix(client5) == 0, "close serverFd5/client5 failed");

    const int nonblockListener = socket_nid_postfix(Inet, Stream | SocketNonBlocking, 0);
    Require(nonblockListener >= 0, "nonblock socket failed");
    std::array<std::uint8_t, 16> nonblockBind{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(nonblockListener, nonblockBind.data(), nonblockBind.size()) == 0, "bind nonblockListener failed");
    std::uint32_t nonblockBindLen = nonblockBind.size();
    Require(getsockname_nid_postfix(nonblockListener, nonblockBind.data(), &nonblockBindLen) == 0 && nonblockBindLen == 16,
        "getsockname nonblockListener failed");
    Require(listen_nid_postfix(nonblockListener, 5) == 0, "listen nonblockListener failed");

    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(nonblockListener, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorWouldBlock,
        "accept on nonblocking listener without connections must fail with EWOULDBLOCK (35)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(nonblockListener, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorWouldBlock,
        "_accept on nonblocking listener without connections must fail with EWOULDBLOCK (35)");

    const int client6 = socket_nid_postfix(Inet, Stream, 0);
    Require(client6 >= 0, "client6 socket failed");
    Require(connect_nid_postfix(client6, nonblockBind.data(), nonblockBind.size()) == 0, "client6 connect failed");
    const int acceptedNonblock = accept_nid_postfix(nonblockListener, nullptr, nullptr);
    Require(acceptedNonblock >= 0, "accept on nonblocking listener failed");
    std::array<char, 32> receiveBuffer{};
    *__error_nid_postfix() = 0;
    Require(recv_nid_postfix(acceptedNonblock, receiveBuffer.data(), receiveBuffer.size(), 0) == -1 &&
        *__error_nid_postfix() == ErrorWouldBlock, "accepted socket must inherit nonblocking state (EWOULDBLOCK 35)");
    Require(fcntl_nid_postfix(acceptedNonblock, GetDescriptorFlags) == 0,
        "accepted socket must not inherit close-on-exec descriptor flag");
    Require(close_nid_postfix(acceptedNonblock) == 0 && close_nid_postfix(client6) == 0 &&
        close_nid_postfix(nonblockListener) == 0, "close nonblock sockets failed");

    const int cloexecListener = socket_nid_postfix(Inet, Stream | SocketCloseOnExec, 0);
    Require(cloexecListener >= 0, "cloexecListener socket failed");
    Require(fcntl_nid_postfix(cloexecListener, GetDescriptorFlags) == CloseOnExec,
        "SocketCloseOnExec must set FD_CLOEXEC on listener");
    std::array<std::uint8_t, 16> cloexecBind{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(cloexecListener, cloexecBind.data(), cloexecBind.size()) == 0, "bind cloexecListener failed");
    std::uint32_t cloexecBindLen = cloexecBind.size();
    Require(getsockname_nid_postfix(cloexecListener, cloexecBind.data(), &cloexecBindLen) == 0 && cloexecBindLen == 16,
        "getsockname cloexecListener failed");
    Require(listen_nid_postfix(cloexecListener, 5) == 0, "listen cloexecListener failed");

    const int client7 = socket_nid_postfix(Inet, Stream, 0);
    Require(client7 >= 0, "client7 socket failed");
    Require(connect_nid_postfix(client7, cloexecBind.data(), cloexecBind.size()) == 0, "client7 connect failed");
    const int acceptedCloexec = accept_nid_postfix(cloexecListener, nullptr, nullptr);
    Require(acceptedCloexec >= 0, "accept on cloexec listener failed");
    Require(fcntl_nid_postfix(acceptedCloexec, GetDescriptorFlags) == 0,
        "accepted socket must not inherit FD_CLOEXEC from listener");
    Require(close_nid_postfix(acceptedCloexec) == 0 && close_nid_postfix(client7) == 0 &&
        close_nid_postfix(cloexecListener) == 0, "close cloexec sockets failed");

    Require(close_nid_postfix(listener) == 0, "close listener failed");
    *__error_nid_postfix() = 0;
    Require(accept_nid_postfix(listener, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "accept on closed listener must fail with EBADF (9)");
    *__error_nid_postfix() = 0;
    Require(_accept_nid_postfix(listener, nullptr, nullptr) == -1 && *__error_nid_postfix() == ErrorBadDescriptor,
        "_accept on closed listener must fail with EBADF (9)");

    return 0;
}
