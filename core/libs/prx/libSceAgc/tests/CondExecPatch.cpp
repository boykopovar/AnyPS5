#include "prx/libSceAgc/Patch/include/CondExec.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "Testing.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

using CondExecWriter = std::uint32_t* (APS5_VABI *)(CommandBuffer*, const volatile std::uint32_t*, std::uint32_t);
using PatchFunction = int (APS5_VABI *)(std::uint32_t*, const volatile std::uint32_t*);

struct Variant {
    CondExecWriter write;
    PatchFunction setEnd;
    PatchFunction setCommandAddress;
};

const volatile std::uint32_t* at(std::uintptr_t address) {
    return reinterpret_cast<const volatile std::uint32_t*>(address);
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    std::uint32_t condition = 1;
    std::uint32_t* packet = nullptr;

    explicit Storage(CondExecWriter write) {
        words.fill(Sentinel);
        packet = write(&buffer, &condition, 3);
        Require(packet == words.data() && packet[0] == 0xc0032200u && packet[4] == 3, "unexpected COND_EXEC packet");
    }
};

std::uintptr_t endAfter(const std::uint32_t* packet, std::uintptr_t numDwords) {
    return reinterpret_cast<std::uintptr_t>(packet + 5) + numDwords * sizeof(std::uint32_t);
}

template <typename TAction>
void expectUnchanged(const Variant& variant, TAction action) {
    Storage storage(variant.write);
    const auto before = storage.words;
    ExpectErr<std::runtime_error>([&] { action(storage.packet); });
    Require(storage.words == before, "failed patch modified the packet");
}

void testSetEnd(const Variant& variant) {
    const auto setEnd = variant.setEnd;
    for (const std::uintptr_t numDwords : {0u, 1u, 7u, 0x3fffu}) {
        Storage storage(variant.write);
        const auto before = storage.words;
        Require(setEnd(storage.packet, at(endAfter(storage.packet, numDwords))) == 0, "SetEnd failed");
        Require(storage.packet[4] == numDwords, "SetEnd wrote the wrong dword count");
        Require(std::equal(before.begin(), before.begin() + 4, storage.words.begin()), "SetEnd modified other packet words");
        Require(std::equal(before.begin() + 5, before.end(), storage.words.begin() + 5), "SetEnd modified following words");
    }

    expectUnchanged(variant, [=](std::uint32_t* packet) { setEnd(packet, at(endAfter(packet, 0x4000u))); });
    expectUnchanged(variant, [=](std::uint32_t* packet) { setEnd(packet, packet + 4); });
    expectUnchanged(variant, [=](std::uint32_t* packet) { setEnd(packet, nullptr); });
    expectUnchanged(variant, [=](std::uint32_t* packet) { setEnd(packet, at(endAfter(packet, 2) + 2)); });
    expectUnchanged(variant, [=](std::uint32_t* packet) { setEnd(packet + 1, at(endAfter(packet, 2))); });
}

void testSetCommandAddress(const Variant& variant) {
    const auto setCommandAddress = variant.setCommandAddress;
    for (const std::uintptr_t address : {std::uintptr_t{0x0000123456789abcu}, std::uintptr_t{4u}, std::uintptr_t{0x0000fffffffffffcu}}) {
        Storage storage(variant.write);
        storage.packet[1] |= 3u;
        const auto before = storage.words;
        Require(setCommandAddress(storage.packet, at(address)) == 0, "SetCommandAddress failed");
        Require(storage.packet[1] == (static_cast<std::uint32_t>(address) | 3u) && storage.packet[2] == static_cast<std::uint32_t>(address >> 32u), "SetCommandAddress wrote the wrong address");
        Require(storage.packet[0] == before[0] && storage.packet[3] == before[3] && storage.packet[4] == before[4], "SetCommandAddress modified other packet words");
        Require(std::equal(before.begin() + 5, before.end(), storage.words.begin() + 5), "SetCommandAddress modified following words");
    }

    expectUnchanged(variant, [=](std::uint32_t* packet) { setCommandAddress(packet, nullptr); });
    expectUnchanged(variant, [=](std::uint32_t* packet) { setCommandAddress(packet, at(0x1002u)); });
    expectUnchanged(variant, [=](std::uint32_t* packet) { setCommandAddress(packet + 1, at(0x1000u)); });
}

}

int main() {
    try {
        for (const Variant& variant : {Variant{sceAgcDcbCondExec, sceAgcCondExecPatchSetEnd, sceAgcCondExecPatchSetCommandAddress},
                                       Variant{sceAgcAcbCondExec, sceAgcAsyncCondExecPatchSetEnd, sceAgcAsyncCondExecPatchSetCommandAddress}}) {
            testSetEnd(variant);
            testSetCommandAddress(variant);
        }
        std::puts("AGC COND_EXEC patch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
