#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "PipelineSpecialization.hpp"
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
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 8> AddOne{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040481u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> AddTwo{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040482u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> AddThree{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040483u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> CopyAcross{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "staging handover: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
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

    std::uint32_t* Words() const { return block; }
    std::uint64_t Address(std::size_t offset = 0) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)) + offset; }

private:
    std::uint32_t* block = nullptr;
};

std::array<std::uint32_t, 4> Descriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 4>& first, const std::array<std::uint32_t, 4>& second) {
    std::vector<std::uint32_t> userData(8, 0u);
    std::copy(first.begin(), first.end(), userData.begin());
    std::copy(second.begin(), second.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct Counts {
    Recorder::HandoverStatistics before = Recorder::HandoverCounts();

    Recorder::HandoverStatistics Since() const {
        const auto now = Recorder::HandoverCounts();
        return {now.taken - before.taken, now.skipped - before.skipped, now.redirected - before.redirected, now.forwardedBytes - before.forwardedBytes, now.elided - before.elided, now.elidedBytes - before.elidedBytes, now.landed - before.landed, now.landedBytes - before.landedBytes};
    }
};

void ExpectLanes(const std::uint32_t* words, std::uint32_t base, std::uint32_t step, const std::string& what) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto expected = base + step * lane;
        Require(words[lane] == expected, "staging handover, " + what + ": lane " + std::to_string(lane) + " is " + std::to_string(words[lane]) + ", expected " + std::to_string(expected));
    }
}

bool ProbeDeferral(AgcDriver::VulkanDevice& device, const GuestBlock& block, const ShaderRecompiler::RecompileResult& addOne) {
    const Counts counts;
    device.Dispatch(addOne, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddOne.data()));
    device.WaitIdle();
    ExpectLanes(block.Words(), 1, 0, "one dispatch");
    const auto since = counts.Since();
    Require(since.elided == 0 && since.taken == 0, "staging handover: a single dispatch took a copy-back over");
    return since.landed == 1;
}

void RepeatedBuild(AgcDriver::VulkanDevice& device, const GuestBlock& block, const ShaderRecompiler::RecompileResult& addOne, bool active) {
    constexpr std::uint32_t Dispatches = 6;
    const auto start = block.Words()[0];
    const Counts counts;
    for (std::uint32_t dispatch = 0; dispatch < Dispatches; ++dispatch) device.Dispatch(addOne, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddOne.data()));
    device.WaitIdle();
    ExpectLanes(block.Words(), start + Dispatches, 0, "six dispatches of one program over one buffer");
    const auto since = counts.Since();
    if (!active) {
        Require(since.taken == 0 && since.elided == 0 && since.landed == 0, "staging handover: copy-backs were deferred although the handover is off");
        return;
    }
    Require(since.elided == Dispatches - 1, "staging handover: six dispatches of one program elided " + std::to_string(since.elided) + " copy-backs, expected 5");
    Require(since.landed == 1, "staging handover: six dispatches of one program landed " + std::to_string(since.landed) + " copy-backs, expected 1");
    Require(since.skipped + since.redirected == Dispatches - 1, "staging handover: six dispatches of one program read " + std::to_string(since.skipped + since.redirected) + " copy-ins from the previous shadow, expected 5");
    Require(since.forwardedBytes == std::uint64_t{BufferBytes} * (Dispatches - 1), "staging handover: six dispatches of one program kept " + std::to_string(since.forwardedBytes) + " bytes off the import, expected " + std::to_string(BufferBytes * (Dispatches - 1)));
}

void AlternatingBuilds(AgcDriver::VulkanDevice& device, const GuestBlock& block, const ShaderRecompiler::RecompileResult& addOne, const ShaderRecompiler::RecompileResult& addTwo, bool active) {
    constexpr std::uint32_t Pairs = 3;
    const auto start = block.Words()[0];
    const Counts counts;
    for (std::uint32_t pair = 0; pair < Pairs; ++pair) {
        device.Dispatch(addOne, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddOne.data()));
        device.Dispatch(addTwo, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddTwo.data()));
    }
    device.WaitIdle();
    ExpectLanes(block.Words(), start + Pairs * 3u, 0, "two programs alternating over one buffer");
    const auto since = counts.Since();
    if (!active) {
        Require(since.taken == 0 && since.elided == 0 && since.landed == 0, "staging handover: copy-backs were deferred although the handover is off");
        return;
    }
    Require(since.elided == Pairs * 2u - 1u, "staging handover: two alternating programs elided " + std::to_string(since.elided) + " copy-backs, expected 5");
    Require(since.landed == 1, "staging handover: two alternating programs landed " + std::to_string(since.landed) + " copy-backs, expected 1");
    Require(since.redirected >= 1, "staging handover: no copy-in of two alternating programs read the other program's shadow");
}

