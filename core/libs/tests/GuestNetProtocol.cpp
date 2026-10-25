#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include "prx/libSceNet/libSceNet.hpp"


static void Require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", where.line());
        std::abort();
    }
}

static void CheckProtocol(int socket, int expected) {
    for (const std::uint32_t capacity : {0u, 1u, 3u, 4u, 8u}) {
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        auto size = capacity;
        Require(sceNetGetsockopt(socket, 0xffff, 0x1016, bytes.data(), &size) == 0);
        Require(size == std::min(capacity, 4u));
        Require(std::memcmp(bytes.data(), &expected, size) == 0);
        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto value) { return value == 0xa5; }));
    }
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    for (const int family : {2, 28}) {
        for (const int type : {1, 2}) {
            const int expected = type == 1 ? 6 : 17;
            for (const int protocol : {0, expected}) {
                const int socket = sceNetSocket("protocol", family, type, protocol);
                Require(socket >= 0);
                CheckProtocol(socket, expected);
                std::uint32_t size = 4;
                int value = 99;
                Require(sceNetGetsockopt(socket, 0xffff, 0x1016, nullptr, &size) < 0 && *sceNetErrnoLoc() == 22);
                Require(sceNetGetsockopt(socket, 0xffff, 0x1016, &value, nullptr) < 0 && *sceNetErrnoLoc() == 22);
                Require(value == 99 && size == 4);
                Require(sceNetSetsockopt(socket, 0xffff, 0x1016, &value, sizeof(value)) < 0);
                CheckProtocol(socket, expected);
                Require(sceNetSocketClose(socket) == 0);
                Require(sceNetGetsockopt(socket, 0xffff, 0x1016, &value, &size) < 0 && *sceNetErrnoLoc() == 9);
                Require(value == 99 && size == 4);
            }
        }
        const int listener = sceNetSocket("protocol-listener", family, 1, 0);
        const int client = sceNetSocket("protocol-client", family, 1, 6);
        Require(listener >= 0 && client >= 0);
        std::array<std::uint8_t, 28> address{};
        std::uint32_t size = family == 2 ? 16 : 28;
        address[0] = size;
        address[1] = family;
        if (family == 2) { address[4] = 127; address[7] = 1; }
        else address[23] = 1;
        Require(sceNetBind_nid_postfix(listener, address.data(), size) == 0);
        Require(sceNetListen(listener, 1) == 0);
        CheckProtocol(listener, 6);
        Require(sceNetGetsockname(listener, address.data(), &size) == 0);
        Require(sceNetConnect(client, address.data(), size) == 0);
        const int accepted = sceNetAccept(listener, nullptr, nullptr);
        Require(accepted >= 0);
        CheckProtocol(accepted, 6);
        CheckProtocol(client, 6);
        for (const int socket : {accepted, client, listener}) Require(sceNetSocketClose(socket) == 0);
    }
}
