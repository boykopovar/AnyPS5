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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::MatchDwordPatternFill;

alignas(256) constexpr std::array<std::uint32_t, 69> Code{
    0xbfa00003u, 0xd7460002u, 0x04010c0au, 0x7da80408u, 0xbf88003fu, 0x7e000c09u, 0xbf070980u, 0x858a807eu,
    0x7e005700u, 0x100000ffu, 0x4f800000u, 0x7e060f00u, 0xd5766a00u, 0x02020609u, 0x7d8a0280u, 0x4c020080u,
    0x02000101u, 0xd56a0001u, 0x00020700u, 0x4c000303u, 0x4a020303u, 0x02000101u, 0xd56a0000u, 0x00020500u,
    0xd5690001u, 0x00020009u, 0x4c060302u, 0x7d860609u, 0x7d8c02f9u, 0x06068c02u, 0x87ea6a0cu, 0x50000080u,
    0xd5286a00u, 0x003200c1u, 0xd5010000u, 0x002a00c1u, 0xd5690000u, 0x00020009u, 0x4c000102u, 0x7d0a0080u,
    0xbe88246au, 0xbf880015u, 0x7d0a0081u, 0xbe8a246au, 0xbf88000cu, 0x7d0a0082u, 0xbeea246au, 0xbf880003u,
    0x7e000207u, 0xe0102000u, 0x80000002u, 0x8afe7e6au, 0xbf880003u, 0x7e000206u, 0xe0102000u, 0x80000002u,
    0xbefe046au, 0x8afe7e0au, 0xbf880003u, 0x7e000205u, 0xe0102000u, 0x80000002u, 0xbefe040au, 0x8afe7e08u,
    0xbf880003u, 0x7e000204u, 0xe0102000u, 0x80000002u, 0xbf810000u};
constexpr std::array<std::uint32_t, 4> Pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
alignas(256) std::array<std::uint32_t, 1028> Output{};

std::vector<std::uint32_t> UserData(void* output, std::uint32_t records, std::uint32_t count, std::uint32_t period, std::uint32_t format = 0x00014004u) {
    const auto address = reinterpret_cast<std::uintptr_t>(output);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), records, format, Pattern[0], Pattern[1], Pattern[2], Pattern[3], count, period};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const std::array<std::uint32_t, 5> packet{0xc0031500u, 4, 1, 1, 0x41u};
    const auto data = UserData(Output.data(), 1024, 256, 4);
    const auto fill = MatchDwordPatternFill(packet, Code, data, compute);
    Require(fill && fill->base == reinterpret_cast<std::uintptr_t>(Output.data()) && fill->bytes == 1024 && fill->pattern == Pattern, "the dword pattern-fill kernel was not recognized as a 16-byte pattern fill");
    for (const auto period : {1u, 2u}) {
        const auto narrow = MatchDwordPatternFill(packet, Code, UserData(Output.data(), 1024, 256, period), compute);
        Require(narrow && narrow->pattern[1] == Pattern[1 % period] && narrow->pattern[3] == Pattern[3 % period], "a shorter pattern period was not repeated over 16 bytes");
    }
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 1u;
        Require(!MatchDwordPatternFill(packet, altered, data, compute), "a different shader matched the dword pattern-fill kernel");
    }
    Require(!MatchDwordPatternFill(packet, std::span(Code).first(68), data, compute), "a truncated shader matched");
    Require(!MatchDwordPatternFill(packet, Code, UserData(Output.data(), 1024, 256, 3), compute), "a three-dword period matched a 16-byte pattern");
    Require(!MatchDwordPatternFill(packet, Code, UserData(Output.data(), 1024, 250, 4), compute), "a count of whole dwords not filling 16-byte units matched");
    Require(!MatchDwordPatternFill(packet, Code, UserData(Output.data() + 1, 1024, 256, 4), compute), "an unaligned destination matched");
    Require(!MatchDwordPatternFill(packet, Code, UserData(Output.data(), 1024, 256, 4, 0x00814004u), compute), "an add-tid descriptor matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchDwordPatternFill(packet, Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto partial = packet;
    partial[4] |= 0x20u;
    Require(!MatchDwordPatternFill(partial, Code, data, compute), "a partial dispatch matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchDwordPatternFill(packet, Code, data, noGroup), "a dispatch without the group id matched");
    const auto clamped = MatchDwordPatternFill(packet, Code, UserData(Output.data(), 64, 256, 4), compute);
    Require(clamped && clamped->bytes == 256, "the fill exceeded the descriptor's records");
}

std::vector<std::uint32_t> Commands(const std::vector<std::uint32_t>& data, std::uint32_t groups, bool partial, std::uint32_t threads) {
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
    words.insert(words.end(), {0xc0031500u, partial ? threads : groups, 1, 1, 0x41u | (partial ? 0x20u : 0u)});
    return words;
}

void ExecuteTests() {
    struct Case {
        std::uint32_t groups, records, count, period;
        bool partial;
        std::uint32_t threads;
    };
    constexpr std::array<Case, 10> cases{{
        {4, 1024, 256, 4, false, 0},
        {4, 1024, 256, 2, false, 0},
        {4, 1024, 256, 1, false, 0},
        {16, 1024, 1024, 4, false, 0},
        {4, 1024, 100, 4, false, 0},
        {4, 1024, 250, 4, false, 0},
        {4, 64, 256, 4, false, 0},
        {4, 1024, 256, 3, false, 0},
        {4, 1024, 256, 5, false, 0},
        {0, 1024, 256, 4, true, 70},
    }};
    for (const auto& item : cases) {
        Output.fill(Untouched);
        const auto words = Commands(UserData(Output.data(), item.records, item.count, item.period), item.groups, item.partial, item.threads);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
        const auto launched = item.partial ? item.threads : item.groups * 64u;
        const auto filled = std::min({launched, item.records, item.count});
        for (std::uint32_t i = 0; i < Output.size(); ++i) {
            const auto expected = i < filled ? Pattern[std::min(i % item.period, 3u)] : Untouched;
            Require(Output[i] == expected, "dword pattern fill mismatch at " + std::to_string(i) + " (period " + std::to_string(item.period) + ", count " + std::to_string(item.count) + ", records " + std::to_string(item.records) + "): got " + std::to_string(Output[i]) + ", expected " + std::to_string(expected));
        }
    }
}

}

int main() {
    try {
        if (std::getenv("APS5_NO_FILL_HLE") == nullptr) MatchTests();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ExecuteTests();
        AgcDriverShutdown_nid_postfix();
        std::puts("dword pattern fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
