#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DwordPatternFill.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::MatchClampedGlobalFill;

alignas(256) constexpr std::array<std::uint32_t, 11> Code{0xd7460000u, 0x04010c06u, 0x7e080205u, 0x7e0a0205u, 0x7e0c0205u, 0x7e0e0205u, 0x34000084u, 0x26000004u, 0xdc788000u, 0x00020400u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t OutputDwords = 1028;
alignas(256) std::array<std::uint32_t, OutputDwords> Output{};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "clamped global fill: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::span<std::uint32_t> Words() { return {block, OutputDwords}; }

private:
    std::uint32_t* block = nullptr;
};

std::vector<std::uint32_t> UserData(void* output, std::uint32_t maxOffset, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(output);
    return {0u, 0u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), maxOffset, value};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const std::array<std::uint32_t, 5> packet{0xc0031500u, 1, 1, 1, 0x41u};
    const auto data = UserData(Output.data(), 1008, 0x5a5a5a5au);
    const auto fill = MatchClampedGlobalFill(packet, Code, data, compute);
    Require(fill && fill->base == reinterpret_cast<std::uintptr_t>(Output.data()) && fill->bytes == 1024 && fill->pattern == std::array<std::uint32_t, 4>{0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au}, "the clamped global fill kernel was not recognized as a fill of its value");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 0x10000u;
        Require(!MatchClampedGlobalFill(packet, altered, data, compute), "a different shader matched the clamped global fill kernel");
    }
    Require(!MatchClampedGlobalFill(packet, std::span(Code).first(10), data, compute), "a truncated shader matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchClampedGlobalFill(packet, Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto threads = packet;
    threads[4] |= 0x20u;
    Require(!MatchClampedGlobalFill(threads, Code, data, compute), "a dispatch in thread dimensions matched");
    auto rows = packet;
    rows[2] = 2;
    Require(!MatchClampedGlobalFill(rows, Code, data, compute), "a two-dimensional dispatch matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchClampedGlobalFill(packet, Code, data, noGroup), "a dispatch without the group id matched");
    Require(!MatchClampedGlobalFill(packet, Code, UserData(Output.data() + 1, 1008, 0), compute), "an unaligned destination matched");
    Require(!MatchClampedGlobalFill(packet, Code, UserData(Output.data(), 1000, 0), compute), "a largest offset off the 16-byte grid matched");
    const auto clamped = MatchClampedGlobalFill(packet, Code, UserData(Output.data(), 240, 0), compute);
    Require(clamped && clamped->bytes == 256, "the fill reached past its largest store offset");
    std::array<std::uint32_t, 5> four = packet;
    four[1] = 4;
    const auto launched = MatchClampedGlobalFill(four, Code, UserData(Output.data(), 0x100000u, 0), compute);
    Require(launched && launched->bytes == 4096, "the fill reached past its last thread's store");
}

std::vector<std::uint32_t> Commands(const std::vector<std::uint32_t>& data, std::uint32_t groups) {
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
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
    return words;
}

void ExecuteTests() {
    struct Case {
        std::uint32_t groups, maxOffset, value;
    };
    constexpr std::array<Case, 5> cases{{
        {1, 1008, 0u},
        {4, 1008, 0x5a5a5a5au},
        {4, 240, 0xffffffffu},
        {1, 0x100000u, 0x01020304u},
        {0, 1008, 0x77777777u},
    }};
    GuestBlock block;
    const auto output = block.Words();
    for (const auto& item : cases) {
        std::fill(output.begin(), output.end(), Untouched);
        const auto words = Commands(UserData(output.data(), item.maxOffset, item.value), item.groups);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(output.data()), output.size_bytes());
        const auto filledDwords = std::min(item.groups * 64u * 4u, item.maxOffset / 4u + 4u);
        for (std::uint32_t i = 0; i < output.size(); ++i) {
            const auto expected = i < filledDwords ? item.value : Untouched;
            Require(output[i] == expected, "clamped global fill mismatch at " + std::to_string(i) + " (groups " + std::to_string(item.groups) + ", largest offset " + std::to_string(item.maxOffset) + "): got " + std::to_string(output[i]) + ", expected " + std::to_string(expected));
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
        std::puts("clamped global fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
