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

int main() {
    CheckDatagram(2);
    CheckDatagram(28);
    CheckStream(2);
    CheckStream(28);
    return 0;
}
