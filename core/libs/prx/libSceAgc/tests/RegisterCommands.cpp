#include "prx/libSceAgc/Patch/include/IndirectRegisters.hpp"
#include "prx/libSceAgc/DcbState/include/Registers.hpp"
#include "prx/libSceAgc/Cb/include/Registers.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

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

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

void testDirect() {
    const std::array writers{sceAgcDcbSetCxRegisterDirect, sceAgcDcbSetShRegisterDirect, sceAgcDcbSetUcRegisterDirect};
    const std::array sizes{sceAgcDcbSetCxRegisterDirectGetSize, sceAgcDcbSetShRegisterDirectGetSize, sceAgcDcbSetUcRegisterDirectGetSize};
    const std::array headers{0xc0016900u, 0xc0017600u, 0xc0017900u};
    for (std::size_t i = 0; i < writers.size(); ++i) {
        Storage storage;
        storage.words.fill(0xabcdef01u);
        auto* packet = writers[i](&storage.buffer, {0xffffu, 0x12345678u});
        const std::array expected{headers[i], 0xffffu, 0x12345678u};
        check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect direct register packet");
        check(storage.buffer.cursor_up == packet + 3 && sizes[i]() == 12, "direct size/cursor mismatch");
        check(packet[3] == 0xabcdef01u, "direct command overwrote following word");
        const auto before = storage.words;
        expectFailure([&] { writers[i](&storage.buffer, {0x10000u, 1}); });
        expectFailure([&] { writers[i](nullptr, {0, 1}); });
        storage.buffer.cursor_down = storage.buffer.cursor_up + 2;
        expectFailure([&] { writers[i](&storage.buffer, {0, 1}); });
        check(storage.words == before && storage.buffer.cursor_up == packet + 3, "invalid direct write modified buffer");
    }
}

void testConfig() {
    Storage storage;
    storage.words.fill(0xabcdef01u);
    auto* direct = sceAgcDcbSetCfRegisterDirect(&storage.buffer, {0x2468u, 0x12345678u});
    const std::array expectedDirect{0xc0016800u, 0x2468u, 0x12345678u};
    check(direct == storage.words.data() && std::equal(expectedDirect.begin(), expectedDirect.end(), direct), "incorrect config register packet");
    const std::array<std::uint32_t, 3> values{1, 2, 3};
    auto* range = sceAgcDcbSetCfRegisterRangeDirect(&storage.buffer, 0x100u, values.data(), values.size());
    const std::array expectedRange{0xc0036800u, 0x100u, 1u, 2u, 3u};
    check(range == direct + 3 && std::equal(expectedRange.begin(), expectedRange.end(), range), "incorrect config register range packet");
    auto* reserved = sceAgcDcbSetCfRegisterRangeDirect(&storage.buffer, 0x200u, nullptr, 2);
    check(reserved == range + 5 && reserved[0] == 0xc0026800u && reserved[1] == 0x200u && reserved[2] == 0xabcdef01u && reserved[3] == 0xabcdef01u, "null config values wrote the payload");
    check(storage.buffer.cursor_up == reserved + 4 && reserved[4] == 0xabcdef01u, "config register cursor mismatch");
    const auto before = storage.words;
    expectFailure([&] { sceAgcDcbSetCfRegisterDirect(&storage.buffer, {0x10000u, 1}); });
    expectFailure([&] { sceAgcDcbSetCfRegisterDirect(nullptr, {0, 1}); });
    expectFailure([&] { sceAgcDcbSetCfRegisterRangeDirect(&storage.buffer, 0x100u, values.data(), 0); });
    expectFailure([&] { sceAgcDcbSetCfRegisterRangeDirect(&storage.buffer, 0xffffu, values.data(), 2); });
    check(storage.words == before && storage.buffer.cursor_up == reserved + 4, "invalid config register write modified buffer");
}

