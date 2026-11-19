#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static int Create(int family, int type) {
    const int result = sceNetSocket("reuseport-test", family, type, 0);
    Require(result >= 0, "create socket");
    return result;
}

static void Set(int descriptor, int option, int value) {
    Require(sceNetSetsockopt(descriptor, 0xffff, option, &value, sizeof(value)) == 0, "set reuse flag");
}

static void Read(int descriptor, int option, int expected) {
    int actual = -1;
    std::uint32_t length = sizeof(actual);
    Require(sceNetGetsockopt(descriptor, 0xffff, option, &actual, &length) == 0, "read reuse flag");
    Require(actual == expected && length == sizeof(actual), "reuse flag value");
    actual = -1;
    length = sizeof(actual);
    Require(getsockopt_nid_postfix(descriptor, 0xffff, option, &actual, &length) == 0, "read flag through kernel");
    Require(actual == expected && length == sizeof(actual), "kernel and Net agree");
}

static void Flags(int family, int type) {
    const int net = Create(family, type);
    const int kernel = socket_nid_postfix(family, type, 0);
    Require(kernel >= 0, "create kernel socket");
    int expected = 0;
    for (const auto [option, value] : std::array{
        std::pair{0x200, 0}, std::pair{0x200, 1}, std::pair{0x4, -1},
        std::pair{0x200, 0}, std::pair{0x200, -1}, std::pair{0x4, 0},
        std::pair{0x200, 2}, std::pair{0x200, 0}}) {
        expected = value != 0 ? expected | option : expected & ~option;
        for (const int descriptor : {net, kernel}) {
            Set(descriptor, option, value);
            Read(descriptor, 0x4, expected & 0x4);
            Read(descriptor, 0x200, expected & 0x200);
        }
    }
    const int one = 1;
    Require(setsockopt_nid_postfix(net, 0xffff, 0x200, &one, sizeof(one)) == 0, "set Net flag through kernel");
    Read(net, 0x200, 0x200);
    Read(net, 0x4, 0);
    for (std::uint32_t size = 0; size <= 4; ++size) {
        std::array<unsigned char, 8> bytes;
        bytes.fill(0xa5);
        auto length = size;
        Require(sceNetGetsockopt(net, 0xffff, 0x200, bytes.data(), &length) == 0 && length == size,
            "truncate flag output");
        const int value = 0x200;
        Require(std::memcmp(bytes.data(), &value, size) == 0, "truncated bytes");
        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto value) { return value == 0xa5; }),
            "preserve output tail");
    }
    Require(sceNetSetsockopt(net, 0xffff, 0x200, nullptr, sizeof(one)) == static_cast<int>(0x8041010eu),
        "reject null option value");
    Require(sceNetSetsockopt(net, 0xffff, 0x200, &one, 3) == static_cast<int>(0x80410116u),
        "reject short option value");
    Read(net, 0x200, 0x200);
    Require(sceNetSocketClose(net) == 0 && close_nid_postfix(kernel) == 0, "close sockets");
    Require(sceNetSetsockopt(net, 0xffff, 0x200, &one, sizeof(one)) == static_cast<int>(0x80410109u),
        "reject closed socket");
}

static std::array<std::uint8_t, 28> Address(int family) {
    std::array<std::uint8_t, 28> result{};
    result[0] = family == 2 ? 16 : 28;
    result[1] = static_cast<std::uint8_t>(family);
    if (family == 2) {
        result[4] = 127;
        result[7] = 1;
    } else {
        result[23] = 1;
    }
    return result;
}

static void DuplicateBind(int family, bool enabled) {
    const int first = Create(family, 2);
    const int second = Create(family, 2);
    Set(first, 0x200, enabled ? 1 : 0);
    Set(second, 0x200, enabled ? 1 : 0);
    Read(first, 0x4, 0);
    auto address = Address(family);
    Require(sceNetBind_nid_postfix(first, address.data(), address[0]) == 0, "bind first datagram");
    std::uint32_t length = address[0];
    Require(sceNetGetsockname(first, address.data(), &length) == 0, "read assigned port");
    const int result = sceNetBind_nid_postfix(second, address.data(), length);
    Require(result == (enabled ? 0 : static_cast<int>(0x80410130u)), "reuseport changes duplicate bind behavior");
    Require(sceNetSocketClose(first) == 0 && sceNetSocketClose(second) == 0, "close datagrams");
}

static void Inheritance(int family) {
    const int listener = Create(family, 1);
    Set(listener, 0x4, 1);
    Set(listener, 0x200, 1);
    auto address = Address(family);
    Require(sceNetBind_nid_postfix(listener, address.data(), address[0]) == 0, "bind listener");
    Require(sceNetListen(listener, 1) == 0, "listen");
    std::uint32_t length = address[0];
    Require(sceNetGetsockname(listener, address.data(), &length) == 0, "read listener port");
    const int client = Create(family, 1);
    Require(sceNetConnect(client, address.data(), length) == 0, "connect");
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept");
    Read(accepted, 0x4, 0x4);
    Read(accepted, 0x200, 0x200);
    Set(listener, 0x200, 0);
    Read(listener, 0x4, 0x4);
    Read(listener, 0x200, 0);
    Read(accepted, 0x200, 0x200);
    Require(sceNetSocketClose(accepted) == 0 && sceNetSocketClose(client) == 0 && sceNetSocketClose(listener) == 0,
        "close streams");
}

int main(int argc, char** argv) {
    Require(argc == 2, "supply socket family");
    const int family = std::atoi(argv[1]);
    Require(family == 2 || family == 28, "supported test family");
    Require(sceNetInit_nid_postfix() == 0, "initialize network");
    const int probe = sceNetSocket("reuseport-probe", family, 2, 0);
    if (family == 28 && (probe == static_cast<int>(0x8041012fu) || probe == static_cast<int>(0x8041012bu))) return 77;
    Require(probe >= 0 && sceNetSocketClose(probe) == 0, "probe family");
    Flags(family, 1);
    Flags(family, 2);
    DuplicateBind(family, false);
    DuplicateBind(family, true);
    Inheritance(family);
}
