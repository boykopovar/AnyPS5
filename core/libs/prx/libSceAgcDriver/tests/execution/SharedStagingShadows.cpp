#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
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
#include <mutex>
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
constexpr std::uint32_t Programs = 6;

struct Program {
    alignas(256) std::array<std::uint32_t, 8> words;
};

std::array<Program, Programs> Adders{};
alignas(256) constexpr std::array<std::uint32_t, 7> CopyAcross{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};

struct Mode {
    bool active = false;
    bool shared = false;
    bool changed = false;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "shared staging shadows: cannot allocate a guest block");
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

ShaderRecompiler::RecompileResult Adder(AgcDriver::VulkanDevice& device, std::uint32_t program, std::uint64_t address) {
    return Compile(device, Adders[program].words, Descriptor(address, BufferBytes), {});
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& shader, std::uint32_t program) {
    device.Dispatch(shader, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Adders[program].words.data()));
}

struct Counts {
    Recorder::HandoverStatistics before = Recorder::HandoverCounts();

    Recorder::HandoverStatistics Since() const {
        auto now = Recorder::HandoverCounts();
        now.taken -= before.taken;
        now.skipped -= before.skipped;
        now.redirected -= before.redirected;
        now.forwardedBytes -= before.forwardedBytes;
        now.elided -= before.elided;
        now.elidedBytes -= before.elidedBytes;
        now.landed -= before.landed;
        now.landedBytes -= before.landedBytes;
        for (std::size_t site = 0; site < Recorder::LandingSites; ++site) now.landedAt[site] -= before.landedAt[site];
        return now;
    }
};

std::uint64_t At(const Recorder::HandoverStatistics& counts, Recorder::LandingSite site) {
    return counts.landedAt[static_cast<std::size_t>(site)];
}

std::size_t Pending() {
    const auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "shared staging shadows: no active recorder");
    return recorder->PendingCopyBacks();
}

void Expect(const std::uint32_t* words, std::uint32_t count, std::uint32_t expected, const std::string& what) {
    for (std::uint32_t lane = 0; lane < count; ++lane) Require(words[lane] == expected, "shared staging shadows, " + what + ": dword " + std::to_string(lane) + " is " + std::to_string(words[lane]) + ", expected " + std::to_string(expected));
}

void ExpectCounts(const Mode& mode, const Recorder::HandoverStatistics& since, std::uint64_t elided, std::uint64_t landed, const std::string& what) {
    if (!mode.active) {
        Require(since.taken == 0 && since.elided == 0 && since.landed == 0, "shared staging shadows, " + what + ": copy-backs were deferred although the handover is not in use");
        return;
    }
    Require(since.elided == elided && since.landed == landed, "shared staging shadows, " + what + ": " + std::to_string(since.elided) + " copy-backs elided and " + std::to_string(since.landed) + " landed, expected " + std::to_string(elided) + " and " + std::to_string(landed));
}

bool ProbeDeferral(AgcDriver::VulkanDevice& device) {
    const GuestBlock block;
    const auto addOne = Adder(device, 0, block.Address());
    const Counts counts;
    Run(device, addOne, 0);
    device.WaitIdle();
    Expect(block.Words(), Lanes, 1, "one dispatch");
    return counts.Since().landed == 1;
}

void AlternatingPrograms(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock block;
    const auto addOne = Adder(device, 0, block.Address());
    const auto addTwo = Adder(device, 1, block.Address());
    const Counts counts;
    for (std::uint32_t pair = 0; pair < 3; ++pair) {
        Run(device, addOne, 0);
        Run(device, addTwo, 1);
    }
    device.WaitIdle();
    Expect(block.Words(), Lanes, 9, "two programs alternating over one region");
    const auto since = counts.Since();
    ExpectCounts(mode, since, 5, 1, "two programs alternating over one region");
    if (!mode.active) return;
    if (mode.shared) Require(since.skipped == 5 && since.redirected == 0, "shared staging shadows: two programs over one region skipped " + std::to_string(since.skipped) + " copy-ins and redirected " + std::to_string(since.redirected) + ", expected 5 and 0");
    else Require(since.redirected == 5, "shared staging shadows: two programs with their own shadows redirected " + std::to_string(since.redirected) + " copy-ins, expected 5");
}

