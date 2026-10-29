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
#else
#include <sys/mman.h>
#endif
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

constexpr std::uint32_t Lanes = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t OutputOffset = 4096;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Never = 0xffffffffu;

alignas(256) constexpr std::array<std::uint32_t, 37> BranchesCode{
    0x7d840080u, 0x7e040200u, 0x7e060201u, 0xd5010002u, 0x01a80502u, 0xd5010003u, 0x01a80703u, 0xdc308000u,
    0x017d0002u, 0xbf8c3f70u, 0xbe880380u, 0xbe8a047eu, 0x7da40006u, 0xbf880001u, 0x88088108u, 0xbefe040au,
    0x7d840006u, 0xbf860001u, 0x88088208u, 0xbf068006u, 0xbf840001u, 0x88088408u, 0x7daa0006u, 0xbf890001u,
    0x88088808u, 0xbefe040au, 0x7d8a0006u, 0xbf870001u, 0x88089008u, 0xbf078006u, 0xbf850001u, 0x8808a008u,
    0x34080082u, 0x4a0a0208u, 0xdc708000u, 0x00040504u, 0xbf810000u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "paired wave zero branches: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

class Reserved {
public:
    Reserved() {
#ifdef _WIN32
        range = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
        range = mmap(nullptr, BlockBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (range == MAP_FAILED) range = nullptr;
#endif
        Require(range != nullptr, "paired wave zero branches: cannot reserve an inaccessible range");
    }
    ~Reserved() {
#ifdef _WIN32
        VirtualFree(range, 0, MEM_RELEASE);
#else
        munmap(range, BlockBytes);
#endif
    }
    Reserved(const Reserved&) = delete;
    Reserved& operator=(const Reserved&) = delete;
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(range)); }

private:
    void* range = nullptr;
};

std::array<std::uint32_t, Lanes> Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, const Reserved& unmapped) {
    std::memset(guest.Data(), 0, BlockBytes);
    auto* output = guest.Data() + OutputOffset;
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) std::memcpy(output + lane * 4u, &Sentinel, 4u);
    const auto data = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const auto out = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output));
    const std::vector<std::uint32_t> userData{
        static_cast<std::uint32_t>(data), static_cast<std::uint32_t>(data >> 32u),
        static_cast<std::uint32_t>(unmapped.Address()), static_cast<std::uint32_t>(unmapped.Address() >> 32u),
        static_cast<std::uint32_t>(out), static_cast<std::uint32_t>(out >> 32u),
        Never,
    };
    const std::span<const std::uint32_t> code(BranchesCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.target.spannedWaves = false;
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::array<std::uint32_t, Lanes> words{};
    std::memcpy(words.data(), output, sizeof(words));
    return words;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        Reserved unmapped;
        const auto words = Run(*device, guest, unmapped);
        Require(words[0] == Sentinel, "paired wave zero branches: lane 0 kept running after its faulting read (wrote " + std::to_string(words[0]) + ")");
        const std::array<const char*, 6> branches{"s_cbranch_execz", "s_cbranch_vccz", "s_cbranch_scc0", "s_cbranch_execnz", "s_cbranch_vccnz", "s_cbranch_scc1"};
        for (std::uint32_t lane = 1; lane < Lanes; ++lane) {
            if (words[lane] == Sentinel && lane == Lanes / 2u) continue;
            for (std::uint32_t bit = 0; bit < branches.size(); ++bit) {
                Require((words[lane] & (1u << bit)) == 0u, std::string("paired wave zero branches: lane ") + std::to_string(lane) + " fell through " + branches[bit] + " after another invocation of its wave stopped (wrote " + std::to_string(words[lane]) + ")");
            }
            Require(words[lane] == 0u, "paired wave zero branches: lane " + std::to_string(lane) + " wrote " + std::to_string(words[lane]) + ", expected 0");
        }
        std::puts("paired wave zero branch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
