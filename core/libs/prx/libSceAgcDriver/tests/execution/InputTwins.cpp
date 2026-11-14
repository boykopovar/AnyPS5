#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::InputTwinCounts;
using AgcDriver::Graphics::InputTwinMiss;
using AgcDriver::Graphics::Recorder;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t BufferBytes = Lanes * 4u;

alignas(256) constexpr std::array<std::uint32_t, 7> CopyAcross{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> AddOne{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040481u, 0xe0701000u, 0x80000201u, 0xbf810000u};

struct Block {
    explicit Block(bool tracked) : watched(tracked && AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes * 2u, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, BlockBytes * 2u, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "input twins: cannot map a block");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes * 2u) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
        }
#endif
        Require(data != nullptr, "input twins: cannot allocate a block");
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, BlockBytes, true, true, true);
    }

    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes * 2u);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, BlockBytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
        } else {
            std::free(data);
        }
#endif
    }

    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;

    std::uint64_t Address(std::size_t offset = 0) const { return reinterpret_cast<std::uintptr_t>(data) + offset; }
    std::uint32_t* Words(std::size_t offset = 0) const { return reinterpret_cast<std::uint32_t*>(data + offset); }

    bool watched;
    std::uint8_t* data = nullptr;
};

std::vector<std::unique_ptr<Block>> blocks;

const Block& NewBlock(bool tracked) {
    blocks.push_back(std::make_unique<Block>(tracked));
    return *blocks.back();
}

std::array<std::uint32_t, 4> Descriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint64_t first, std::uint64_t second) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto one = Descriptor(first, BufferBytes);
    std::copy(one.begin(), one.end(), userData.begin());
    if (second != 0) {
        const auto two = Descriptor(second, BufferBytes);
        std::copy(two.begin(), two.end(), userData.begin() + 4);
    }
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

ShaderRecompiler::RecompileResult Copy(AgcDriver::VulkanDevice& device, std::uint64_t from, std::uint64_t to) {
    return Compile(device, CopyAcross, from, to);
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& shader, std::span<const std::uint32_t> code = CopyAcross) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    device.Dispatch(shader, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

void Fill(const Block& block, std::uint32_t seed, std::size_t offset = 0) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) block.Words(offset)[lane] = seed + lane;
}

void Expect(const Block& block, std::uint32_t seed, const std::string& what, std::size_t offset = 0) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto found = block.Words(offset)[lane];
        Require(found == seed + lane, "input twins, " + what + ": dword " + std::to_string(lane) + " is " + std::to_string(found) + ", expected " + std::to_string(seed + lane));
    }
}

void EndFrame() {
    AgcDriver::Graphics::NoteHostImportFrame();
}

struct Marks {
    InputTwinCounts before = AgcDriver::Graphics::InputTwinCounters();

    InputTwinCounts Since() const {
        auto now = AgcDriver::Graphics::InputTwinCounters();
        now.made -= before.made;
        now.shared -= before.shared;
        now.copies -= before.copies;
        now.copiedBytes -= before.copiedBytes;
        now.refreshes -= before.refreshes;
        now.reuses -= before.reuses;
        now.outdated -= before.outdated;
        now.demoted -= before.demoted;
        for (std::size_t miss = 0; miss < AgcDriver::Graphics::InputTwinMisses; ++miss) now.missed[miss] -= before.missed[miss];
        return now;
    }
};

std::uint64_t Missed(const InputTwinCounts& counts, InputTwinMiss miss) {
    return counts.missed[static_cast<std::size_t>(miss)];
}

std::string Text(const InputTwinCounts& counts) {
    auto text = std::to_string(counts.made) + " made, " + std::to_string(counts.shared) + " shared, " + std::to_string(counts.copies) + " copies, " + std::to_string(counts.refreshes) + " refreshes, " + std::to_string(counts.reuses) + " reuses, " + std::to_string(counts.outdated) + " outdated";
    for (std::size_t miss = 0; miss < AgcDriver::Graphics::InputTwinMisses; ++miss) {
        if (counts.missed[miss] != 0) text += ", " + std::to_string(counts.missed[miss]) + " " + AgcDriver::Graphics::InputTwinMissName(static_cast<InputTwinMiss>(miss));
    }
    return text;
}