void AlternatingGroups(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock first;
    const GuestBlock second;
    const auto overFirst = Adder(device, 0, first.Address());
    const auto overSecond = Adder(device, 0, second.Address());
    const Counts counts;
    for (std::uint32_t round = 0; round < 3; ++round) {
        Run(device, overFirst, 0);
        Run(device, overSecond, 0);
    }
    if (mode.active) Require(Pending() == 2, "shared staging shadows: two alternating groups left " + std::to_string(Pending()) + " copy-backs pending, expected 2");
    const auto recorded = counts.Since();
    if (mode.active) Require(recorded.landed == 0, "shared staging shadows: " + std::to_string(recorded.landed) + " copy-backs of two unrelated groups landed while they alternated");
    device.WaitIdle();
    Expect(first.Words(), Lanes, 3, "the first of two alternating groups");
    Expect(second.Words(), Lanes, 3, "the second of two alternating groups");
    const auto since = counts.Since();
    ExpectCounts(mode, since, 4, 2, "two alternating groups");
    if (mode.active) Require(since.skipped == 4 && since.redirected == 0, "shared staging shadows: two alternating groups skipped " + std::to_string(since.skipped) + " copy-ins and redirected " + std::to_string(since.redirected) + ", expected 4 and 0");
}

void ReaderInPlace(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock source;
    const GuestBlock other;
    const GuestBlock target;
    const auto overSource = Adder(device, 2, source.Address());
    const auto overOther = Adder(device, 0, other.Address());
    const auto copy = Compile(device, CopyAcross, Descriptor(source.Address(), BufferBytes), Descriptor(target.Address(), BufferBytes));
    const Counts counts;
    Run(device, overSource, 2);
    Run(device, overOther, 0);
    if (mode.active) Require(Pending() == 2, "shared staging shadows: two unrelated dispatches left " + std::to_string(Pending()) + " copy-backs pending, expected 2");
    device.Dispatch(copy, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
    const auto recorded = counts.Since();
    if (mode.active) {
        Require(recorded.landed == 1 && At(recorded, Recorder::LandingSite::StagingReach) + At(recorded, Recorder::LandingSite::DispatchReach) == 1, "shared staging shadows: a dispatch reading a range with a pending copy-back landed " + std::to_string(recorded.landed) + " copy-backs, expected the one it reads");
        Require(Pending() == 2, "shared staging shadows: after a reader of one pending range " + std::to_string(Pending()) + " copy-backs are pending, expected the unrelated one and its own");
    }
    device.WaitIdle();
    Expect(source.Words(), Lanes, 3, "the range a later dispatch read in place");
    Expect(other.Words(), Lanes, 1, "the unrelated range");
    Expect(target.Words(), Lanes, 3, "what the reader copied out of the range");
    ExpectCounts(mode, counts.Since(), 0, 3, "a reader of a pending range");
}

void LiveShadowBytes(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock block;
    std::vector<ShaderRecompiler::RecompileResult> shaders;
    for (std::uint32_t program = 0; program < Programs; ++program) shaders.push_back(Adder(device, program, block.Address()));
    const auto hits = AgcDriver::Graphics::SharedStagingShadowHits();
    Run(device, shaders[0], 0);
    const auto afterFirst = AgcDriver::Graphics::LiveStagingShadowBytes();
    for (std::uint32_t program = 1; program < Programs; ++program) Run(device, shaders[program], program);
    const auto afterAll = AgcDriver::Graphics::LiveStagingShadowBytes();
    const auto shared = AgcDriver::Graphics::SharedStagingShadowHits() - hits;
    device.WaitIdle();
    Expect(block.Words(), Lanes, 21, "six programs over one region");
    if (!mode.shared) {
        Require(shared == 0, "shared staging shadows: " + std::to_string(shared) + " shadows were shared although sharing is off");
        return;
    }
    Require(shared >= Programs - 1, "shared staging shadows: six builds of one region shared its shadow " + std::to_string(shared) + " times, expected 5 or 6");
    if (!mode.changed) Require(afterAll == afterFirst, "shared staging shadows: live shadow bytes went from " + std::to_string(afterFirst) + " to " + std::to_string(afterAll) + " over six builds of one region");
}

void FillBetween(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock block;
    const auto addOne = Adder(device, 0, block.Address());
    const auto addTwo = Adder(device, 1, block.Address());
    const Counts counts;
    Run(device, addOne, 0);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint32_t, 4> pattern{7u, 7u, 7u, 7u};
        Require(device.FillBuffer(block.Address(), BufferBytes, pattern), "shared staging shadows: the fill over the staged region was not recorded on the GPU");
    }
    if (mode.active) Require(counts.Since().landed == 1 && Pending() == 0, "shared staging shadows: a fill over a region with a pending copy-back did not land it first");
    Run(device, addTwo, 1);
    device.WaitIdle();
    Expect(block.Words(), Lanes, 9, "a dispatch, a fill over its region, then another program");
    ExpectCounts(mode, counts.Since(), 0, 2, "a fill between two users");
}

