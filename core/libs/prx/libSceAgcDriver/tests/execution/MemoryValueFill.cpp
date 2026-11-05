#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DwordPatternFill.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::KeysFillMemo;
using AgcDriver::DriverDetail::MatchMemoryValueBufferFill;

alignas(256) constexpr std::array<std::uint32_t, 9> Code{0xd7460000u, 0x04010c08u, 0xf4201a82u, 0xfa000000u, 0xbf8cc07fu, 0x7e02026au, 0xe0102000u, 0x80000100u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Format32Uint = 0x00014004u;
alignas(256) std::array<std::uint32_t, 4> Value{};
alignas(256) std::array<std::uint32_t, 1028> Output{};

void KeysFillMemoTests() {
    using AgcDriver::Graphics::DccKeys;
    const auto bytes = [](std::uint32_t byte) { const auto word = byte * 0x01010101u; return std::array<std::uint32_t, 4>{word, word, word, word}; };
    const std::array<std::pair<std::uint32_t, DccKeys>, 6> codes{{{0x00u, DccKeys::Clear0000}, {0x40u, DccKeys::Clear0001}, {0x80u, DccKeys::Clear1110}, {0xc0u, DccKeys::Clear1111}, {0x20u, DccKeys::ClearRegister}, {0xffu, DccKeys::Uncompressed}}};
    for (const auto& [byte, keys] : codes) {
        Require(KeysFillMemo(bytes(byte), false, true) == keys, "a memory-value fill of a key code over keys that are no storage image's was not recorded");
        Require(KeysFillMemo(bytes(byte), true, false) == keys, "a fill of a key code over a storage image's keys was not recorded");
        Require(KeysFillMemo(bytes(byte), true, true) == keys, "a memory-value fill over a storage image's keys was not recorded");
        Require(!KeysFillMemo(bytes(byte), false, false), "a fill over keys that are no storage image's, other than a memory-value fill, was recorded");
    }
    Require(!KeysFillMemo(bytes(0x12u), true, true), "a byte that is no key code was recorded");
    auto mixed = bytes(0x40u);
    mixed[3] = 0x40404041u;
    Require(!KeysFillMemo(mixed, true, true), "a pattern of two different words was recorded");
    mixed = bytes(0x40u);
    mixed[0] = 0x40404000u;
    Require(!KeysFillMemo(mixed, true, true), "a word of different bytes was recorded");
}

std::vector<std::uint32_t> UserData(const std::uint32_t* output, std::uint32_t records, std::uint32_t valueRecords = 16, std::uint32_t valueStride = 0, std::uint32_t format = Format32Uint) {
    const auto address = reinterpret_cast<std::uintptr_t>(output);
    const auto value = reinterpret_cast<std::uintptr_t>(Value.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), records, format, static_cast<std::uint32_t>(value), static_cast<std::uint32_t>((value >> 32u) & 0xffffu) | (valueStride << 16u), valueRecords, Format32Uint};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const std::array<std::uint32_t, 5> packet{0xc0031500u, 4, 1, 1, 0x41u};
    const auto data = UserData(Output.data(), 1024);
    const auto fill = MatchMemoryValueBufferFill(packet, Code, data, compute);
    Require(fill && fill->address == reinterpret_cast<std::uintptr_t>(Output.data()) && fill->bytes == 1024 && fill->valueAddress == reinterpret_cast<std::uintptr_t>(Value.data()), "the memory-value fill kernel was not taken as a fill of 256 dwords");
    for (const std::uint32_t format : {0x00015004u, 0x00016004u}) {
        Require(MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 16, 0, format), compute).has_value(), "a 32-bit output format was refused");
    }
    for (const std::uint32_t format : {0x0000d004u, 0x00027facu, 0x00038004u}) {
        Require(!MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 16, 0, format), compute), "an output format whose elements are not one 32-bit value matched");
    }
    Require(MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 1, 4), compute).has_value(), "a value descriptor of one 4-byte element was refused");
    Require(MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 4), compute).has_value(), "a value descriptor of 4 bytes was refused");
    Require(!MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 0), compute), "a value descriptor without records matched (the load returns 0)");
    Require(!MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 3), compute), "a value descriptor shorter than the loaded dword matched");
    Require(!MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 1024, 1, 2), compute), "a value descriptor of one 2-byte element matched");
    Require(!MatchMemoryValueBufferFill(packet, Code, UserData(Output.data() + 1, 1024), compute), "an unaligned destination matched");
    Require(!MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 102), compute), "a record count not filling 16-byte units matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchMemoryValueBufferFill(packet, Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto partial = packet;
    partial[4] |= 0x20u;
    Require(!MatchMemoryValueBufferFill(partial, Code, data, compute), "a partial dispatch matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchMemoryValueBufferFill(packet, Code, data, noGroup), "a dispatch without the group id matched");
    auto altered = Code;
    altered[5] ^= 1u;
    Require(!MatchMemoryValueBufferFill(packet, altered, data, compute), "a different shader matched");
    const auto clamped = MatchMemoryValueBufferFill(packet, Code, UserData(Output.data(), 64), compute);
    Require(clamped && clamped->bytes == 256, "the fill exceeded the output descriptor's records");
}

