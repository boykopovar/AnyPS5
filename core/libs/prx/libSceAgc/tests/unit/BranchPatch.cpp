#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbBranch(CommandBuffer*, std::uint8_t, std::uint8_t, const volatile std::uint64_t*, std::uint64_t, std::uint64_t, std::uint8_t, const volatile std::uint32_t*, std::uint32_t, std::uint8_t, const volatile std::uint32_t*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcCbBranchGetSize();
int APS5_VABI sceAgcBranchPatchSetCompareAddress(std::uint32_t*, const volatile std::uint64_t*);
int APS5_VABI sceAgcBranchPatchSetThenTarget(std::uint32_t*, const volatile std::uint32_t*, std::uint32_t);
int APS5_VABI sceAgcBranchPatchSetElseTarget(std::uint32_t*, const volatile std::uint32_t*, std::uint32_t);
}

namespace {

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    Testing::RequireThrows<std::runtime_error>(action, "expected invalid input to fail");
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

void VerifyWriter() {
    Storage storage;
    const std::array expected{0xc00c3f00u, 0x302u, 0x56789ab8u, 0x1234u, 0x55667788u, 0x11223344u, 0xddeeff00u, 0x99aabbccu, 0x33445564u, 0x22u, 0x10000123u, 0x77889900u, 0x66u, 0x20000456u};
    Require(storage.packet == storage.words.data() && std::equal(expected.begin(), expected.end(), storage.packet), "incorrect branch packet");
    Require(storage.buffer.cursor_up == storage.packet + expected.size() && sceAgcCbBranchGetSize() == expected.size() * sizeof(std::uint32_t), "branch size/cursor mismatch");
    Require(storage.packet[expected.size()] == 0xabcdef01u, "branch command overwrote following word");
}

void VerifyUnusedFields() {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    auto* packet = sceAgcCbBranch(&buffer, 1, 0, compareAt(0x0000001234567894ull), 0, 0, 0, targetAt(0x0000002233445564ull), 0x123u, 0, nullptr, 0);
    const std::array expected{0xc00c3f00u, 0x1u, 0x34567894u, 0x12u, 0u, 0u, 0u, 0u, 0x33445564u, 0x22u, 0x123u, 0u, 0u, 0u};
    Require(std::equal(expected.begin(), expected.end(), packet), "an always-taken branch without an else target was not written");
    packet = sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, nullptr, 0, 0, nullptr, 0);
    Require(packet[1] == 1u && packet[2] == 0u && packet[8] == 0u && packet[10] == 0u, "an empty always-taken branch was not written");
    const auto cursor = buffer.cursor_up;
    ExpectFailure([&] { sceAgcCbBranch(&buffer, 1, 3, compareAt(0x0000001234567894ull), 0, 0, 0, targetAt(0x2000u), 1, 0, nullptr, 0); });
    ExpectFailure([&] { sceAgcCbBranch(&buffer, 1, 0, nullptr, 0, 0, 0, nullptr, 1, 0, nullptr, 0); });
    ExpectFailure([&] { sceAgcCbBranch(&buffer, 2, 0, nullptr, 0, 0, 0, targetAt(0x2000u), 1, 0, nullptr, 1); });
    Require(buffer.cursor_up == cursor, "a rejected branch advanced the command buffer");
}

void VerifyCompareAddress() {
    Storage storage;
    auto expected = storage.words;
    expected[2] = 0xfedcba98u;
    expected[3] = 0x7654u;
    Require(sceAgcBranchPatchSetCompareAddress(storage.packet, compareAt(0x00007654fedcba98ull)) == 0, "compare address setter failed");
    Require(storage.words == expected, "compare address setter wrote the wrong words");
    ExpectFailure([&] { sceAgcBranchPatchSetCompareAddress(storage.packet, nullptr); });
    ExpectFailure([&] { sceAgcBranchPatchSetCompareAddress(storage.packet, compareAt(0x00007654fedcba9cull)); });
    ExpectFailure([&] { sceAgcBranchPatchSetCompareAddress(nullptr, compareAt(0x1000u)); });
    Require(storage.words == expected, "invalid compare address modified the packet");
}

template <typename TSetter>
void VerifyTarget(TSetter setter, std::size_t field, std::uint32_t cachePolicyBits, const char* name) {
    Storage storage;
    auto expected = storage.words;
    expected[field] = 0x89abcdecu;
    expected[field + 1] = 0x4567u;
    expected[field + 2] = cachePolicyBits | 0xfffffu;
    Require(setter(storage.packet, targetAt(0x0000456789abcdecull), 0xfffffu) == 0, name);
    Require(storage.words == expected, name);
    expected[field] = 0x1000u;
    expected[field + 1] = 0;
    expected[field + 2] = cachePolicyBits;
    Require(setter(storage.packet, targetAt(0x1000u), 0) == 0, name);
    Require(storage.words == expected, name);
    ExpectFailure([&] { setter(storage.packet, nullptr, 1); });
    ExpectFailure([&] { setter(storage.packet, targetAt(0x1002u), 1); });
    ExpectFailure([&] { setter(storage.packet, targetAt(0x2000u), 0x100000u); });
    ExpectFailure([&] { setter(nullptr, targetAt(0x2000u), 1); });
    Require(storage.words == expected, name);
}

void VerifyMalformedPackets() {
    for (const auto header : {0xc0023f00u, 0xc00b3f00u, 0xc00c3f01u, 0xc00c2200u}) {
        Storage storage;
        storage.packet[0] = header;
        const auto malformed = storage.words;
        ExpectFailure([&] { sceAgcBranchPatchSetCompareAddress(storage.packet, compareAt(0x1000u)); });
        ExpectFailure([&] { sceAgcBranchPatchSetThenTarget(storage.packet, targetAt(0x2000u), 1); });
        ExpectFailure([&] { sceAgcBranchPatchSetElseTarget(storage.packet, targetAt(0x2000u), 1); });
        Require(storage.words == malformed, "setter modified a packet that is not a branch");
    }
}

}

namespace {

const Testing::Case writer{"CbBranch_AllFields_WritesPacketAndAdvancesCursor", [] {
    VerifyWriter();
}};

const Testing::Case unusedFields{"CbBranch_UnusedOrInvalidFields_WritesOrRejectsWithoutAdvancing", [] {
    VerifyUnusedFields();
}};

const Testing::Case compareAddress{"BranchPatchSetCompareAddress_ValidOrInvalidAddress_PatchesOrRejects", [] {
    VerifyCompareAddress();
}};

const Testing::Case thenTarget{"BranchPatchSetThenTarget_ValidOrInvalidTarget_PatchesOrRejects", [] {
    VerifyTarget(sceAgcBranchPatchSetThenTarget, 8, 0x10000000u, "then target setter wrote the wrong words");
}};

const Testing::Case elseTarget{"BranchPatchSetElseTarget_ValidOrInvalidTarget_PatchesOrRejects", [] {
    VerifyTarget(sceAgcBranchPatchSetElseTarget, 11, 0x20000000u, "else target setter wrote the wrong words");
}};

const Testing::Case malformed{"BranchPatchSetters_NonBranchPacket_RejectWithoutWriting", [] {
    VerifyMalformedPackets();
}};

} // namespace