void HalfOverlap(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock block;
    const auto low = Adder(device, 0, block.Address());
    const auto high = Adder(device, 0, block.Address(BufferBytes / 2u));
    const Counts counts;
    for (std::uint32_t round = 0; round < 3; ++round) {
        Run(device, low, 0);
        Run(device, high, 0);
    }
    device.WaitIdle();
    Expect(block.Words(), Lanes / 2u, 3, "the part only the lower range covers");
    Expect(block.Words() + Lanes / 2u, Lanes / 2u, 6, "the part both ranges cover");
    Expect(block.Words() + Lanes, Lanes / 2u, 3, "the part only the upper range covers");
    Expect(block.Words() + Lanes + Lanes / 2u, Lanes / 2u, 0, "the dwords past both ranges");
    ExpectCounts(mode, counts.Since(), 0, 6, "two half-overlapping ranges");
}

void SubmitBetween(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock block;
    const auto addOne = Adder(device, 0, block.Address());
    const auto addTwo = Adder(device, 1, block.Address());
    const Counts counts;
    Run(device, addOne, 0);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.SubmitRecorded(false);
    }
    Run(device, addTwo, 1);
    device.WaitIdle();
    Expect(block.Words(), Lanes, 3, "two users of one shadow in two batches");
    ExpectCounts(mode, counts.Since(), 0, 2, "a submit between two users");
}

void FullDeviceMemory(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const GuestBlock block;
    const auto addOne = Adder(device, 0, block.Address());
    const auto addTwo = Adder(device, 1, block.Address());
    const Counts counts;
    Run(device, addOne, 0);
    AgcDriver::Graphics::SetDeviceLocalFullForTests(true);
    const auto refused = AgcDriver::Graphics::StagingShadowsRefused();
    try {
        Run(device, addTwo, 1);
    } catch (...) {
        AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
        throw;
    }
    AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
    const auto refusals = AgcDriver::Graphics::StagingShadowsRefused() - refused;
    device.WaitIdle();
    Expect(block.Words(), Lanes, 3, "a second build while device memory reports full");
    if (mode.shared && !mode.changed) {
        Require(refusals == 0, "shared staging shadows: a build of a region with a live shadow was refused " + std::to_string(refusals) + " shadows by a full device heap");
        ExpectCounts(mode, counts.Since(), 1, 1, "a second build while device memory reports full");
    } else if (!mode.shared) {
        Require(refusals != 0, "shared staging shadows: a full device heap refused no shadow of a build with its own");
    }
}

