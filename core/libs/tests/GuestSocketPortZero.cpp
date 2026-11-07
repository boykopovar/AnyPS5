#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
        std::exit(1);
    }
}

using Address = std::array<unsigned char, 28>;

static Address Loopback(int family, std::uint32_t size) {
    Address address{};
    address[0] = static_cast<unsigned char>(size);
    address[1] = static_cast<unsigned char>(family);
    if (family == 2) { address[4] = 127; address[7] = 1; }
    else address[23] = 1;
    return address;
}

static void CheckDatagram(int family) {
    const std::uint32_t size = family == 2 ? 16 : 28;
    const int probe = socket_nid_postfix(family, 2, 0);
    if (probe < 0 && family == 28 && *__error_nid_postfix() == 47) return;
    Require(probe >= 0, "socket");
    auto address = Loopback(family, size);
    Address name{};
    std::uint32_t length = name.size();
    Require(connect_nid_postfix(probe, address.data(), size) == -1 && *__error_nid_postfix() == 49,
        "connect to port 0 on an unconnected datagram socket must fail with EADDRNOTAVAIL (49)");
    Require(getpeername_nid_postfix(probe, name.data(), &length) == -1 && *__error_nid_postfix() == 57, "still unconnected");
    const int peer = socket_nid_postfix(family, 2, 0);
    Require(peer >= 0, "peer");
    const int bound = bind_nid_postfix(peer, address.data(), size);
    if (bound < 0 && family == 28 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(peer) == 0 && close_nid_postfix(probe) == 0, "close");
        return;
    }
    Require(bound == 0, "bind peer");
    Address target{};
    length = target.size();
    Require(getsockname_nid_postfix(peer, target.data(), &length) == 0, "peer name");
    Require(connect_nid_postfix(probe, target.data(), size) == 0, "connect to the peer");
    length = name.size();
    Require(getpeername_nid_postfix(probe, name.data(), &length) == 0, "connected");
    Require(connect_nid_postfix(probe, address.data(), size) == -1 && *__error_nid_postfix() == 49,
        "connect to port 0 on a connected datagram socket must fail with EADDRNOTAVAIL (49)");
    length = name.size();
    Require(getpeername_nid_postfix(probe, name.data(), &length) == -1 && *__error_nid_postfix() == 57,
        "connect to port 0 must disconnect a connected datagram socket");
    Require(send_nid_postfix(probe, "x", 1, 0) == -1 && *__error_nid_postfix() == 39,
        "send without an address after the disconnect must fail with EDESTADDRREQ (39)");
    Require(close_nid_postfix(peer) == 0 && close_nid_postfix(probe) == 0, "close");
}

static void CheckStream(int family) {
    const std::uint32_t size = family == 2 ? 16 : 28;
    const int listener = socket_nid_postfix(family, 1, 0);
    if (listener < 0 && family == 28 && *__error_nid_postfix() == 47) return;
    Require(listener >= 0, "listener");
    auto address = Loopback(family, size);
    const int bound = bind_nid_postfix(listener, address.data(), size);
    if (bound < 0 && family == 28 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(listener) == 0, "close");
        return;
    }
    Require(bound == 0 && listen_nid_postfix(listener, 1) == 0, "listen");
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &length) == 0, "listener name");
    auto zero = Loopback(family, size);
    Require(connect_nid_postfix(listener, zero.data(), size) == -1 && *__error_nid_postfix() == 45,
        "connect on a listening stream socket must fail with EOPNOTSUPP (45)");
    const int client = socket_nid_postfix(family, 1, 0);
    Require(client >= 0, "client");
    Require(connect_nid_postfix(client, zero.data(), size) == -1 && *__error_nid_postfix() == 49,
        "connect to port 0 on an unconnected stream socket must fail with EADDRNOTAVAIL (49)");
    Require(connect_nid_postfix(client, address.data(), size) == 0, "connect");
    Require(connect_nid_postfix(client, zero.data(), size) == -1 && *__error_nid_postfix() == 56,
        "connect to port 0 on a connected stream socket must fail with EISCONN (56)");
    Require(sendto_nid_postfix(client, "x", 1, 0, zero.data(), size) == 1, "sendto port 0 on a connected stream socket");
    Require(close_nid_postfix(client) == 0 && close_nid_postfix(listener) == 0, "close");
}

static void CheckUnspecified(int type) {
    const int server = socket_nid_postfix(28, type, 0);
    if (server < 0 && *__error_nid_postfix() == 47) return;
    Require(server >= 0, "server");
    auto address = Loopback(28, 28);
    const int bound = bind_nid_postfix(server, address.data(), 28);
    if (bound < 0 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(server) == 0, "close");
        return;
    }
    Require(bound == 0, "bind server");
    if (type == 1) Require(listen_nid_postfix(server, 1) == 0, "listen");
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(server, address.data(), &length) == 0, "server name");
    auto unspecified = address;
    std::memset(unspecified.data() + 8, 0, 16);
    const int client = socket_nid_postfix(28, type, 0);
    Require(client >= 0, "client");
    Address any{};
    any[0] = 28;
    any[1] = 28;
    Require(connect_nid_postfix(client, any.data(), 28) == -1 && *__error_nid_postfix() == 49,
        "connect to port 0 of the unspecified AF_INET6 address must fail with EADDRNOTAVAIL (49)");
    Require(connect_nid_postfix(client, unspecified.data(), 28) == 0,
        "connect to the unspecified AF_INET6 address must reach ::1 on the same port");
    Address peer{};
    length = peer.size();
    Require(getpeername_nid_postfix(client, peer.data(), &length) == 0 && length == 28 && peer[1] == 28
            && std::memcmp(peer.data() + 2, address.data() + 2, 2) == 0 && std::memcmp(peer.data() + 8, address.data() + 8, 16) == 0,
        "the peer of a connect to the unspecified AF_INET6 address must be ::1 on the same port");
    if (type == 1) {
        const int accepted = accept_nid_postfix(server, nullptr, nullptr);
        Require(accepted >= 0, "accept");
        Require(send_nid_postfix(client, "x", 1, 0) == 1, "send");
        char byte = 0;
        Require(recv_nid_postfix(accepted, &byte, 1, 0) == 1 && byte == 'x', "receive on the accepted socket");
        Require(close_nid_postfix(accepted) == 0, "close");
    } else {
        Require(send_nid_postfix(client, "y", 1, 0) == 1, "send");
        char byte = 0;
        Require(recv_nid_postfix(server, &byte, 1, 0) == 1 && byte == 'y', "receive on ::1");
    }
    Require(close_nid_postfix(client) == 0 && close_nid_postfix(server) == 0, "close");
}

int main() {
    CheckDatagram(2);
    CheckDatagram(28);
    CheckStream(2);
    CheckStream(28);
    CheckUnspecified(1);
    CheckUnspecified(2);
    return 0;
}
