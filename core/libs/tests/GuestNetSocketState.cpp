#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "prx/libSceNet/libSceNet.hpp"


static void Require(bool condition) {
    if (!condition) std::abort();
}

static void CheckOption(int socket, int option, int expected) {
    for (const std::uint32_t capacity : {0u, 1u, 3u, 4u, 8u}) {
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        std::uint32_t size = capacity;
        Require(sceNetGetsockopt(socket, 0xffff, option, bytes.data(), &size) == 0);
        Require(size == std::min(capacity, 4u));
        Require(std::memcmp(bytes.data(), &expected, size) == 0);
        Require(std::all_of(bytes.begin() + size, bytes.end(), [](auto byte) { return byte == 0xa5; }));
    }
}

static void RequireError(int result, int error) {
    Require(result == static_cast<int>(0x80410100u | static_cast<std::uint32_t>(error)));
    Require(*sceNetErrnoLoc() == error);
}

static void CheckStateErrors(int family) {
    const int datagram = sceNetSocket("errors-datagram", family, 2, 17);
    const int stream = sceNetSocket("errors-stream", family, 1, 6);
    const int closed = sceNetSocket("errors-closed", family, 1, 6);
    Require(datagram >= 0 && stream >= 0 && closed >= 0);
    Require(sceNetSocketClose(closed) == 0);
    for (const int how : {-1, 3}) RequireError(sceNetShutdown(stream, how), 22);
    RequireError(sceNetShutdown(closed, 1), 9);
    RequireError(sceNetListen(closed, 1), 9);
    RequireError(sceNetListen(datagram, 1), 45);
    RequireError(sceNetAccept(closed, nullptr, nullptr), 9);
    RequireError(sceNetAccept(stream, nullptr, nullptr), 22);
    std::array<std::uint8_t, 28> address{};
    std::uint32_t size = family == 2 ? 16 : 28;
    address[0] = static_cast<std::uint8_t>(size);
    address[1] = static_cast<std::uint8_t>(family);
    RequireError(sceNetBind_nid_postfix(closed, address.data(), size), 9);
    RequireError(sceNetGetsockname(closed, address.data(), &size), 9);
    for (const int socket : {stream, datagram}) Require(sceNetSocketClose(socket) == 0);
}

int main() {
    Require(sceNetInit_nid_postfix() == 0);
    for (const int family : {2, 28}) {
        CheckStateErrors(family);
        const int datagram = sceNetSocket("state-datagram", family, 2, 17);
        const int listener = sceNetSocket("state-listener", family, 1, 6);
        const int client = sceNetSocket("state-client", family, 1, 6);
        Require(datagram >= 0 && listener >= 0 && client >= 0);
        CheckOption(datagram, 0x1008, 2);
        CheckOption(datagram, 2, 0);
        CheckOption(listener, 0x1008, 1);
        CheckOption(listener, 2, 0);
        std::array<std::uint8_t, 28> address{};
        const std::uint32_t addressSize = family == 2 ? 16 : 28;
        address[0] = addressSize;
        address[1] = family;
        if (family == 2) {
            address[4] = 127;
            address[7] = 1;
        } else {
            address[23] = 1;
        }
        Require(sceNetBind_nid_postfix(listener, address.data(), addressSize) == 0);
        Require(sceNetListen(listener, 1) == 0);
        CheckOption(listener, 2, 2);
        CheckOption(client, 2, 0);
        std::uint32_t size = addressSize;
        Require(sceNetGetsockname(listener, address.data(), &size) == 0);
        Require(sceNetConnect(client, address.data(), size) == 0);
        const int accepted = sceNetAccept(listener, nullptr, nullptr);
        Require(accepted >= 0);
        CheckOption(listener, 2, 2);
        CheckOption(client, 0x1008, 1);
        CheckOption(client, 2, 0);
        CheckOption(accepted, 0x1008, 1);
        CheckOption(accepted, 2, 0);
        int value = 0;
        size = sizeof(value);
        for (const int option : {2, 0x1008}) {
            Require(sceNetGetsockopt(listener, 0xffff, option, nullptr, &size) == static_cast<int>(0x80410116u));
            Require(*sceNetErrnoLoc() == 22);
            Require(sceNetGetsockopt(listener, 0xffff, option, &value, nullptr) == static_cast<int>(0x80410116u));
            Require(*sceNetErrnoLoc() == 22);
        }
        for (const int socket : {accepted, client, listener, datagram}) Require(sceNetSocketClose(socket) == 0);
        for (const int option : {2, 0x1008}) {
            Require(sceNetGetsockopt(listener, 0xffff, option, &value, &size) == static_cast<int>(0x80410109u));
            Require(*sceNetErrnoLoc() == 9);
        }
    }
}