void FailingUseBetween(AgcDriver::VulkanDevice& device, const Mode& mode) {
    const auto* limit = device.WorkGroupCountLimit();
    if (limit[0] == 0xffffffffu) {
        std::puts("shared staging shadows: skipped the failing use, the device bounds no group count");
        return;
    }
    const GuestBlock block;
    const auto addOne = Adder(device, 0, block.Address());
    const auto addTwo = Adder(device, 1, block.Address());
    auto refused = Adder(device, 2, block.Address());
    std::erase_if(refused.specialization, [](const auto& constant) { return ShaderRecompiler::PipelineSpecialization::IsWorkgroupOffset(constant.id); });
    const Counts counts;
    Run(device, addOne, 0);
    bool threw = false;
    try {
        device.Dispatch(refused, limit[0] + 1u, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Adders[2].words.data()));
    } catch (const std::exception& error) {
        threw = std::string(error.what()).find("exceeds device limits") != std::string::npos;
        if (!threw) throw;
    }
    Require(threw, "shared staging shadows: a dispatch above the group count limit without a workgroup offset was recorded");
    Run(device, addTwo, 1);
    device.WaitIdle();
    Expect(block.Words(), Lanes, 3, "a failing use between two users of one shadow");
    ExpectCounts(mode, counts.Since(), 1, 1, "a failing use between two users");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("shared staging shadows: cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("shared staging shadows: cannot set ") + name);
#endif
}

}

int main(int argc, char** argv) {
    try {
        const std::string run = argc > 1 ? argv[1] : "";
        Require(run.empty() || run == "--off" || run == "--private" || run == "--changed", "shared staging shadows: unknown mode " + run);
        Mode mode;
        mode.changed = run == "--changed";
        mode.shared = run != "--private";
        SetEnvironment("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
        SetEnvironment(mode.changed ? "APS5_CHANGED_COPY_BACK" : "APS5_NO_CHANGED_COPY_BACK", "1");
        if (run == "--off") SetEnvironment("APS5_NO_STAGING_HANDOVER", "1");
        if (run == "--private") SetEnvironment("APS5_NO_SHARED_STAGING_SHADOWS", "1");
        for (std::uint32_t program = 0; program < Programs; ++program) Adders[program].words = {0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040481u + program, 0xe0701000u, 0x80000201u, 0xbf810000u};
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        mode.active = ProbeDeferral(*device);
        const bool deferring = run.empty() || run == "--private";
        Require(deferring || !mode.active, "shared staging shadows: a copy-back was deferred in a mode without the handover");
        if (deferring && !mode.active) {
            device->WaitIdle();
            std::puts("skipped, the device stages no written buffer in device memory");
            return VulkanTestSkipped;
        }
        AlternatingPrograms(*device, mode);
        std::puts("shared staging shadows: two programs over one region kept every increment");
        AlternatingGroups(*device, mode);
        std::puts("shared staging shadows: two alternating groups kept every increment");
        ReaderInPlace(*device, mode);
        std::puts("shared staging shadows: a reader of a pending range read what its writer stored");
        LiveShadowBytes(*device, mode);
        std::puts("shared staging shadows: six builds of one region kept every increment");
        FillBetween(*device, mode);
        std::puts("shared staging shadows: a fill between two users landed in order");
        HalfOverlap(*device, mode);
        std::puts("shared staging shadows: two half-overlapping ranges kept every increment");
        SubmitBetween(*device, mode);
        std::puts("shared staging shadows: two users in two batches kept every increment");
        FullDeviceMemory(*device, mode);
        std::puts("shared staging shadows: a second build under a full device heap kept every increment");
        FailingUseBetween(*device, mode);
        std::puts("shared staging shadows: a failing use between two users left their increments");
        device->WaitIdle();
        const auto counts = Recorder::HandoverCounts();
        for (std::size_t site = 0; site < Recorder::LandingSites; ++site) {
            if (counts.landedAt[site] != 0) std::printf("shared staging shadows: %llu copy-backs landed by %s\n", static_cast<unsigned long long>(counts.landedAt[site]), Recorder::LandingSiteName(static_cast<Recorder::LandingSite>(site)));
        }
        std::printf("shared staging shadows tests passed%s\n", run.empty() ? "" : (" in mode " + run).c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
