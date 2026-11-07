#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int NetError = static_cast<int>(0x80410100u);
constexpr int Einval = 22;
constexpr int SolSocket = 0xffff;
constexpr int SoAcceptConn = 0x2;

using Address = std::array<std::uint8_t, 28>;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

std::uint32_t Size(int family) {
    return family == 2 ? 16 : 28;
}

Address Loopback(int family) {
    Address result{};
    result[0] = static_cast<std::uint8_t>(Size(family));
    result[1] = static_cast<std::uint8_t>(family);
    if (family == 2) {
        result[4] = 127;
        result[7] = 1;
    } else {
        result[23] = 1;
    }
    return result;
}

unsigned Port(const Address& address) {
    return static_cast<unsigned>(address[2]) << 8 | address[3];
}

Address Name(int socket, int family) {
    Address name{};
    std::uint32_t length = name.size();
    Require(sceNetGetsockname(socket, name.data(), &length) == 0);
    Require(length == Size(family) && name[1] == family);
    return name;
}

bool Listening(int socket) {
    int value = -1;
    std::uint32_t length = sizeof(value);
    Require(sceNetGetsockopt(socket, SolSocket, SoAcceptConn, &value, &length) == 0);
    return value == SoAcceptConn;
}

void Connect(int listener, int family, unsigned port) {
    auto target = Loopback(family);
    target[2] = static_cast<std::uint8_t>(port >> 8);
    target[3] = static_cast<std::uint8_t>(port);
    const int client = sceNetSocket("listen-client", family, 1, 0);
    Require(client >= 0);
    Require(sceNetConnect(client, target.data(), Size(family)) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    Require(sceNetSocketClose(accepted) == 0 && sceNetSocketClose(client) == 0);
}

void Unbound(int family) {
    const int listener = sceNetSocket("listen-unbound", family, 1, 0);
    Require(listener >= 0);
    Require(sceNetListen(listener, 1) == 0);
    Require(Listening(listener));
    const unsigned port = Port(Name(listener, family));
    Require(port != 0);
    Connect(listener, family, port);
    auto address = Loopback(family);
    Require(sceNetBind_nid_postfix(listener, address.data(), Size(family)) == (NetError | Einval) && *sceNetErrnoLoc() == Einval);
    Require(Port(Name(listener, family)) == port);
    Require(sceNetSocketClose(listener) == 0);
}

void Bound(int family) {
    const int listener = sceNetSocket("listen-bound", family, 1, 0);
    Require(listener >= 0);
    auto address = Loopback(family);
    Require(sceNetBind_nid_postfix(listener, address.data(), Size(family)) == 0);
    const unsigned port = Port(Name(listener, family));
    Require(port != 0);
    Require(sceNetListen(listener, 1) == 0);
    Require(Listening(listener));
    Require(Port(Name(listener, family)) == port);
    Connect(listener, family, port);
    Require(sceNetSocketClose(listener) == 0);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    const int family = std::atoi(argv[1]);
    Require(family == 2 || family == 28);
    Require(sceNetInit_nid_postfix() == 0);
    const int probe = sceNetSocket("listen-probe", family, 1, 0);
    if (family == 28 && (probe == static_cast<int>(0x8041012fu) || probe == static_cast<int>(0x8041012bu))) return 77;
    Require(probe >= 0 && sceNetSocketClose(probe) == 0);
    Unbound(family);
    Bound(family);
}