void ExpectOff(bool twins, const Marks& marks, const std::string& what) {
    if (twins) return;
    const auto since = marks.Since();
    Require(since.made == 0 && since.copies == 0 && since.reuses == 0 && since.shared == 0, "input twins, " + what + ": " + Text(since) + " although the twins are off");
}

bool Tracked(const AgcDriver::VulkanDevice& device, const Block& block) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    return AgcDriver::Graphics::WatchedAfterSettle(context, block.Address(), BufferBytes) && AgcDriver::GuestMemory::CollectWrites(block.Address(), BufferBytes) != 0;
}

bool UnchangedInput(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& source = NewBlock(true);
    const auto& first = NewBlock(true);
    const auto& second = NewBlock(true);
    Fill(source, 1000);
    const auto toFirst = Copy(device, source.Address(), first.Address());
    const auto toSecond = Copy(device, source.Address(), second.Address());
    const Marks marks;
    Run(device, toFirst);
    device.WaitIdle();
    Expect(first, 1000, "the first dispatch over an input");
    if (twins && !Tracked(device, source)) return false;
    constexpr std::uint32_t Frames = 4;
    constexpr std::uint32_t PerFrame = 6;
    for (std::uint32_t frame = 0; frame < Frames; ++frame) {
        for (std::uint32_t use = 0; use < PerFrame; ++use) Run(device, use % 2u == 0 ? toFirst : toSecond);
        device.WaitIdle();
        EndFrame();
    }
    Expect(first, 1000, "an unchanged input read by one program");
    Expect(second, 1000, "an unchanged input read by a second program");
    ExpectOff(twins, marks, "an unchanged input");
    if (!twins) return true;
    const auto since = marks.Since();
    const auto uses = std::uint64_t{Frames} * PerFrame + 1u;
    Require(since.made == 1 && since.shared >= 1, "input twins: two programs over one input made " + std::to_string(since.made) + " twins and shared " + std::to_string(since.shared) + ", expected one twin shared");
    Require(since.copies == 1 && since.copies + since.reuses == uses, "input twins: an unchanged input was copied " + std::to_string(since.copies) + " times and reused " + std::to_string(since.reuses) + " times over " + std::to_string(uses) + " dispatches, expected one copy");
    Require(since.copiedBytes == since.copies * BufferBytes, "input twins: " + std::to_string(since.copiedBytes) + " bytes were copied by " + std::to_string(since.copies) + " copies of " + std::to_string(BufferBytes) + " bytes");
    return true;
}

void CpuStoreBetween(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& source = NewBlock(true);
    const auto& target = NewBlock(true);
    Fill(source, 2000);
    const auto copy = Copy(device, source.Address(), target.Address());
    const auto& other = NewBlock(true);
    const auto toOther = Copy(device, source.Address(), other.Address());
    Run(device, copy);
    device.WaitIdle();
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 2000, "an input before a CPU store");
    const Marks marks;
    Fill(source, 3000);
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 3000, "an input after a guest store");
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 3000, "an input reused after a guest store");
    std::array<std::uint32_t, Lanes> words{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) words[lane] = 4000 + lane;
    AgcDriver::GuestMemory::Write(source.Address(), std::as_bytes(std::span(words)));
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 4000, "an input after a driver store");
    const auto since = marks.Since();
    const Marks batched;
    Run(device, copy);
    Fill(source, 5000);
    Run(device, toOther);
    device.WaitIdle();
    Expect(other, 5000, "an input stored between two dispatches of one batch");
    ExpectOff(twins, marks, "a CPU store between dispatches");
    if (!twins) return;
    Require(since.refreshes == 2 && since.copies == 2 && since.reuses == 1, "input twins: two CPU stores between four dispatches gave " + Text(since) + ", expected two refreshes and one reuse");
    const auto inBatch = batched.Since();
    Require(inBatch.reuses == 1 && inBatch.refreshes == 1 && inBatch.shared == 1, "input twins: a CPU store between two dispatches of one batch gave " + Text(inBatch) + ", expected a reuse, then the second program's refresh of the shared twin");
}

