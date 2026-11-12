#include "prx/libSceRudp/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) {
    if (!value) std::abort();
}

static void APS5_VABI Handler(int, int, int, void*) {}

int main() {
    alignas(16) static std::array<unsigned char, 0x10000> pool{};
    Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0);

    const std::array<std::uint8_t, 16> loopback{16, 2, 0, 0, 127, 0, 0, 1};
    const int socket = sceNetSocket("rudp-local", 2, 2, 0);
    Require(socket >= 0);
    Require(sceNetBind_nid_postfix(socket, loopback.data(), static_cast<std::uint32_t>(loopback.size())) == 0);

    std::array<std::uint8_t, 0x1c> expected{};
    std::uint32_t expectedSize = static_cast<std::uint32_t>(expected.size());
    Require(sceNetGetsockname(socket, expected.data(), &expectedSize) == 0);
    Require(expectedSize == 16 && expected[0] == 16 && expected[1] == 2);

    int context = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &context) == 0);
    Require(sceRudpBind(context, socket, 0x2711, 1) == 0);

    int socketId = -1;
    std::uint16_t vport = 0;
    std::uint8_t muxMode = 0;
    std::array<std::uint8_t, 0x40> address{};
    std::uint32_t addressLen = 0x40;
    Require(sceRudpGetLocalInfo(context, &socketId, address.data(), &addressLen, &vport, &muxMode) == 0);
    Require(socketId == socket && vport == 0x2711 && muxMode == 1);
    Require(addressLen == 0x1c);
    Require(std::memcmp(address.data(), expected.data(), expectedSize) == 0);
    for (std::size_t index = expectedSize; index < 0x1c; ++index) {
        Require(address[index] == 0);
    }

    for (const std::uint32_t requested : {0x1du, 0xffffffffu, 0x80000000u}) {
        std::array<std::uint8_t, 0x40> again{};
        std::uint32_t length = requested;
        Require(sceRudpGetLocalInfo(context, nullptr, again.data(), &length, nullptr, nullptr) == 0);
        Require(length == 0x1c);
        Require(std::memcmp(again.data(), expected.data(), expectedSize) == 0);
    }

    {
        std::array<std::uint8_t, 8> small{};
        std::uint32_t length = 4;
        Require(sceRudpGetLocalInfo(context, nullptr, small.data(), &length, nullptr, nullptr) == 0);
        Require(length == 4);
        Require(std::memcmp(small.data(), expected.data(), 4) == 0);
        Require(small[4] == 0 && small[5] == 0 && small[6] == 0 && small[7] == 0);
    }

    {
        std::array<std::uint8_t, 16> zero{};
        zero.fill(0x5a);
        std::uint32_t length = 0;
        Require(sceRudpGetLocalInfo(context, nullptr, zero.data(), &length, nullptr, nullptr) == 0);
        Require(length == 0);
        for (const auto byte : zero) {
            Require(byte == 0x5a);
        }
    }

    {
        std::array<std::uint8_t, 16> noLength{};
        noLength.fill(0x5a);
        Require(sceRudpGetLocalInfo(context, nullptr, noLength.data(), nullptr, nullptr, nullptr) == 0);
        for (const auto byte : noLength) {
            Require(byte == 0x5a);
        }
    }

    const std::array<std::uint8_t, 28> any6{28, 28};
    const int socket6 = sceNetSocket("rudp-local6", 28, 2, 0);
    Require(socket6 >= 0);
    Require(sceNetBind_nid_postfix(socket6, any6.data(), static_cast<std::uint32_t>(any6.size())) == 0);
    std::array<std::uint8_t, 28> expected6{};
    std::uint32_t expected6Size = static_cast<std::uint32_t>(expected6.size());
    Require(sceNetGetsockname(socket6, expected6.data(), &expected6Size) == 0);
    Require(expected6Size == 28 && expected6[0] == 28 && expected6[1] == 28);
    int context6 = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &context6) == 0);
    Require(sceRudpBind(context6, socket6, 0x2712, 1) == 0);
    std::array<std::uint8_t, 0x40> address6{};
    std::uint32_t address6Len = 0x40;
    Require(sceRudpGetLocalInfo(context6, nullptr, address6.data(), &address6Len, nullptr, nullptr) == 0);
    Require(address6Len == 0x1c);
    Require(std::memcmp(address6.data(), expected6.data(), expected6.size()) == 0);

    int broken = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &broken) == 0);
    Require(sceRudpBind(broken, 12345, 0x2713, 1) == 0);
    {
        std::array<std::uint8_t, 0x20> rejected{};
        std::uint32_t length = 0x20;
        bool thrown = false;
        try {
            sceRudpGetLocalInfo(broken, nullptr, rejected.data(), &length, nullptr, nullptr);
        } catch (...) {
            thrown = true;
        }
        Require(thrown);
        Require(length == 0x20);
        for (const auto byte : rejected) {
            Require(byte == 0);
        }
    }

    Require(sceNetSocketClose(socket) == 0);
    Require(sceNetSocketClose(socket6) == 0);
    Require(sceRudpEnd() == 0);
    return 0;
}
