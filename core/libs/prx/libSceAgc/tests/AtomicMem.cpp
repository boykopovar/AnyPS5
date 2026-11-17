#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "Testing.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() {
        words.fill(Sentinel);
    }
};

using Writer = std::uint32_t* (APS5_VABI *)(CommandBuffer*, std::uint8_t, std::uint8_t, const volatile void*, std::uint8_t, std::uint64_t, std::uint64_t, std::uint16_t);

std::uint32_t* APS5_VABI writeDcb(CommandBuffer* buffer, std::uint8_t atomicOp, std::uint8_t command, const volatile void* address, std::uint8_t cachePolicy, std::uint64_t srcData, std::uint64_t compareData, std::uint16_t loopInterval) {
    return sceAgcDcbAtomicMem(buffer, 1, atomicOp, command, cachePolicy, address, srcData, compareData, loopInterval);
}

void testPacket(Writer writer, std::uint32_t size, std::uint8_t atomicOp, std::uintptr_t address, std::uint32_t engineBits) {
    Require(size == 36, "ATOMIC_MEM packet size");
    Storage storage;
    auto* packet = writer(&storage.buffer, atomicOp, 0x5, reinterpret_cast<const volatile void*>(address), 2, 0x1122334455667788ull, 0x99aabbccddeeff00ull, 0x1abc);
    const std::array<std::uint32_t, 9> expected{
        0xc0071e00u,
        atomicOp | (0x5u << 8u) | (2u << 25u) | engineBits,
        static_cast<std::uint32_t>(address),
        static_cast<std::uint32_t>(address >> 32u),
        0x55667788u,
        0x11223344u,
        0xddeeff00u,
        0x99aabbccu,
        0x1abcu,
    };
    Require(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect ATOMIC_MEM packet");
    Require(storage.buffer.cursor_up == packet + size / sizeof(std::uint32_t), "the cursor did not advance by the packet size");
    Require(std::all_of(storage.words.begin() + expected.size(), storage.words.end(), [](std::uint32_t word) { return word == Sentinel; }), "ATOMIC_MEM wrote past its nine words");
}

void testMasking() {
    Storage compute;
    const auto* packet = sceAgcAcbAtomicMem(&compute.buffer, 0xff, 0xff, reinterpret_cast<const volatile void*>(0x1002ull), 0xff, 0, 0, 0xffff);
    Require(packet[1] == (0x7fu | 0xf00u | (3u << 25u)) && packet[2] == 0x1002u && packet[8] == 0x1fffu, "compute ATOMIC_MEM did not mask its fields");
    Storage draw;
    const auto* drawPacket = sceAgcDcbAtomicMem(&draw.buffer, 0xff, 0xff, 0xff, 0xff, reinterpret_cast<const volatile void*>(0x1002ull), 0, 0, 0xffff);
    Require(drawPacket[1] == (0x7fu | 0xf00u | (3u << 25u) | 0xc0000000u) && drawPacket[2] == 0x1002u && drawPacket[8] == 0x1fffu, "draw ATOMIC_MEM did not mask its fields");
}

}

int main() {
    try {
        testPacket(writeDcb, sceAgcDcbAtomicMemGetSize(), 0x2f, 0x0000123456789ab4ull, 0x40000000u);
        testPacket(writeDcb, sceAgcDcbAtomicMemGetSize(), 0x6f, 0x0000123456789ab8ull, 0x40000000u);
        testPacket(sceAgcAcbAtomicMem, sceAgcAcbAtomicMemGetSize(), 0x2f, 0x0000123456789ab4ull, 0);
        testPacket(sceAgcAcbAtomicMem, sceAgcAcbAtomicMemGetSize(), 0x6f, 0x0000123456789ab8ull, 0);
        testMasking();
        std::puts("AGC atomic memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