void testRangeFromAnyByte() {
    const std::array writers{sceAgcCbSetShRegisterRangeDirect, sceAgcCbSetUcRegisterRangeDirect, sceAgcDcbSetCfRegisterRangeDirect};
    const std::array headers{0xc0047600u, 0xc0047900u, 0xc0046800u};
    alignas(16) std::array<unsigned char, 32> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<unsigned char>(0x10u + i);
    struct Case {
        std::size_t offset;
        std::array<std::uint32_t, 4> values;
    };
    const std::array cases{
        Case{1, {0x14131211u, 0x18171615u, 0x1c1b1a19u, 0x201f1e1du}},
        Case{2, {0x15141312u, 0x19181716u, 0x1d1c1b1au, 0x21201f1eu}},
        Case{3, {0x16151413u, 0x1a191817u, 0x1e1d1c1bu, 0x2221201fu}},
        Case{13, {0x201f1e1du, 0x24232221u, 0x28272625u, 0x2c2b2a29u}},
    };
    for (std::size_t i = 0; i < writers.size(); ++i) {
        for (const auto& entry : cases) {
            Storage storage;
            storage.words.fill(0xabcdef01u);
            const auto* values = reinterpret_cast<const std::uint32_t*>(bytes.data() + entry.offset);
            auto* packet = writers[i](&storage.buffer, 0x40u, values, 4);
            check(packet == storage.words.data() && storage.buffer.cursor_up == packet + 6, "unaligned register range allocation mismatch");
            check(packet[0] == headers[i] && packet[1] == 0x40u && std::equal(entry.values.begin(), entry.values.end(), packet + 2), "unaligned register values were not read byte for byte");
            check(packet[6] == 0xabcdef01u, "unaligned register range overwrote the following word");
        }
    }
}

void testListFromAnyByte() {
    const std::array writers{sceAgcCbSetShRegistersDirect, sceAgcCbSetUcRegistersDirect};
    const std::array opcodes{0x7600u, 0x7900u};
    const std::array<ShaderRegister, 3> registers{{{0x10u, 0x11223344u}, {0x11u, 0x55667788u}, {0x30u, 0x99aabbccu}}};
    for (std::size_t i = 0; i < writers.size(); ++i) {
        for (const std::size_t offset : {1u, 2u, 3u, 5u}) {
            alignas(16) std::array<unsigned char, 48> bytes{};
            std::memcpy(bytes.data() + offset, registers.data(), sizeof(registers));
            Storage storage;
            storage.words.fill(0xabcdef01u);
            const auto* list = reinterpret_cast<const volatile ShaderRegister*>(bytes.data() + offset);
            auto* packet = writers[i](&storage.buffer, list, static_cast<std::uint32_t>(registers.size()));
            const std::array<std::uint32_t, 7> expected{0xc0020000u | opcodes[i], 0x10u, 0x11223344u, 0x55667788u, 0xc0010000u | opcodes[i], 0x30u, 0x99aabbccu};
            check(packet == storage.words.data() && storage.buffer.cursor_up == packet + expected.size(), "unaligned register list allocation mismatch");
            check(std::equal(expected.begin(), expected.end(), packet), "unaligned register list was not read byte for byte");
            check(packet[expected.size()] == 0xabcdef01u, "unaligned register list overwrote the following word");
        }
    }
}

