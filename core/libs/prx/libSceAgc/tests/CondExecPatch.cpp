#include "prx/libSceAgc/Patch/include/CondExec.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr int InvalidPacket = static_cast<int>(0x8a6c000cu);

using CondExecWriter = std::uint32_t* (APS5_VABI *)(CommandBuffer*, const volatile std::uint32_t*, std::uint32_t);
using PatchFunction = int (APS5_VABI *)(std::uint32_t*, const volatile std::uint32_t*);

struct Variant {
    CondExecWriter write;
    PatchFunction setEnd;
    PatchFunction setCommandAddress;
};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

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
        check(packet == words.data() && packet[0] == 0xc0032200u && packet[4] == 3, "unexpected COND_EXEC packet");
    }
};

std::uintptr_t endAfter(const std::uint32_t* packet, std::uintptr_t numDwords) {
    return reinterpret_cast<std::uintptr_t>(packet + 5) + numDwords * sizeof(std::uint32_t);
}

template <typename TAction>
void expectInvalidPacket(const Variant& variant, TAction action) {
    Storage storage(variant.write);
    const auto before = storage.words;
    const int status = action(storage.packet);
    check(status == InvalidPacket, "expected invalid packet error code");
    check(storage.words == before, "invalid packet patch modified storage");
}

void testSetEnd(const Variant& variant) {
    const auto setEnd = variant.setEnd;
    for (const std::uintptr_t numDwords : {0u, 1u, 7u, 0x3fffu}) {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        const auto before = storage.words;
        check(setEnd(storage.packet, at(endAfter(storage.packet, numDwords))) == 0, "SetEnd failed");
        check(storage.packet[4] == (0xabcd0000u | static_cast<std::uint32_t>(numDwords)), "SetEnd wrote the wrong dword count");
        check(std::equal(before.begin(), before.begin() + 4, storage.words.begin()), "SetEnd modified other packet words");
        check(std::equal(before.begin() + 5, before.end(), storage.words.begin() + 5), "SetEnd modified following words");
    }

    {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        check(setEnd(storage.packet, at(endAfter(storage.packet, 0x4000u))) == 0, "SetEnd wrap 0x4000 failed");
        check(storage.packet[4] == 0xabcd0000u, "SetEnd did not mask count to 14 bits");
    }
    {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        check(setEnd(storage.packet, storage.packet + 4) == 0, "SetEnd packet+4 failed");
        check(storage.packet[4] == 0xabcd3fffu, "SetEnd did not mask negative delta to 0x3fff");
    }
    {
        Storage storage(variant.write);
        storage.packet[4] |= 0xabcd0000u;
        check(setEnd(storage.packet, at(endAfter(storage.packet, 2) + 2)) == 0, "SetEnd unaligned failed");
        check(storage.packet[4] == 0xabcd0002u, "SetEnd did not shift unaligned byte delta");
    }

    expectInvalidPacket(variant, [=](std::uint32_t* packet) { return setEnd(nullptr, at(endAfter(packet, 2))); });
    expectInvalidPacket(variant, [=](std::uint32_t* packet) { return setEnd(packet + 1, at(endAfter(packet, 2))); });
    {
        Storage storage(variant.write);
        storage.packet[0] = (storage.packet[0] & ~0xff00u) | 0x2100u;
        const auto before = storage.words;
        check(setEnd(storage.packet, at(endAfter(storage.packet, 2))) == InvalidPacket, "SetEnd accepted wrong opcode");
        check(storage.words == before, "SetEnd modified wrong-opcode packet");
    }
}

void testSetCommandAddress(const Variant& variant) {
    const auto setCommandAddress = variant.setCommandAddress;
    for (const std::uintptr_t address : {std::uintptr_t{0x0000123456789abcu}, std::uintptr_t{4u}, std::uintptr_t{0x0000fffffffffffcu}}) {
        Storage storage(variant.write);
        storage.packet[1] |= 3u;
        const auto before = storage.words;
        check(setCommandAddress(storage.packet, at(address)) == 0, "SetCommandAddress failed");
        check(storage.packet[1] == (static_cast<std::uint32_t>(address) | 3u) && storage.packet[2] == static_cast<std::uint32_t>(address >> 32u), "SetCommandAddress wrote the wrong address");
        check(storage.packet[0] == before[0] && storage.packet[3] == before[3] && storage.packet[4] == before[4], "SetCommandAddress modified other packet words");
        check(std::equal(before.begin() + 5, before.end(), storage.words.begin() + 5), "SetCommandAddress modified following words");
    }

    {
        Storage storage(variant.write);
        storage.packet[1] = 0x1234567bu;
        storage.packet[2] = 0xdeadbeefu;
        check(setCommandAddress(storage.packet, nullptr) == 0, "SetCommandAddress null address failed");
        check(storage.packet[1] == 3u && storage.packet[2] == 0u, "SetCommandAddress did not clear null address");
    }
    {
        Storage storage(variant.write);
        storage.packet[1] |= 2u;
        check(setCommandAddress(storage.packet, at(0x1002u)) == 0, "SetCommandAddress unaligned failed");
        check(storage.packet[1] == 0x1002u && storage.packet[2] == 0u, "SetCommandAddress did not mask low two address bits");
    }

    expectInvalidPacket(variant, [=](std::uint32_t*) { return setCommandAddress(nullptr, at(0x1000u)); });
    expectInvalidPacket(variant, [=](std::uint32_t* packet) { return setCommandAddress(packet + 1, at(0x1000u)); });
    {
        Storage storage(variant.write);
        storage.packet[0] = (storage.packet[0] & ~0xff00u) | 0x2100u;
        const auto before = storage.words;
        check(setCommandAddress(storage.packet, at(0x1000u)) == InvalidPacket, "SetCommandAddress accepted wrong opcode");
        check(storage.words == before, "SetCommandAddress modified wrong-opcode packet");
    }
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
