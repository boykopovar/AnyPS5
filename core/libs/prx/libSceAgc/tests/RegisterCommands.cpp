#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbSetCxRegisterDirect(CommandBuffer*, ShaderRegister);
std::uint32_t* APS5_VABI sceAgcDcbSetShRegisterDirect(CommandBuffer*, ShaderRegister);
std::uint32_t* APS5_VABI sceAgcDcbSetUcRegisterDirect(CommandBuffer*, ShaderRegister);
std::uint32_t APS5_VABI sceAgcDcbSetCxRegisterDirectGetSize();
std::uint32_t APS5_VABI sceAgcDcbSetShRegisterDirectGetSize();
std::uint32_t APS5_VABI sceAgcDcbSetUcRegisterDirectGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetCxRegistersIndirect(CommandBuffer*, const volatile ShaderRegister*, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcDcbSetShRegistersIndirect(CommandBuffer*, const volatile ShaderRegister*, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcDcbSetUcRegistersIndirect(CommandBuffer*, const volatile ShaderRegister*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcDcbSetCxRegistersIndirectGetSize(std::uint32_t);
std::uint32_t APS5_VABI sceAgcDcbSetShRegistersIndirectGetSize(std::uint32_t);
std::uint32_t APS5_VABI sceAgcDcbSetUcRegistersIndirectGetSize(std::uint32_t);
int APS5_VABI sceAgcSetCxRegIndirectPatchSetNumRegisters(std::uint32_t*, std::uint32_t);
int APS5_VABI sceAgcSetShRegIndirectPatchSetNumRegisters(std::uint32_t*, std::uint32_t);
int APS5_VABI sceAgcSetUcRegIndirectPatchSetNumRegisters(std::uint32_t*, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcCbSetShRegistersDirect(CommandBuffer*, const volatile ShaderRegister*, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcCbSetUcRegistersDirect(CommandBuffer*, const volatile ShaderRegister*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcCbSetShRegistersDirectGetSize(std::uint32_t);
std::uint32_t APS5_VABI sceAgcCbSetUcRegistersDirectGetSize(std::uint32_t);
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
        for (const auto count : {0x4000u, 0xffffffffu}) {
            expectFailure([&] { sizes[i](count); });
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
        testIndirect();
        testDirectList();
        std::puts("AGC register command tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