void testIndirect() {
    const std::array writers{sceAgcDcbSetCxRegistersIndirect, sceAgcDcbSetShRegistersIndirect, sceAgcDcbSetUcRegistersIndirect};
    const std::array sizes{sceAgcDcbSetCxRegistersIndirectGetSize, sceAgcDcbSetShRegistersIndirectGetSize, sceAgcDcbSetUcRegistersIndirectGetSize};
    const std::array setters{sceAgcSetCxRegIndirectPatchSetNumRegisters, sceAgcSetShRegIndirectPatchSetNumRegisters, sceAgcSetUcRegIndirectPatchSetNumRegisters};
    const std::array headers{0xc0039f00u, 0xc0036300u, 0xc0036400u};
    ShaderRegister reg{0x10u, 7};
    for (std::size_t i = 0; i < writers.size(); ++i) {
        Storage storage;
        auto* packet = writers[i](&storage.buffer, &reg, 1);
        const auto original = storage.words;
        check(packet[0] == headers[i] && storage.buffer.cursor_up == packet + 5, "incorrect indirect packet");
        for (const auto count : {0u, 0x3fffu, 2u}) {
            check(sizes[i](count) == 20, "indirect size mismatch");
            check(setters[i](packet, count) == 0 && packet[4] == count, "indirect count was not replaced");
            auto expected = original;
            expected[4] = count;
            check(storage.words == expected, "count setter modified unrelated packet data");
        }
        const auto before = storage.words;
        for (const auto count : {0x4000u, 0xffffffffu, 0xf5f5039cu}) {
            check(sizes[i](count) == 20, "indirect size depends on the count");
            expectFailure([&] { setters[i](packet, count); });
        }
        expectFailure([&] { setters[i](nullptr, 0); });
        expectFailure([&] { setters[(i + 1) % setters.size()](packet, 0); });
        check(storage.words == before, "invalid count setter modified packet");
        for (const auto field : {0u, 3u, 4u}) {
            storage.words = before;
            packet[field] = field == 0 ? headers[i] ^ 0x10000u : 0xffffffffu;
            const auto malformed = storage.words;
            expectFailure([&] { setters[i](packet, 1); });
            check(storage.words == malformed, "setter modified malformed packet");
        }
    }
}

void testIndirectPlaceholder() {
    const std::array writers{sceAgcDcbSetCxRegistersIndirect, sceAgcDcbSetShRegistersIndirect, sceAgcDcbSetUcRegistersIndirect};
    const std::array addressSetters{sceAgcSetCxRegIndirectPatchSetAddress, sceAgcSetShRegIndirectPatchSetAddress, sceAgcSetUcRegIndirectPatchSetAddress};
    const std::array countSetters{sceAgcSetCxRegIndirectPatchSetNumRegisters, sceAgcSetShRegIndirectPatchSetNumRegisters, sceAgcSetUcRegIndirectPatchSetNumRegisters};
    const std::array headers{0xc0039f00u, 0xc0036300u, 0xc0036400u};
    const std::array<ShaderRegister, 2> registers{{{0x10u, 7}, {0x11u, 8}}};
    for (std::size_t i = 0; i < writers.size(); ++i) {
        Storage storage;
        auto* packet = writers[i](&storage.buffer, nullptr, 0);
        check(packet[0] == headers[i] && packet[1] == 0 && packet[2] == 0 && packet[3] == 0x80000000u && packet[4] == 0, "placeholder packet mismatch");
        check(storage.buffer.cursor_up == packet + 5, "placeholder cursor mismatch");
        const auto before = storage.words;
        expectFailure([&] { writers[i](&storage.buffer, nullptr, 1); });
        check(storage.words == before && storage.buffer.cursor_up == packet + 5, "null register list with a count modified the buffer");
        check(addressSetters[i](packet, registers.data()) == 0 && countSetters[i](packet, 2) == 0, "placeholder was not patched");
        const auto address = reinterpret_cast<std::uintptr_t>(registers.data());
        check(packet[1] == static_cast<std::uint32_t>(address) && packet[2] == static_cast<std::uint32_t>(address >> 32u) && packet[4] == 2, "patched placeholder mismatch");
    }
}

