#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Memory.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() {
        words.fill(Sentinel);
    }
};

using Writer = std::uint32_t* (APS5_VABI *)(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint8_t, const volatile void*, std::uint64_t, std::uint64_t, std::uint16_t);

void testPacket(Writer writer, std::uint32_t size, std::uint8_t atomicOp, std::uintptr_t address) {
    check(size == 36, "ATOMIC_MEM packet size");
    Storage storage;
    auto* packet = writer(&storage.buffer, atomicOp, 0x5, 2, reinterpret_cast<const volatile void*>(address), 0x1122334455667788ull, 0x99aabbccddeeff00ull, 0x1abc);
    const std::array<std::uint32_t, 9> expected{
        0xc0071e00u,
        atomicOp | (0x5u << 8u) | (2u << 25u),
        static_cast<std::uint32_t>(address),
        static_cast<std::uint32_t>(address >> 32u),
        0x55667788u,
        0x11223344u,
        0xddeeff00u,
        0x99aabbccu,
        0x1abcu,
    };
    check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect ATOMIC_MEM packet");
    check(storage.buffer.cursor_up == packet + size / sizeof(std::uint32_t), "the cursor did not advance by the packet size");
    check(std::all_of(storage.words.begin() + expected.size(), storage.words.end(), [](std::uint32_t word) { return word == Sentinel; }), "ATOMIC_MEM wrote past its nine words");
}

}

int main() {
    try {
        testPacket(sceAgcDcbAtomicMem, sceAgcDcbAtomicMemGetSize(), 0x2f, 0x0000123456789ab4ull);
        testPacket(sceAgcDcbAtomicMem, sceAgcDcbAtomicMemGetSize(), 0x6f, 0x0000123456789ab8ull);
        testPacket(sceAgcAcbAtomicMem, sceAgcAcbAtomicMemGetSize(), 0x2f, 0x0000123456789ab4ull);
        testPacket(sceAgcAcbAtomicMem, sceAgcAcbAtomicMemGetSize(), 0x6f, 0x0000123456789ab8ull);
        std::puts("AGC atomic memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
