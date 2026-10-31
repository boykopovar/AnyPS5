#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t RecordBytes = 128;
constexpr std::uint32_t TokenWord = 4;
constexpr std::uint32_t ReportWord = 8;
constexpr std::uint32_t Tokens = 128;
constexpr std::uint32_t SpinBudget = 1000000;
constexpr std::uint32_t Rounds = 4;

alignas(256) constexpr std::array<std::uint32_t, 63> CrossWorkgroupBufferFlagCode{
    0x7da40080, 0xbe850304, 0xbe860380, 0xbe870380, 0xbf09ff05, 0x00000080, 0xbf850030, 0xe030c010,
    0x80000100, 0xbf8c3f70, 0x7d840205, 0xbf870006, 0x80068106, 0xbf0aff06, 0x000f4240, 0xbf85fff7,
    0x7e040283, 0xbf820026, 0xbf068005, 0xbf850014, 0xe038c000, 0x80000800, 0x7e180205, 0x4a1a18ff,
    0x00010000, 0x4a1c18ff, 0x00020000, 0x4a1e18ff, 0x00030000, 0xbf8c3f70, 0x3a101908, 0x3a121b09,
    0x3a141d0a, 0x3a161f0b, 0xd7720008, 0x042a1308, 0x38101708, 0x7d8a1080, 0xbf860001, 0x80078107,
    0x80088105, 0x7e080208, 0x4a0a08ff, 0x00010000, 0x4a0c08ff, 0x00020000, 0x4a0e08ff, 0x00030000,
    0xe0784000, 0x80000400, 0xbbfd0000, 0xe0704010, 0x80000400, 0x80058205, 0xbf82ffcd, 0x7e040282,
    0x7e060206, 0x7e080207, 0x7e0a0205, 0x8f098404, 0xe0780020, 0x09000200, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "cross-workgroup buffer flag: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }
    ~GuestBlock() { GuestAllocations::Mutation().Remove(block); }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Words() { return reinterpret_cast<std::uint32_t*>(block); }

private:
    std::uint8_t* block = nullptr;
};

bool Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize) {
    auto* words = guest.Words();
    std::memset(words, 0, RecordBytes);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(words));
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, RecordBytes, 0x31016facu};
    const std::span<const std::uint32_t> code(CrossWorkgroupBufferFlagCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(std::ranges::any_of(result.bindings, [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers; }), "cross-workgroup buffer flag: the V# was not bound as a guest buffer descriptor");
    device.Dispatch(result, 2, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    bool passed = words[TokenWord] == Tokens;
    for (std::uint32_t index = 0; index < 4u; ++index) passed = passed && words[index] == Tokens + (index << 16u);
    for (std::uint32_t group = 0; group < 2u; ++group) {
        const auto* report = words + ReportWord + group * 4u;
        passed = passed && report[0] == 2u && report[2] == 0u && report[3] == Tokens + group;
        std::printf("wave%u group %u: %s, %u spin iterations, %u stale payloads, next token %u\n", waveSize, group, report[0] == 2u ? "took every turn" : report[0] == 3u ? "gave up" : "?", report[1], report[2], report[3]);
    }
    std::printf("wave%u: token %u of %u, payload %08x %08x %08x %08x\n", waveSize, words[TokenWord], Tokens, words[0], words[1], words[2], words[3]);
    return passed;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        std::uint32_t failures = 0;
        std::uint32_t runs = 0;
        for (std::uint32_t round = 0; round < Rounds; ++round) {
            for (const std::uint32_t waveSize : {64u, 32u}) {
                ++runs;
                if (!Run(*device, guest, waveSize)) ++failures;
            }
        }
        std::printf("%u of %u runs passed the token %u times within %u spin iterations per workgroup\n", runs - failures, runs, Tokens, SpinBudget);
        Require(failures == 0u, "two workgroups handing a token back and forth with buffer_load_dword glc dlc through a V# did not see each other's stores, or read a stale payload after the token");
        std::puts("cross-workgroup buffer flag tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