void BehindPendingWrite(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& origin = NewBlock(true);
    const auto& middle = NewBlock(true);
    const auto& target = NewBlock(true);
    Fill(origin, 5000);
    Fill(middle, 4000);
    const auto produce = Copy(device, origin.Address(), middle.Address());
    const auto consume = Copy(device, middle.Address(), target.Address());
    Run(device, consume);
    device.WaitIdle();
    Run(device, consume);
    device.WaitIdle();
    Expect(target, 4000, "an input no dispatch wrote yet");
    const Marks marks;
    Run(device, produce);
    Run(device, consume);
    const auto recorded = marks.Since();
    device.WaitIdle();
    Expect(middle, 5000, "what a dispatch wrote into the next one's input");
    Expect(target, 5000, "an input behind a pending GPU write");
    if (twins) Require(Missed(recorded, InputTwinMiss::PendingWrite) == 1 && recorded.copies >= 1, "input twins: a dispatch reading behind a pending write counted " + std::to_string(Missed(recorded, InputTwinMiss::PendingWrite)) + " pending misses and " + std::to_string(recorded.copies) + " copies, expected one miss with its copy");
    Fill(origin, 6000);
    Run(device, produce);
    Run(device, consume);
    device.WaitIdle();
    Expect(target, 6000, "an input behind a second pending GPU write");
    const Marks settled;
    Run(device, consume);
    device.WaitIdle();
    Run(device, consume);
    device.WaitIdle();
    Expect(target, 6000, "an input once its writer completed");
    ExpectOff(twins, marks, "an input behind a pending write");
    if (!twins) return;
    const auto since = settled.Since();
    Require(since.copies == 1 && since.reuses == 1, "input twins: two dispatches after the writer completed gave " + Text(since) + ", expected one copy then one reuse");
}

void ReadAndWritten(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& block = NewBlock(true);
    Fill(block, 7000);
    const auto add = Compile(device, AddOne, block.Address(), 0);
    const auto onto = Copy(device, block.Address(), block.Address());
    const Marks marks;
    for (std::uint32_t use = 0; use < 3; ++use) Run(device, add, AddOne);
    Run(device, onto);
    device.WaitIdle();
    Expect(block, 7003, "a region both read and written");
    const auto since = marks.Since();
    Require(since.made == 0 && since.copies == 0, "input twins: a region both read and written gave " + Text(since) + ", expected no twin");
    ExpectOff(twins, marks, "a region both read and written");
}

void OverBudget(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& kept = NewBlock(true);
    const auto& refused = NewBlock(true);
    const auto& later = NewBlock(true);
    const auto& target = NewBlock(true);
    Fill(kept, 8000);
    Fill(refused, 8100);
    Fill(later, 8200);
    const auto overKept = Copy(device, kept.Address(), target.Address());
    const auto overRefused = Copy(device, refused.Address(), target.Address(BufferBytes));
    const auto overLater = Copy(device, later.Address(), target.Address(BufferBytes * 2u));
    const auto& ballast = NewBlock(true);
    for (std::uint32_t part = 0; part < 4; ++part) Run(device, Copy(device, ballast.Address(part * BufferBytes), target.Address(BufferBytes * 3u)));
    device.WaitIdle();
    const auto resident = AgcDriver::Graphics::InputTwinCounters().residentBytes;
    const auto limit = resident + BufferBytes + BufferBytes / 2u;
    AgcDriver::Graphics::SetInputTwinLimitForTests(limit);
    try {
        const Marks marks;
        Run(device, overKept);
        Run(device, overRefused);
        Run(device, overRefused);
        device.WaitIdle();
        Expect(target, 8000, "an input within the budget");
        Expect(target, 8100, "an input over the budget", BufferBytes);
        const auto since = marks.Since();
        if (twins) {
            Require(since.made == 1 && Missed(since, InputTwinMiss::Budget) >= 1, "input twins: a second input over the budget gave " + Text(since) + " and " + std::to_string(Missed(since, InputTwinMiss::Budget)) + " budget misses, expected one twin and a miss");
            Require(AgcDriver::Graphics::InputTwinCounters().residentBytes <= limit, "input twins: " + std::to_string(AgcDriver::Graphics::InputTwinCounters().residentBytes) + " bytes are resident over a limit of " + std::to_string(limit));
        }
        Fill(refused, 8150);
        Run(device, overRefused);
        device.WaitIdle();
        Expect(target, 8150, "an input left in place after a guest store", BufferBytes);
        for (std::uint32_t frame = 0; frame < 4; ++frame) {
            Run(device, overKept);
            device.WaitIdle();
            EndFrame();
        }
        Run(device, overLater);
        Run(device, overRefused);
        device.WaitIdle();
        Expect(target, 8200, "an input built once older twins were released", BufferBytes * 2u);
        Expect(target, 8150, "an input left in place after older twins were released", BufferBytes);
        if (twins) Require(AgcDriver::Graphics::InputTwinCounters().residentBytes <= limit, "input twins: " + std::to_string(AgcDriver::Graphics::InputTwinCounters().residentBytes) + " bytes are resident over a limit of " + std::to_string(limit) + " after older twins were released");
        ExpectOff(twins, marks, "inputs over the budget");
    } catch (...) {
        AgcDriver::Graphics::SetInputTwinLimitForTests(0);
        throw;
    }
    AgcDriver::Graphics::SetInputTwinLimitForTests(0);
}

