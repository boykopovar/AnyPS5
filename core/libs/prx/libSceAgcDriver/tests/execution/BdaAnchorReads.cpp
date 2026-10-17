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
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t InputDwords = 1024;
constexpr std::uint32_t Fill = 0xcdcdcdcdu;
constexpr std::uint32_t OutputBase = 0x8000;
constexpr std::uint32_t OutputDwords = 8;

alignas(256) constexpr std::array<std::uint32_t, 33> Code{
    0x34040082, 0x34060085, 0x4a0606ff, OutputBase,
    0xdc308010, 0x04000002, 0xdc308014, 0x05000002, 0xdc308018, 0x06000002,
    0xdc308403, 0x07000002,
    0xf4000100, 0xfa000203, 0xf4000140, 0xfa0002f6,
    0xbf8c3f70, 0xbf8cc07f, 0x7e100204, 0x7e120205,
    0xdc708000, 0x00000403, 0xdc708004, 0x00000503, 0xdc708008, 0x00000603, 0xdc70800c, 0x00000703,
    0xdc708010, 0x00000803, 0xdc708014, 0x00000903, 0xbf810000};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "BDA anchor reads: cannot allocate the guest block");
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<std::uint32_t> Initial() {
    std::vector<std::uint32_t> memory(BlockBytes / 4u, Fill);
    for (std::uint32_t dword = 0; dword < InputDwords; ++dword) memory[dword] = 0x61000000u + dword * 0x00010307u;
    return memory;
}

std::uint32_t Bytes(const std::vector<std::uint32_t>& memory, std::uint32_t address) {
    std::uint32_t value = 0;
    for (std::uint32_t byte = 0; byte < 4u; ++byte) {
        const std::uint32_t at = address + byte;
        value |= ((memory[at / 4u] >> (at % 4u * 8u)) & 0xffu) << (byte * 8u);
    }
    return value;
}

std::vector<std::uint32_t> Expected() {
    const auto initial = Initial();
    auto memory = initial;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* out = &memory[OutputBase / 4u + tid * OutputDwords];
        out[0] = Bytes(initial, tid * 4u + 0x10u);
        out[1] = Bytes(initial, tid * 4u + 0x14u);
        out[2] = Bytes(initial, tid * 4u + 0x18u);
        out[3] = Bytes(initial, tid * 4u + 0x403u);
        out[4] = Bytes(initial, 0x200u);
        out[5] = Bytes(initial, 0x2f4u);
    }
    return memory;
}

ShaderRecompiler::RecompileResult Recompile(std::uint32_t waveSize, std::span<const std::uint32_t> userData, std::span<const std::byte> guest, const ShaderRecompiler::SpirvTarget& target) {
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 2> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}, {reinterpret_cast<std::uintptr_t>(guest.data()), guest}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, regions},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

bool SearchesAnchor(const ShaderRecompiler::SharedSpirv& spirv) {
    const std::string_view text(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(std::uint32_t));
    return text.find("bda_anchor_range") != std::string_view::npos;
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::string& name) {
    auto memory = Initial();
    std::memcpy(guest.Data(), memory.data(), BlockBytes);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const auto result = Recompile(waveSize, userData, std::span<const std::byte>(reinterpret_cast<const std::byte*>(guest.Data()), InputDwords * sizeof(std::uint32_t)), target);
    Require(SearchesAnchor(result.spirv), "BDA anchor reads: " + name + " reads of one user-data base do not share an anchor");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    std::memcpy(memory.data(), guest.Data(), BlockBytes);
    const auto expected = Expected();
    for (std::size_t dword = 0; dword < memory.size(); ++dword) {
        Require(memory[dword] == expected[dword], "BDA anchor reads: " + name + " byte " + std::to_string(dword * 4u) + " is " + Hex(memory[dword]) + ", expected " + Hex(expected[dword]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        const auto native = device->Target();
        const auto split = device->ComputeTarget(32);
        Run(*device, guest, 32, native, "wave32");
        Run(*device, guest, 64, native, "wave64");
        if (split.subgroupSize != native.subgroupSize) Run(*device, guest, 64, split, "wave64 split");
        std::puts("BDA anchor read tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
