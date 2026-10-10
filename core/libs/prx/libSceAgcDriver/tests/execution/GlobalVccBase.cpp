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
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t InputDwords = 768;
constexpr std::uint32_t Fill = 0xcdcdcdcdu;
constexpr std::uint32_t VectorBase = 512;
constexpr std::uint32_t DwordBase = 4;
constexpr std::uint32_t AddtidStoreBase = 256;
constexpr std::uint32_t AddtidBase = 384;
constexpr std::uint32_t AtomicBase = 448;
constexpr std::uint32_t OutputBase = 1024;
constexpr std::uint32_t OutputDwords = 8;

alignas(256) constexpr std::array<std::uint32_t, 22> VccBaseCode{
    0xbeea0400, 0x34020082, 0x34040084, 0x340c0085, 0x4a0c0cff, 0x00001000, 0x4ad404ff, 0x00000048,
    0xdc308010, 0x046a0001, 0xdcc98700, 0x0c6a0201, 0xdc3887b8, 0x006a006a, 0xbf8c3f70, 0xdc788000,
    0x006a0006, 0xdc708010, 0x006a0406, 0xdc708014, 0x006a0c06, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> VccAddtidCode{
    0xbeea0400, 0x340c0085, 0x4a0c0cff, 0x00001000, 0xdc588600, 0x056a0000, 0xbf8c3f70, 0xdc708018,
    0x006a0506, 0xdc5c8400, 0x006a0500, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "global vcc base: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
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
    for (std::uint32_t dword = 0; dword < InputDwords; ++dword) memory[dword] = 0x51000000u + dword * 0x00010203u;
    return memory;
}

std::vector<std::uint32_t> ExpectedBase() {
    const auto initial = Initial();
    auto memory = initial;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* out = &memory[OutputBase + tid * OutputDwords];
        for (std::uint32_t component = 0; component < 4u; ++component) out[component] = initial[VectorBase + tid * 4u + component];
        out[4] = initial[DwordBase + tid];
        out[5] = initial[AtomicBase + tid];
        memory[AtomicBase + tid] = initial[AtomicBase + tid] + tid * 16u;
    }
    return memory;
}

std::vector<std::uint32_t> ExpectedAddtid(std::uint32_t waveSize) {
    const auto initial = Initial();
    auto memory = initial;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t lane = tid % waveSize;
        memory[OutputBase + tid * OutputDwords + 6u] = initial[AddtidBase + lane];
        memory[AddtidStoreBase + lane] = initial[AddtidBase + lane];
    }
    return memory;
}

ShaderRecompiler::RecompileResult Recompile(std::span<const std::uint32_t> code, std::uint32_t waveSize, std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
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

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::vector<std::uint32_t>& expected, const std::string& name) {
    auto memory = Initial();
    std::memcpy(guest.Data(), memory.data(), BlockBytes);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    std::vector<std::uint32_t> userData(2, 0u);
    userData[0] = static_cast<std::uint32_t>(address);
    userData[1] = static_cast<std::uint32_t>(address >> 32u);
    const auto result = Recompile(code, waveSize, userData, target);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::memcpy(memory.data(), guest.Data(), BlockBytes);
    for (std::size_t dword = 0; dword < memory.size(); ++dword) {
        Require(memory[dword] == expected[dword], "global vcc base: " + name + " byte " + std::to_string(dword * 4u) + " is " + Hex(memory[dword]) + ", expected " + Hex(expected[dword]));
    }
}

void CheckRejected(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& reason) {
    const std::vector<std::uint32_t> userData(2, 0u);
    std::string failure;
    try {
        static_cast<void>(Recompile(code, 32, userData, device.Target()));
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find(reason) != std::string::npos, "global vcc base: expected '" + reason + "', got '" + failure + "'");
}

void CheckRejections(const AgcDriver::VulkanDevice& device) {
    alignas(256) static constexpr std::array<std::uint32_t, 3> vccHiLoad{0xdc308000, 0x046b0001, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> m0Atomic{0xdcc88000, 0x007c0201, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> execStore{0xdc708000, 0x007e0201, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> vccHiAddtid{0xdc588000, 0x056b0000, 0xbf810000};
    CheckRejected(device, vccHiLoad, "FLAT scalar address register range overflow");
    CheckRejected(device, m0Atomic, "FLAT scalar address register range overflow");
    CheckRejected(device, execStore, "FLAT scalar address register range overflow");
    CheckRejected(device, vccHiAddtid, "global_load_dword_addtid supports only an SGPR pair as base address");
}

void SkipUnlessWaveFitsSubgroup(const AgcDriver::VulkanDevice& device) {
    if (device.Target().subgroupSize < 32u) {
        Testing::Skip("the device's subgroups are narrower than a wave (" + std::to_string(device.Target().subgroupSize) + " lanes)");
    }
}

const Testing::Case outOfRangeBases{"GlobalVccBase_OutOfRangeScalarBase_IsRejected", [] {
    auto& device = SharedVulkanTestDevice();
    CheckRejections(device);
}};

const Testing::Case wave32Accesses{"GlobalVccBase_Wave32_AccessesThroughVccBase", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    Run(device, guest, VccBaseCode, 32, device.Target(), ExpectedBase(), "wave32");
}};

const Testing::Case wave64Accesses{"GlobalVccBase_Wave64_AccessesThroughVccBase", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    Run(device, guest, VccBaseCode, 64, device.Target(), ExpectedBase(), "wave64");
}};

const Testing::Case wave64SplitAccesses{"GlobalVccBase_Wave64SplitAcrossSubgroups_AccessesThroughVccBase", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    Run(device, guest, VccBaseCode, 64, device.ComputeTarget(32), ExpectedBase(), "wave64 split");
}};

const Testing::Case wave32Addtid{"GlobalVccBase_AddtidWave32_AccessesThroughVccBase", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessWaveFitsSubgroup(device);
    GuestBlock guest;
    Run(device, guest, VccAddtidCode, 32, device.Target(), ExpectedAddtid(32), "addtid wave32");
}};

const Testing::Case wave64Addtid{"GlobalVccBase_AddtidWave64_AccessesThroughVccBase", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessWaveFitsSubgroup(device);
    GuestBlock guest;
    Run(device, guest, VccAddtidCode, 64, device.Target(), ExpectedAddtid(64), "addtid wave64");
}};

const Testing::Case wave64SplitAddtid{"GlobalVccBase_AddtidWave64SplitAcrossSubgroups_AccessesThroughVccBase", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessWaveFitsSubgroup(device);
    GuestBlock guest;
    Run(device, guest, VccAddtidCode, 64, device.ComputeTarget(32), ExpectedAddtid(64), "addtid wave64 split");
}};

} // namespace
