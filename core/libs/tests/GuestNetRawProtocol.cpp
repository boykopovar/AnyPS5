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
    const int protocol = 1;
    const int socket = sceNetSocket("raw-protocol", 2, 3, protocol);
    if (socket < 0) {
        const int error = *sceNetErrnoLoc();
        if (error == 1 || error == 13) return 77;
        Require(false);
    }
    CheckProtocol(socket, protocol);
    std::uint32_t size = 4;
    int value = 99;
    Require(sceNetGetsockopt(socket, 0xffff, 0x1016, nullptr, &size) < 0 && *sceNetErrnoLoc() == 22);
    Require(sceNetGetsockopt(socket, 0xffff, 0x1016, &value, nullptr) < 0 && *sceNetErrnoLoc() == 22);
    Require(value == 99 && size == 4);
    Require(sceNetSocketClose(socket) == 0);
    const int unlistedProtocol = 17;
    const int unlisted = sceNetSocket("raw-unlisted-protocol", 2, 3, unlistedProtocol);
    if (unlisted < 0) {
        const int error = *sceNetErrnoLoc();
        if (error == 1 || error == 13) return 77;
        Require(false);
    }
    CheckProtocol(unlisted, 0);
    std::uint32_t unlistedSize = 4;
    int unlistedValue = 99;
    Require(sceNetGetsockopt(unlisted, 0xffff, 0x1016, nullptr, &unlistedSize) < 0 && *sceNetErrnoLoc() == 22);
    Require(sceNetGetsockopt(unlisted, 0xffff, 0x1016, &unlistedValue, nullptr) < 0 && *sceNetErrnoLoc() == 22);
    Require(unlistedValue == 99 && unlistedSize == 4);
    Require(sceNetSocketClose(unlisted) == 0);
}
