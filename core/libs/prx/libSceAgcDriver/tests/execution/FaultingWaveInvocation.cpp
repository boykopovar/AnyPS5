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
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t FaultingLane = 5;
constexpr std::uint32_t PassLimit = 100;
constexpr std::uint32_t CountOffset = 0x200;
constexpr std::uint32_t ValueOffset = 0x300;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 22> Code{
    0x34020082u, 0x7d8a0085u, 0x020202ffu, 0x00010000u, 0xdc308000u, 0x02000001u, 0xbf8c3f70u, 0xbe8a0380u,
    0x7da2040au, 0xbf880004u, 0x800a810au, 0xbf0aff0au, PassLimit, 0xbf85fffau, 0xbefe04c1u, 0x7e06020au,
    0x34080082u, 0xdc708200u, 0x00000304u, 0xdc708300u, 0x00000204u, 0xbf810000u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "faulting wave invocation: cannot allocate the guest block");
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

    std::uint32_t& At(std::uint32_t offset) { return block[offset / 4u]; }
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }

private:
    std::uint32_t* block = nullptr;
};

std::uint32_t Passes(std::uint32_t lane) {
    return lane % 4u + 1u;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            guest.At(lane * 4u) = Passes(lane);
            guest.At(CountOffset + lane * 4u) = Untouched;
            guest.At(ValueOffset + lane * 4u) = Untouched;
        }
        const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(guest.Address()), static_cast<std::uint32_t>(guest.Address() >> 32u)};
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        std::uint32_t passes = 0;
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            if (lane != FaultingLane) passes = std::max(passes, Passes(lane));
        }
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto what = "faulting wave invocation: lane " + std::to_string(lane);
            const auto count = guest.At(CountOffset + lane * 4u);
            const auto value = guest.At(ValueOffset + lane * 4u);
            if (lane == FaultingLane) {
                Require(count == Untouched && value == Untouched, what + " stored after its read faulted");
                continue;
            }
            Require(value == Passes(lane), what + " stored " + std::to_string(value) + ", expected its loaded " + std::to_string(Passes(lane)));
            Require(count == passes, what + " left the EXEC loop after " + std::to_string(count) + " passes, expected " + std::to_string(passes) + " (the loop stops at " + std::to_string(PassLimit) + ")");
        }
        std::puts("faulting wave invocation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
