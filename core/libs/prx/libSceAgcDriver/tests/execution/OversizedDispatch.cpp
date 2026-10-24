#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <algorithm>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Period = 4;
constexpr std::uint32_t Groups = 129199u;
constexpr std::uint32_t Stores = Groups * Threads;

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

class GuestBlock {
public:
    explicit GuestBlock(std::size_t dwords) : Bytes(dwords * sizeof(std::uint32_t)) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(256, Bytes));
#endif
        Require(block != nullptr, "oversized dispatch: cannot allocate the guest block");
        Clear();
        GuestAllocations::Mutation().Add(block, Bytes, true, true, true);
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

    void Clear() const { std::memset(block, 0, Bytes); }
    auto* Data() const { return reinterpret_cast<std::uint32_t*>(block); }
    const std::size_t Bytes;

private:
    std::uint8_t* block = nullptr;
};

void CheckSplitCoverage(AgcDriver::VulkanDevice& device) {
    const GuestBlock guest(Stores);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::vector<std::uint32_t> userData{
        static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u),
        Stores, 0x00014004u, Pattern[0], Pattern[1], Pattern[2], Pattern[3], Stores, Period};
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}, std::nullopt, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, Groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(guest.Data()), guest.Bytes);
    for (std::uint32_t i = 0; i < Stores; ++i) {
        Require(guest.Data()[i] == Pattern[i % Period], "oversized dispatch: dword " + std::to_string(i) + " is " + Hex(guest.Data()[i]) + ", expected " + Hex(Pattern[i % Period]));
    }
}


constexpr std::array<std::uint32_t, 69> LdsCode{
    0x34020084, 0xe0381000, 0x80000401, 0x7e100208, 0x7e3e0209, 0x343e3e82, 0x4a103f08, 0x7e40020a,
    0x34404083, 0x4a104108, 0x34121090, 0x4a121300, 0x4a1412ff, 0x10000000, 0x7e160280, 0x7e180280,
    0x7e1a0281, 0x7e1e0281, 0x7e200280, 0xbf8c0000, 0xd9340000, 0x00000b06, 0xd834000c, 0x00000b06,
    0xd8340000, 0x00000904, 0xd8340000, 0x00000a05, 0xd8340000, 0x00000907, 0xbf8c0000, 0xbf8a0000,
    0xd880000c, 0x28000d06, 0xd9800000, 0x29000f06, 0x3a260884, 0x3a2a0aff, 0x00000080, 0x363608ff,
    0xfffffff8, 0xbf8c0000, 0xbf8a0000, 0xd8d80000, 0x2b000013, 0xd8d80000, 0x2c000015, 0xd8d80000,
    0x2d000007, 0xd8d8000c, 0x2e000006, 0xd9d80000, 0x2f000006, 0xd9d80000, 0x3100001b, 0xd8d80000,
    0x33000005, 0x343c1086, 0x4a3c3d00, 0x343c3c86, 0xbf8c0000, 0xe0781000, 0x8001281e, 0xe0781010,
    0x80012c1e, 0xe0781020, 0x8001301e, 0xbf8c0000, 0xbf810000,
};
alignas(256) std::array<std::uint32_t, Threads * 4> LdsInput{};
alignas(256) std::array<std::uint32_t, Threads * 4> LdsOutput{};

void CheckLdsSplitThrows(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    const auto ldsBytes = target.maxWorkgroupSharedMemoryBytes + 4096u;
    std::vector<std::uint32_t> userData(8, 0u);
    const auto descriptor = [&](const void* data, std::uint32_t bytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(data);
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
    };
    const auto input = descriptor(LdsInput.data(), static_cast<std::uint32_t>(LdsInput.size() * 4u));
    const auto output = descriptor(LdsOutput.data(), static_cast<std::uint32_t>(LdsOutput.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(LdsCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, ldsBytes / 4u, {true, true, true}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target, {0, 0, 0, 128}, std::nullopt, false};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(result.workgroupMemoryDwords != 0u, "oversized dispatch: the LDS shader did not take the device-memory path");
    for (const auto z : {1u, std::numeric_limits<std::uint32_t>::max()}) {
        bool threw = false;
        try {
            device.Dispatch(result, 1, device.WorkGroupCountLimit()[1] + 1u, z, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        } catch (const std::runtime_error& error) {
            Require(std::string(error.what()).find("a split aliases the device-memory LDS slots") != std::string::npos, "oversized dispatch: wrong LDS split refusal: " + std::string(error.what()));
            threw = true;
        }
        device.WaitIdle();
        Require(threw, "oversized dispatch: a Y count over the limit with device-memory LDS did not throw");
    }
}

alignas(256) constexpr std::array<std::uint32_t, 6> GroupYCode{
    0x7e000204u, 0x34020082u, 0x4a040081u, 0xe0701000u, 0x80000201u, 0xbf810000u};
constexpr std::uint32_t GroupYSplitCap = 1u << 20u;
constexpr std::uint32_t GroupYTail = 4097u;

bool CheckGroupYSplitCoverage(AgcDriver::VulkanDevice& device) {
    const auto limit = device.WorkGroupCountLimit()[1];
    if (limit >= GroupYSplitCap) {
        std::printf("oversized dispatch Y split: skipped, the device reports a %u Y workgroup limit\n", limit);
        return false;
    }
    const auto groups = limit + GroupYTail;
    const GuestBlock guest(groups);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::vector<std::uint32_t> userData{
        static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), groups * 4u, 0x31016facu};
    const std::span<const std::uint32_t> code(GroupYCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, true, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}, std::nullopt, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, groups, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(guest.Data()), guest.Bytes);
    for (std::uint32_t group = 0; group < groups; ++group) {
        Require(guest.Data()[group] == group + 1u, "oversized dispatch Y split: dword " + std::to_string(group) + " is " + std::to_string(guest.Data()[group]) + ", expected " + std::to_string(group + 1u));
    }
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckLdsSplitThrows(*device);
        bool split = CheckGroupYSplitCoverage(*device);
        if (device->WorkGroupCountLimit()[0] < Groups) {
            CheckSplitCoverage(*device);
            split = true;
        } else {
            std::printf("oversized dispatch X split: skipped, the device reports a %u workgroup limit, which never splits a %ux1x1 dispatch\n", device->WorkGroupCountLimit()[0], Groups);
        }
        if (!split) {
            AgcDriverShutdown_nid_postfix();
            return VulkanTestSkipped;
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("oversized dispatch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