void Evicted(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& source = NewBlock(true);
    const auto& target = NewBlock(true);
    Fill(source, 11000);
    const auto copy = Copy(device, source.Address(), target.Address());
    Run(device, copy);
    device.WaitIdle();
    for (std::uint32_t frame = 0; frame < 3; ++frame) EndFrame();
    const auto before = AgcDriver::Graphics::InputTwinCounters().residentBytes;
    const Marks marks;
    Run(device, copy);
    AgcDriver::Graphics::ReleaseTextureCaches(std::uint64_t{1} << 40u);
    device.WaitIdle();
    Expect(target, 11000, "an input read while its twin was evicted");
    AgcDriver::Graphics::ReleaseTextureCaches(std::uint64_t{1} << 40u);
    const auto after = AgcDriver::Graphics::InputTwinCounters().residentBytes;
    if (twins) Require(after + BufferBytes <= before, "input twins: releasing the caches left " + std::to_string(after) + " of " + std::to_string(before) + " twin bytes resident, expected the evicted build's twin freed");
    Fill(source, 12000);
    AgcDriver::Graphics::SetDeviceLocalFullForTests(true);
    try {
        Run(device, copy);
    } catch (...) {
        AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
        throw;
    }
    AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
    device.WaitIdle();
    Expect(target, 12000, "an input whose twin was evicted, with device memory refused");
    const auto refused = marks.Since();
    Fill(source, 13000);
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 13000, "an input left in place after its twin was evicted");
    ExpectOff(twins, marks, "an evicted input");
    if (!twins) return;
    Require(refused.made == 0 && refused.copies == 0 && Missed(refused, InputTwinMiss::Allocation) >= 1, "input twins: a dispatch after its twin was evicted, with device memory refused, gave " + Text(refused) + ", expected it read in place");
}

void RefusedAllocation(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& source = NewBlock(true);
    const auto& target = NewBlock(true);
    Fill(source, 9000);
    const auto copy = Copy(device, source.Address(), target.Address());
    const Marks marks;
    AgcDriver::Graphics::SetDeviceLocalFullForTests(true);
    try {
        Run(device, copy);
    } catch (...) {
        AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
        throw;
    }
    AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
    const auto since = marks.Since();
    device.WaitIdle();
    Expect(target, 9000, "an input refused device memory");
    Fill(source, 9100);
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 9100, "an input refused device memory after a guest store");
    if (twins) Require(since.made == 0 && since.copies == 0 && Missed(since, InputTwinMiss::Allocation) >= 1, "input twins: an input refused device memory gave " + Text(since) + " and " + std::to_string(Missed(since, InputTwinMiss::Allocation)) + " allocation misses, expected no twin and a miss");
    ExpectOff(twins, marks, "an input refused device memory");
}