std::vector<std::uint32_t> Commands(const std::vector<std::uint32_t>& data, std::uint32_t groups, std::uint32_t wave) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{64, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (1u << 7u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u | (wave == 32 ? 0x8000u : 0u)});
    return words;
}

void Label(std::vector<std::uint32_t>& words, std::uint32_t* target, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, 0, 0});
}

void Submit(const std::vector<std::uint32_t>& words) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
}

void Expect(std::size_t first, std::size_t count, std::uint32_t value, const std::string& what) {
    for (std::size_t i = 0; i < Output.size(); ++i) {
        const auto expected = i >= first && i < first + count ? value : Untouched;
        Require(Output[i] == expected, what + ": word " + std::to_string(i) + " is " + std::to_string(Output[i]) + ", expected " + std::to_string(expected));
    }
}

void ExecuteTests() {
    struct Case {
        std::uint32_t value, first, records, groups, valueRecords, valueStride, stored;
        const char* what;
    };
    constexpr std::array<Case, 8> cases{{
        {0xffffffffu, 0, 256, 4, 16, 0, 0xffffffffu, "a 16-byte aligned fill of every launched thread"},
        {0u, 0, 256, 2, 16, 0, 0u, "a fill shorter than its records"},
        {0x40404040u, 0, 1000, 4, 16, 0, 0x40404040u, "a fill longer than its launched threads"},
        {0x0c0c0c0cu, 0, 256, 4, 1, 4, 0x0c0c0c0cu, "a fill whose value descriptor holds one 4-byte element"},
        {0x12345678u, 0, 255, 4, 16, 0, 0x12345678u, "a fill whose size is not a multiple of 16 bytes"},
        {0x80808080u, 1, 256, 4, 16, 0, 0x80808080u, "a fill whose start is not 16-byte aligned"},
        {0x20202020u, 0, 0, 4, 16, 0, 0x20202020u, "an empty fill"},
        {0xc0c0c0c0u, 0, 256, 4, 0, 0, 0u, "a fill whose value descriptor has no records (the load returns 0)"},
    }};
    for (const auto wave : {32u, 64u}) {
        for (const auto& item : cases) {
            Output.fill(Untouched);
            Value[0] = item.value;
            Submit(Commands(UserData(Output.data() + item.first, item.records, item.valueRecords, item.valueStride), item.groups, wave));
            Expect(item.first, std::min(item.records, item.groups * 64u), item.stored, std::string(item.what) + " (wave " + std::to_string(wave) + ")");
        }
    }
    Output.fill(Untouched);
    Value[0] = 0x11111111u;
    auto words = Commands(UserData(Output.data(), 256), 4, 64);
    Label(words, &Value[0], 0xc0c0c0c0u);
    const auto again = Commands(UserData(Output.data(), 128), 2, 64);
    words.insert(words.end(), again.begin(), again.end());
    Submit(words);
    for (std::size_t i = 0; i < 256; ++i) {
        Require(Output[i] == (i < 128 ? 0xc0c0c0c0u : 0x11111111u), "a fill did not read the value a preceding GPU label stored, at word " + std::to_string(i));
    }
}

}

int main() {
    try {
        KeysFillMemoTests();
        if (std::getenv("APS5_NO_FILL_HLE") == nullptr) MatchTests();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ExecuteTests();
        AgcDriverShutdown_nid_postfix();
        std::puts("memory-value fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