void testDirectList() {
    constexpr std::uint32_t sentinel = 0xabcdef01u;
    constexpr std::uint32_t wordSize = sizeof(std::uint32_t);
    const std::array writers{sceAgcCbSetShRegistersDirect, sceAgcCbSetUcRegistersDirect};
    const std::array sizes{sceAgcCbSetShRegistersDirectGetSize, sceAgcCbSetUcRegistersDirectGetSize};
    const std::array opcodes{0x7600u, 0x7900u};
    const std::array<ShaderRegister, 4> scattered{{{0x12u, 1}, {0x11u, 2}, {0x20u, 3}, {0xffffu, 4}}};
    const std::array<ShaderRegister, 4> adjacent{{{0x10u, 5}, {0x11u, 6}, {0x12u, 7}, {0x13u, 8}}};
    const std::array<ShaderRegister, 4> paired{{{0x10u, 5}, {0x11u, 6}, {0x20u, 7}, {0x21u, 8}}};
    for (std::size_t i = 0; i < writers.size(); ++i) {
        const auto single = 0xc0010000u | opcodes[i];
        for (std::uint32_t count = 1; count <= scattered.size(); ++count) {
            const auto words = sizes[i](count) / wordSize;
            Storage storage;
            check(sizes[i](count) % wordSize == 0 && words != 0 && words <= storage.words.size(), "register list size is not a usable word count");
            storage.words.fill(sentinel);
            storage.buffer.cursor_down = storage.buffer.cursor_up + words;
            auto* packet = writers[i](&storage.buffer, scattered.data(), count);
            check(packet == storage.words.data() && storage.buffer.cursor_up == storage.buffer.cursor_down, "separate registers do not fill the queried size");
            for (std::uint32_t reg = 0; reg < count; ++reg) {
                const std::array expected{single, scattered[reg].offset, scattered[reg].value};
                check(std::equal(expected.begin(), expected.end(), packet + reg * expected.size()), "incorrect register list packet");
            }
            check(std::all_of(storage.words.begin() + words, storage.words.end(), [](std::uint32_t word) { return word == sentinel; }), "register list overwrote following words");
            Storage shortStorage;
            shortStorage.buffer.cursor_down = shortStorage.buffer.cursor_up + words - 1u;
            expectFailure([&] { writers[i](&shortStorage.buffer, scattered.data(), count); });
        }
        const auto limit = sizes[i](4) / wordSize;
        Storage merged;
        merged.buffer.cursor_down = merged.buffer.cursor_up + limit;
        const std::array<std::uint32_t, 6> expectedMerged{0xc0040000u | opcodes[i], 0x10u, 5, 6, 7, 8};
        auto* packet = writers[i](&merged.buffer, adjacent.data(), 4);
        check(std::equal(expectedMerged.begin(), expectedMerged.end(), packet) && merged.buffer.cursor_up == packet + expectedMerged.size(), "adjacent registers were not merged into one range");
        Storage split;
        split.buffer.cursor_down = split.buffer.cursor_up + limit;
        const std::array<std::uint32_t, 8> expectedSplit{0xc0020000u | opcodes[i], 0x10u, 5, 6, 0xc0020000u | opcodes[i], 0x20u, 7, 8};
        packet = writers[i](&split.buffer, paired.data(), 4);
        check(std::equal(expectedSplit.begin(), expectedSplit.end(), packet) && split.buffer.cursor_up == packet + expectedSplit.size(), "register pairs were not written as two ranges");
        check(sizes[i](0) == 0, "an empty register list has a size");
        Storage empty;
        empty.words.fill(sentinel);
        check(writers[i](&empty.buffer, nullptr, 0) == empty.words.data() && empty.buffer.cursor_up == empty.words.data() && empty.words.front() == sentinel, "an empty register list wrote a packet");
        expectFailure([&] { writers[i](nullptr, nullptr, 0); });
        check(sizes[i](0x15555555u) == 0xfffffffcu, "largest register list size mismatch");
        for (const auto count : {0x15555556u, 0x40000000u, 0xffffffffu}) {
            expectFailure([&] { sizes[i](count); });
        }
    }
}

}

int main() {
    try {
        testDirect();
        testConfig();
        testRangeFromAnyByte();
        testListFromAnyByte();
        testIndirect();
        testIndirectPlaceholder();
        testDirectList();
        std::puts("AGC register command tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