void UnwrittenBytesBetween(AgcDriver::VulkanDevice& device, bool active) {
    const GuestBlock block;
    const auto copy = Compile(device, CopyAcross, Descriptor(block.Address(), BufferBytes * 2u), Descriptor(block.Address(BufferBytes), BufferBytes));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) block.Words()[lane] = 100u + lane;
    const Counts counts;
    device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) block.Words()[lane] = 500u + lane;
    device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
    device.WaitIdle();
    ExpectLanes(block.Words() + Lanes, 500, 1, "bytes the first dispatch left alone, stored by the CPU before the second");
    ExpectLanes(block.Words(), 500, 1, "the CPU's bytes outside the written range");
    const auto since = counts.Since();
    if (!active) return;
    Require(since.elided == 1 && since.landed == 1, "staging handover: two dispatches over a partly written region elided " + std::to_string(since.elided) + " and landed " + std::to_string(since.landed) + " copy-backs, expected 1 and 1");
    Require(since.forwardedBytes == BufferBytes, "staging handover: a partly written region kept " + std::to_string(since.forwardedBytes) + " bytes off the import, expected the " + std::to_string(BufferBytes) + " written ones");
}

void FailingTaker(AgcDriver::VulkanDevice& device, bool active) {
    const GuestBlock block;
    const auto descriptor = Descriptor(block.Address(), BufferBytes);
    const auto addOne = Compile(device, AddOne, descriptor, {});
    auto refused = Compile(device, AddThree, descriptor, {});
    std::erase_if(refused.specialization, [](const auto& constant) { return ShaderRecompiler::PipelineSpecialization::IsWorkgroupOffset(constant.id); });
    const auto* limit = device.WorkGroupCountLimit();
    if (limit[0] == 0xffffffffu) {
        std::puts("staging handover: skipped the failing taker, the device bounds no group count");
        return;
    }
    const Counts counts;
    device.Dispatch(addOne, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddOne.data()));
    bool threw = false;
    try {
        device.Dispatch(refused, limit[0] + 1u, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddThree.data()));
    } catch (const std::exception& error) {
        threw = std::string(error.what()).find("exceeds device limits") != std::string::npos;
        if (!threw) throw;
    }
    Require(threw, "staging handover: a dispatch above the group count limit without a workgroup offset was recorded");
    device.WaitIdle();
    ExpectLanes(block.Words(), 1, 0, "a dispatch whose successor failed after reading its shadow");
    const auto since = counts.Since();
    if (active) {
        Require(since.taken == 1 && since.redirected == 1, "staging handover: the failing dispatch took " + std::to_string(since.taken) + " handovers and redirected " + std::to_string(since.redirected) + " copy-ins, expected 1 and 1");
        Require(since.elided == 0 && since.landed == 1, "staging handover: after a failing taker " + std::to_string(since.elided) + " copy-backs were elided and " + std::to_string(since.landed) + " landed, expected 0 and 1");
    }
    device.Dispatch(addOne, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(AddOne.data()));
    device.WaitIdle();
    ExpectLanes(block.Words(), 2, 0, "a dispatch after the failed one");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("staging handover: cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("staging handover: cannot set ") + name);
#endif
}

}

int main(int argc, char** argv) {
    try {
        const bool off = argc > 1 && std::string(argv[1]) == "--off";
        SetEnvironment("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
        SetEnvironment("APS5_NO_CHANGED_COPY_BACK", "1");
        if (off) SetEnvironment("APS5_NO_STAGING_HANDOVER", "1");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(Recorder::StagingHandoverEnabled() == !off, "staging handover: APS5_NO_STAGING_HANDOVER does not match the run");
        {
            const GuestBlock block;
            const auto descriptor = Descriptor(block.Address(), BufferBytes);
            const auto addOne = Compile(*device, AddOne, descriptor, {});
            const auto addTwo = Compile(*device, AddTwo, descriptor, {});
            const bool active = ProbeDeferral(*device, block, addOne);
            Require(!(off && active), "staging handover: a copy-back was deferred although the handover is off");
            if (!off && !active) {
                device->WaitIdle();
                std::puts("skipped, the device stages no written buffer in device memory");
                return VulkanTestSkipped;
            }
            RepeatedBuild(*device, block, addOne, active);
            std::puts("staging handover: six dispatches of one program kept every increment");
            AlternatingBuilds(*device, block, addOne, addTwo, active);
            std::puts("staging handover: two alternating programs kept every increment");
            UnwrittenBytesBetween(*device, active);
            std::puts("staging handover: bytes outside the written range came from guest memory");
            FailingTaker(*device, active);
            std::puts("staging handover: a failing taker left the earlier results in guest memory");
        }
        device->WaitIdle();
        std::puts(off ? "staging handover tests passed with the handover off" : "staging handover tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