void Untracked(AgcDriver::VulkanDevice& device, bool twins) {
    const auto& source = NewBlock(false);
    const auto& target = NewBlock(true);
    Fill(source, 10000);
    const auto copy = Copy(device, source.Address(), target.Address());
    const Marks marks;
    constexpr std::uint32_t Uses = 12;
    for (std::uint32_t use = 0; use < Uses; ++use) {
        Fill(source, 10000 + use * 100u);
        Run(device, copy);
        device.WaitIdle();
        Expect(target, 10000 + use * 100u, "an untracked input, use " + std::to_string(use));
    }
    ExpectOff(twins, marks, "an untracked input");
    if (!twins) return;
    const auto since = marks.Since();
    if (Tracked(device, source)) {
        std::puts("input twins: the device tracks memory outside the guest arena, so the generation-0 path was not taken");
        return;
    }
    Require(since.reuses == 0 && Missed(since, InputTwinMiss::Untracked) >= 1, "input twins: an untracked input gave " + Text(since) + " and " + std::to_string(Missed(since, InputTwinMiss::Untracked)) + " untracked misses, expected a copy per use and no reuse");
    Require(since.demoted == 1 && since.copies < Uses && Missed(since, InputTwinMiss::Volatile) >= 1, "input twins: an input copied on every use gave " + Text(since) + ", " + std::to_string(since.demoted) + " demotions and " + std::to_string(Missed(since, InputTwinMiss::Volatile)) + " volatile misses, expected it left in place after a few uses");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("input twins: cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("input twins: cannot set ") + name);
#endif
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        Require(mode.empty() || mode == "--off" || mode == "--unstaged", "input twins: unknown mode " + mode);
        const bool twins = mode != "--off";
        SetEnvironment(twins ? "APS5_INPUT_TWINS" : "APS5_NO_INPUT_TWINS", "1");
        SetEnvironment("APS5_INPUT_TWIN_MIN_KIB", "0");
        if (mode != "--unstaged") {
            SetEnvironment("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
            SetEnvironment("APS5_NO_CHANGED_COPY_BACK", "1");
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        struct ReleaseBlocks {
            AgcDriver::VulkanDevice& device;
            ~ReleaseBlocks() {
                try {
                    device.WaitIdle();
                } catch (const std::exception& error) {
                    std::cerr << error.what() << '\n';
                }
                blocks.clear();
            }
        } releaseBlocks{*device};
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if (context.hostImportAlignment == 0) {
            std::puts("skipped, host imports are unavailable, so no dispatch input is read out of one");
            return VulkanTestSkipped;
        }
        Require(AgcDriver::Graphics::InputTwinsEnabled(context) == twins, "input twins: the environment does not match the run");
        {
            if (!UnchangedInput(*device, twins)) {
                device->WaitIdle();
                std::puts("skipped, the device does not keep host imports write-watched, so a twin cannot be proved unchanged");
                return VulkanTestSkipped;
            }
            std::puts("input twins: an unchanged input was copied once over many dispatches and frames");
            CpuStoreBetween(*device, twins);
            std::puts("input twins: CPU stores between dispatches reached the next dispatch");
            BehindPendingWrite(*device, twins);
            std::puts("input twins: an input behind a pending GPU write was read after its writer");
            ReadAndWritten(*device, twins);
            std::puts("input twins: a region both read and written kept its staging");
            OverBudget(*device, twins);
            std::puts("input twins: inputs over the budget were read in place");
            Evicted(*device, twins);
            std::puts("input twins: an input whose twin was evicted was read in place");
            RefusedAllocation(*device, twins);
            std::puts("input twins: an input refused device memory was read in place");
            Untracked(*device, twins);
            std::puts("input twins: an untracked input was read afresh by every dispatch");
            device->WaitIdle();
        }
        device->WaitIdle();
        const auto counts = AgcDriver::Graphics::InputTwinCounters();
        std::printf("input twins: %s, %llu bytes copied, %llu outdated, %llu demoted, %llu bytes resident\n", Text(counts).c_str(), static_cast<unsigned long long>(counts.copiedBytes), static_cast<unsigned long long>(counts.outdated), static_cast<unsigned long long>(counts.demoted), static_cast<unsigned long long>(counts.residentBytes));
        std::printf("input twins tests passed%s\n", mode.empty() ? "" : (" in mode " + mode).c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
