#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <utility>

static void Require(bool condition) {
    if (!condition) std::abort();
}

template<typename TAction>
static void Reject(TAction action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
}

struct PacketWords {
    std::array<std::uint32_t, 8> words{};
};

static std::array<std::uint32_t, 8>* Acquire(PacketWords& packet, std::uint8_t engine, std::uint32_t cbDbOp, std::uint32_t gcrControl, const volatile void* base, std::uint64_t sizeBytes, std::uint32_t pollCycles) {
    CommandBuffer buffer{packet.words.data(), packet.words.data() + packet.words.size(), packet.words.data(), packet.words.data() + packet.words.size(), nullptr, nullptr, 0};
    auto* written = sceAgcDcbAcquireMem(&buffer, engine, cbDbOp, gcrControl, base, sizeBytes, pollCycles);
    Require(written == packet.words.data());
    return &packet.words;
}

int main() {
    Require(sceAgcDcbAcquireMemGetSize() == 32);

    for (const auto [pollCycles, pollWord] : std::array<std::pair<std::uint32_t, std::uint32_t>, 10>{{
             {0u, 0u},
             {15u, 0u},
             {16u, 1u},
             {0x100u, 0x10u},
             {1000000u, 62500u},
             {0xfffff0u, 0xffffu},
             {0x100000u, 0xffffu},
             {0x1000000u, 0xffffu},
             {0xffffffffu, 0xffffu},
             {0x44444444u, 0xffffu},
         }}) {
        PacketWords packet;
        auto* words = Acquire(packet, 0, 0, 0, reinterpret_cast<const volatile void*>(0x40000000ull), 0x1000ull, pollCycles);
        Require((*words)[6] == pollWord);
    }

    for (const auto [gcrControl, emitted] : std::array<std::pair<std::uint32_t, std::uint32_t>, 4>{{
             {0u, 0u},
             {0x3fffu, 0x3fffu},
             {0x4000u, 0xc000u},
             {0x4001u, 0xc001u},
         }}) {
        PacketWords packet;
        auto* words = Acquire(packet, 0, 0, gcrControl, reinterpret_cast<const volatile void*>(0x40000000ull), 0x1000ull, 0u);
        Require((*words)[7] == emitted);
    }

    {
        PacketWords packet;
        auto* words = Acquire(packet, 1, 0x1234u, 0, reinterpret_cast<const volatile void*>(0x40000000ull), 0x1000ull, 0u);
        Require((*words)[1] == (0x80000000u | 0x1234u));
        Require((*words)[2] == 0x1000u >> 8);
        Require((*words)[3] == 0 && (*words)[4] == 0x40000000u >> 8);
    }

    for (const std::uint64_t sizeBytes : {0x10000000000ull, 0x10000000001ull, 0xffffffffffffffffull}) {
        PacketWords packet;
        auto* words = Acquire(packet, 0, 0, 0, reinterpret_cast<const volatile void*>(0x40000000ull), sizeBytes, 0u);
        Require((*words)[2] == 0 && (*words)[3] == 0 && (*words)[4] == 0);
    }

    {
        PacketWords packet;
        CommandBuffer buffer{packet.words.data(), packet.words.data() + packet.words.size(), packet.words.data(), packet.words.data() + packet.words.size(), nullptr, nullptr, 0};
        Reject([&] { sceAgcDcbAcquireMem(&buffer, 2, 0, 0, reinterpret_cast<const volatile void*>(0x40000000ull), 0x1000ull, 0u); });
        Reject([&] { sceAgcDcbAcquireMem(&buffer, 0, 0, 0x80000u, reinterpret_cast<const volatile void*>(0x40000000ull), 0x1000ull, 0u); });
    }
}
