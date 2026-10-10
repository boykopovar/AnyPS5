#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbBranch(CommandBuffer*, std::uint8_t, std::uint8_t, const volatile std::uint64_t*, std::uint64_t, std::uint64_t, std::uint8_t, const volatile std::uint32_t*, std::uint32_t, std::uint8_t, const volatile std::uint32_t*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcCbBranchGetSize();
int APS5_VABI sceAgcBranchPatchSetCompareAddress(std::uint32_t*, const volatile std::uint64_t*);
int APS5_VABI sceAgcBranchPatchSetThenTarget(std::uint32_t*, const volatile std::uint32_t*, std::uint32_t);
int APS5_VABI sceAgcBranchPatchSetElseTarget(std::uint32_t*, const volatile std::uint32_t*, std::uint32_t);
}

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("expected invalid input to fail");
}

const volatile std::uint64_t* compareAt(std::uint64_t address) {
    return reinterpret_cast<const volatile std::uint64_t*>(static_cast<std::uintptr_t>(address));
}

const volatile std::uint32_t* targetAt(std::uint64_t address) {
    return reinterpret_cast<const volatile std::uint32_t*>(static_cast<std::uintptr_t>(address));
}

struct Storage {
    std::array<std::uint32_t, 20> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    std::uint32_t* packet = nullptr;

    Storage() {
        words.fill(0xabcdef01u);
        packet = sceAgcCbBranch(&buffer, 2, 3, compareAt(0x0000123456789ab8ull), 0x1122334455667788ull, 0x99aabbccddeeff00ull, 1, targetAt(0x0000002233445564ull), 0x123u, 2, targetAt(0x0000006677889900ull), 0x456u);
    }
};

void testWriter() {
    Storage storage;
    const std::array expected{0xc00c3f00u, 0x302u, 0x56789ab8u, 0x1234u, 0x55667788u, 0x11223344u, 0xddeeff00u, 0x99aabbccu, 0x33445564u, 0x22u, 0x10000123u, 0x77889900u, 0x66u, 0x20000456u};
    check(storage.packet == storage.words.data() && std::equal(expected.begin(), expected.end(), storage.packet), "incorrect branch packet");
    check(storage.buffer.cursor_up == storage.packet + expected.size() && sceAgcCbBranchGetSize() == expected.size() * sizeof(std::uint32_t), "branch size/cursor mismatch");
    check(storage.packet[expected.size()] == 0xabcdef01u, "branch command overwrote following word");
}

void testUnusedFields() {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    auto* packet = sceAgcCbBranch(&buffer, 1, 0, compareAt(0x0000001234567894ull), 0, 0, 0, targetAt(0x0000002233445564ull), 0x123u, 0, nullptr, 0);
    const std::array expected{0xc00c3f00u, 0x1u, 0x34567894u, 0x12u, 0u, 0u, 0u, 0u, 0x33445564u, 0x22u, 0x123u, 0u, 0u, 0u};
    check(std::equal(expected.begin(), expected.end(), packet), "an always-taken branch without an else target was not written");
    packet = sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, nullptr, 0, 0, nullptr, 0);
    check(packet[1] == 1u && packet[2] == 0u && packet[8] == 0u && packet[10] == 0u, "an empty always-taken branch was not written");
    const auto cursor = buffer.cursor_up;
    expectFailure([&] { sceAgcCbBranch(&buffer, 1, 3, compareAt(0x0000001234567894ull), 0, 0, 0, targetAt(0x2000u), 1, 0, nullptr, 0); });
    expectFailure([&] { sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, nullptr, 1, 0, nullptr, 0); });
    expectFailure([&] { sceAgcCbBranch(&buffer, 2, 0, nullptr, 0, 0, 0, targetAt(0x2000u), 1, 0, nullptr, 1); });
    check(buffer.cursor_up == cursor, "a rejected branch advanced the command buffer");
}

void testCompareAddress() {
    Storage storage;
    auto expected = storage.words;
    expected[2] = 0xfedcba98u;
    expected[3] = 0x7654u;
    check(sceAgcBranchPatchSetCompareAddress(storage.packet, compareAt(0x00007654fedcba98ull)) == 0, "compare address setter failed");
    check(storage.words == expected, "compare address setter wrote the wrong words");
    expectFailure([&] { sceAgcBranchPatchSetCompareAddress(storage.packet, nullptr); });
    expectFailure([&] { sceAgcBranchPatchSetCompareAddress(storage.packet, compareAt(0x00007654fedcba9cull)); });
    expectFailure([&] { sceAgcBranchPatchSetCompareAddress(nullptr, compareAt(0x1000u)); });
    check(storage.words == expected, "invalid compare address modified the packet");
}

template <typename TSetter>
void testTarget(TSetter setter, std::size_t field, std::uint32_t cachePolicyBits, const char* name) {
    Storage storage;
    auto expected = storage.words;
    expected[field] = 0x89abcdecu;
    expected[field + 1] = 0x4567u;
    expected[field + 2] = cachePolicyBits | 0xfffffu;
    check(setter(storage.packet, targetAt(0x0000456789abcdecull), 0xfffffu) == 0, name);
    check(storage.words == expected, name);
    expected[field] = 0x1000u;
    expected[field + 1] = 0;
    expected[field + 2] = cachePolicyBits;
    check(setter(storage.packet, targetAt(0x1000u), 0) == 0, name);
    check(storage.words == expected, name);
    expectFailure([&] { setter(storage.packet, nullptr, 1); });
    expectFailure([&] { setter(storage.packet, targetAt(0x1002u), 1); });
    expectFailure([&] { setter(storage.packet, targetAt(0x2000u), 0x100000u); });
    expectFailure([&] { setter(nullptr, targetAt(0x2000u), 1); });
    check(storage.words == expected, name);
}

void testMalformedPackets() {
    for (const auto header : {0xc0023f00u, 0xc00b3f00u, 0xc00c3f01u, 0xc00c2200u}) {
        Storage storage;
        storage.packet[0] = header;
        const auto malformed = storage.words;
        expectFailure([&] { sceAgcBranchPatchSetCompareAddress(storage.packet, compareAt(0x1000u)); });
        expectFailure([&] { sceAgcBranchPatchSetThenTarget(storage.packet, targetAt(0x2000u), 1); });
        expectFailure([&] { sceAgcBranchPatchSetElseTarget(storage.packet, targetAt(0x2000u), 1); });
        check(storage.words == malformed, "setter modified a packet that is not a branch");
    }
}

}

int main() {
    try {
        testWriter();
        testUnusedFields();
        testCompareAddress();
        testTarget(sceAgcBranchPatchSetThenTarget, 8, 0x10000000u, "then target setter wrote the wrong words");
        testTarget(sceAgcBranchPatchSetElseTarget, 11, 0x20000000u, "else target setter wrote the wrong words");
        testMalformedPackets();
        std::puts("AGC branch patch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
