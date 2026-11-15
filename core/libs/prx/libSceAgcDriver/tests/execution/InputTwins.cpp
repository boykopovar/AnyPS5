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
#include <chrono>
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
    Block(bool tracked, std::size_t bytes) : watched(tracked && AgcDriver::GuestMemory::WriteWatched()), size(bytes) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(size * 2u, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, size, PAGE_READWRITE, BlockBytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, size * 2u, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "input twins: cannot map a block");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + size != begin + size * 2u) munmap(reinterpret_cast<void*>(aligned + size), begin + size - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, size);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, size));
        }
#endif
        Require(data != nullptr, "input twins: cannot allocate a block");
        std::memset(data, 0, size);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, size, true, true, true);
    }

    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), size, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, size);
            GuestArena::GuestArenaRelease_nid_postfix(data, size * 2u);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, size);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, size);
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
    std::size_t size;
    std::uint8_t* data = nullptr;
};

std::vector<std::unique_ptr<Block>> blocks;

const Block& NewBlock(bool tracked, std::size_t bytes = BlockBytes) {
    blocks.push_back(std::make_unique<Block>(tracked, bytes));
    return *blocks.back();
}

std::array<std::uint32_t, 4> Descriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint64_t first, std::uint64_t second, std::uint32_t firstBytes = BufferBytes) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto one = Descriptor(first, firstBytes);
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
        now.memoised -= before.memoised;
        now.checked -= before.checked;
        now.verified -= before.verified;
        now.differing -= before.differing;
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

struct Settings {
    bool twins;
    bool staged;
    bool verdicts;
    bool verify;
};

std::string Verdicts(const InputTwinCounts& counts) {
    return std::to_string(counts.memoised) + " served by a memoised verdict, " + std::to_string(counts.checked) + " checked in full, " + std::to_string(counts.verified) + " verified, " + std::to_string(counts.differing) + " differing";
}

void OneEpoch(AgcDriver::VulkanDevice& device, const Settings& settings) {
    const auto& source = NewBlock(true);
    const auto& first = NewBlock(true);
    const auto& second = NewBlock(true);
    Fill(source, 14000);
    const auto toFirst = Copy(device, source.Address(), first.Address());
    const auto toSecond = Copy(device, source.Address(), second.Address());
    Run(device, toFirst);
    Run(device, toSecond);
    device.WaitIdle();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const Marks marks;
    constexpr std::uint32_t Dispatches = 400;
    const auto started = std::chrono::steady_clock::now();
    for (std::uint32_t use = 0; use < Dispatches; ++use) Run(device, use % 2u == 0 ? toFirst : toSecond);
    const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count();
    device.WaitIdle();
    Expect(first, 14000, "an input read by many dispatches of one collect epoch");
    Expect(second, 14000, "an input read by a second program in one collect epoch");
    const auto since = marks.Since();
    std::printf("input twins: %u dispatches of one collect epoch took %.2f us each (%s)\n", Dispatches, elapsed / Dispatches, Verdicts(since).c_str());
    ExpectOff(settings.twins, marks, "dispatches of one collect epoch");
    if (!settings.twins) return;
    Require(since.copies == 0 && since.reuses == Dispatches, "input twins: " + std::to_string(Dispatches) + " dispatches of one collect epoch gave " + Text(since) + ", expected a reuse each");
    if (!settings.verdicts) {
        Require(since.memoised == 0 && since.checked == Dispatches, "input twins: without memoised verdicts, " + std::to_string(Dispatches) + " dispatches gave " + Verdicts(since) + ", expected a full check each");
        return;
    }
    Require(since.checked == 1 && since.memoised == Dispatches - 1, "input twins: " + std::to_string(Dispatches) + " dispatches of two programs over one twin in one collect epoch gave " + Verdicts(since) + ", expected one full check");
    if (settings.verify) Require(since.verified == Dispatches && since.differing == 0, "input twins: the verification of memoised verdicts gave " + Verdicts(since));
}

void LargeInput(AgcDriver::VulkanDevice& device, const Settings& settings) {
    constexpr std::size_t InputBytes = std::size_t{2} << 20u;
    constexpr std::uint32_t Dispatches = 200;
    const auto& source = NewBlock(true, InputBytes);
    const auto& target = NewBlock(true);
    Fill(source, 20000);
    const auto copy = Compile(device, CopyAcross, source.Address(), target.Address(), static_cast<std::uint32_t>(InputBytes));
    Run(device, copy);
    Run(device, copy);
    device.WaitIdle();
    const auto timed = [&](bool ordered) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const auto started = std::chrono::steady_clock::now();
        for (std::uint32_t use = 0; use < Dispatches; ++use) {
            if (ordered) AgcDriver::GuestMemory::BumpCollectEpoch();
            Run(device, copy);
        }
        const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / Dispatches;
        device.WaitIdle();
        return elapsed;
    };
    const Marks marks;
    const auto inEpoch = timed(false);
    const auto epochs = timed(true);
    const auto since = marks.Since();
    double walk = 0;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto started = std::chrono::steady_clock::now();
        for (std::uint32_t use = 0; use < Dispatches; ++use) {
            AgcDriver::GuestMemory::BumpCollectEpoch();
            Require(AgcDriver::GuestMemory::CollectWrites(source.Address(), InputBytes) != 0, "input twins: a 2 MiB input is not write-watched");
        }
        walk = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / Dispatches;
    }
    Expect(target, 20000, "a 2 MiB input");
    std::printf("input twins: a dispatch over a 2 MiB input took %.2f us within one collect epoch and %.2f us behind an ordering point each (%s); a collect of the 2 MiB range in a new epoch took %.2f us\n", inEpoch, epochs, Verdicts(since).c_str(), walk);
    ExpectOff(settings.twins, marks, "a 2 MiB input");
    if (!settings.twins) return;
    Require(since.copies == 0 && since.reuses == Dispatches * 2u, "input twins: an unchanged 2 MiB input gave " + Text(since) + ", expected a reuse per dispatch");
    if (settings.verdicts) Require(since.checked == Dispatches + 1u && since.memoised == Dispatches - 1u, "input twins: an unchanged 2 MiB input gave " + Verdicts(since) + ", expected one full check per collect epoch");
}

void StoresInEpoch(AgcDriver::VulkanDevice& device, const Settings& settings) {
    const auto& source = NewBlock(true);
    const auto& target = NewBlock(true);
    const auto& origin = NewBlock(true);
    Fill(source, 15000);
    Fill(origin, 18000);
    const auto copy = Copy(device, source.Address(), target.Address());
    const auto produce = Copy(device, origin.Address(), source.Address());
    const auto twice = [&] {
        Run(device, copy);
        Run(device, copy);
        device.WaitIdle();
    };
    Run(device, copy);
    device.WaitIdle();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const Marks reused;
    twice();
    Expect(target, 15000, "an input reused within one collect epoch");
    const auto inEpoch = reused.Since();
    const Marks driver;
    std::array<std::uint32_t, Lanes> words{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) words[lane] = 16000 + lane;
    AgcDriver::GuestMemory::Write(source.Address(), std::as_bytes(std::span(words)));
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 16000, "an input after a driver store inside one collect epoch");
    const auto afterDriver = driver.Since();
    twice();
    const Marks cpu;
    Fill(source, 17000);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Run(device, copy);
    device.WaitIdle();
    Expect(target, 17000, "an input after a CPU store and an ordering point");
    const auto afterCpu = cpu.Since();
    twice();
    const Marks gpu;
    Run(device, produce);
    Run(device, copy);
    device.WaitIdle();
    Expect(source, 18000, "what a dispatch wrote into the next one's input inside one collect epoch");
    Expect(target, 18000, "an input written by a recorded dispatch inside one collect epoch");
    const auto afterGpu = gpu.Since();
    twice();
    Expect(target, 18000, "an input reused after a recorded write was copied");
    ExpectOff(settings.twins, reused, "stores inside one collect epoch");
    if (!settings.twins) return;
    if (settings.verdicts) Require(inEpoch.memoised == 1 && inEpoch.checked == 1, "input twins: two dispatches of one collect epoch gave " + Verdicts(inEpoch) + ", expected one full check and one memoised verdict");
    Require(afterDriver.copies == 1 && afterDriver.refreshes == 1 && afterDriver.outdated == 1 && afterDriver.memoised == 0, "input twins: a driver store inside one collect epoch gave " + Text(afterDriver) + ", " + Verdicts(afterDriver) + ", expected the twin outdated and refreshed");
    Require(afterCpu.copies == 1 && afterCpu.refreshes == 1 && afterCpu.memoised == 0, "input twins: a CPU store behind an ordering point gave " + Text(afterCpu) + ", " + Verdicts(afterCpu) + ", expected a refresh");
    Require(afterGpu.outdated == 1 && Missed(afterGpu, InputTwinMiss::PendingWrite) == 1 && afterGpu.refreshes == 1 && afterGpu.memoised == 0, "input twins: a recorded write over a twin inside one collect epoch gave " + Text(afterGpu) + ", " + Verdicts(afterGpu) + ", expected the twin outdated and copied behind the write");
}

void ProofOverTwins(AgcDriver::VulkanDevice& device, const Settings& settings) {
    using AgcDriver::Graphics::DrawProofCounted;
    using AgcDriver::Graphics::DrawProofKind;
    using AgcDriver::Graphics::DrawProofMiss;
    const auto& source = NewBlock(true);
    const auto& target = NewBlock(true);
    Fill(source, 19000);
    const auto shader = Copy(device, source.Address(), target.Address());
    const AgcDriver::Graphics::CompiledShader compiled{ShaderStage::Compute, &shader, 0};
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    device.WaitIdle();
    AgcDriver::GuestMemory::MarkProofThread();
    std::shared_ptr<AgcDriver::Graphics::ShaderResources> resources;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        resources = std::make_shared<AgcDriver::Graphics::ShaderResources>(context, compiled);
    }
    struct Release {
        AgcDriver::VulkanDevice& device;
        std::shared_ptr<AgcDriver::Graphics::ShaderResources>& resources;
        ~Release() {
            try {
                device.WaitIdle();
            } catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
            }
            resources.reset();
        }
    } release{device, resources};
    device.WaitIdle();
    Require(resources->Reusable(), "input twins: the build over a twin is not reusable");
    const auto revalidate = [&](const std::string& what) {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(resources->Revalidate(compiled), "input twins: " + what + " did not prove current");
    };
    const auto delta = [](const AgcDriver::Graphics::DrawProofCounts& before) {
        const auto& from = before[static_cast<std::size_t>(DrawProofKind::Resources)];
        auto to = DrawProofCounted()[static_cast<std::size_t>(DrawProofKind::Resources)];
        to.reused -= from.reused;
        to.proved -= from.proved;
        to.verified -= from.verified;
        to.differing -= from.differing;
        to.stored -= from.stored;
        for (std::size_t miss = 0; miss < to.missed.size(); ++miss) to.missed[miss] -= from.missed[miss];
        return to;
    };
    const auto skipped = [&](const AgcDriver::Graphics::DrawProofKindCounts& counts) { return settings.verify ? counts.verified : counts.reused; };
    const auto text = [](const AgcDriver::Graphics::DrawProofCounts& now, const AgcDriver::Graphics::DrawProofCounts& before) { return AgcDriver::Graphics::DrawProofText(now, before); };
    const bool proofs = AgcDriver::Graphics::DrawProofsEnabled() && !settings.staged;
    constexpr std::uint32_t Uses = 4;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    auto before = DrawProofCounted();
    for (std::uint32_t use = 0; use < Uses; ++use) revalidate("an unchanged build over a twin");
    const auto unchanged = delta(before);
    std::printf("input twins: %u proofs of an unchanged build: %s\n", Uses, text(DrawProofCounted(), before).c_str());
    if (proofs) Require(unchanged.proved == 1 && skipped(unchanged) == Uses - 1, "input twins: an unchanged build over a current twin did not take a proof once and reuse it");
    else Require(unchanged.reused == 0 && unchanged.verified == 0, "input twins: a build that takes no proof reused one");
    before = DrawProofCounted();
    const Marks marks;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(Recorder::Active() != nullptr, "input twins: no recorder is active");
        Recorder::Active()->NotePendingWrite(source.Address(), BufferBytes);
    }
    revalidate("a build over a twin outdated by a recorded write");
    const auto outdated = delta(before);
    const auto copied = marks.Since();
    std::printf("input twins: a proof after a recorded write over the twin: %s\n", text(DrawProofCounted(), before).c_str());
    if (proofs && settings.twins) Require(outdated.missed[static_cast<std::size_t>(DrawProofMiss::InputTwins)] == 1 && skipped(outdated) == 0, "input twins: a build whose twin a recorded write outdated kept its proof");
    if (proofs && !settings.twins) Require(skipped(outdated) == 1, "input twins: a recorded write over an input read in place broke the proof");
    if (settings.twins) Require(copied.copies == 1 && Missed(copied, InputTwinMiss::PendingWrite) == 1, "input twins: the build whose twin a recorded write outdated gave " + Text(copied) + ", expected a copy behind the write");
    device.WaitIdle();
    before = DrawProofCounted();
    const Marks settled;
    for (std::uint32_t use = 0; use < Uses; ++use) revalidate("a build over a twin once the write completed");
    const auto again = delta(before);
    std::printf("input twins: %u proofs once the write completed: %s\n", Uses, text(DrawProofCounted(), before).c_str());
    if (proofs) Require(skipped(again) >= Uses - 2 && again.proved <= 1, "input twins: a build whose twin was refreshed did not take a proof again");
    if (proofs && settings.twins) Require(again.proved == 1 && settled.Since().refreshes == 1, "input twins: a build whose twin was copied behind a write did not refresh it once before its proof");
    if (settings.verify) Require(unchanged.differing == 0 && outdated.differing == 0 && again.differing == 0 && unchanged.stored == 0 && again.stored == 0, "input twins: the full checks beside reused proofs over twins found a difference");
    const auto twinCounts = AgcDriver::Graphics::InputTwinCounters();
    Require(twinCounts.differing == 0, "input twins: " + std::to_string(twinCounts.differing) + " memoised verdicts differed from the full check");
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
            const Settings settings{twins, mode != "--unstaged", std::getenv("APS5_NO_DRAW_PROOFS") == nullptr, std::getenv("APS5_VERIFY_DRAW_PROOFS") != nullptr};
            OneEpoch(*device, settings);
            std::puts("input twins: dispatches of one collect epoch checked their shared twin once");
            LargeInput(*device, settings);
            std::puts("input twins: a 2 MiB input was checked once per collect epoch");
            StoresInEpoch(*device, settings);
            std::puts("input twins: driver stores, CPU stores behind an ordering point and recorded writes refreshed a twin inside one collect epoch");
            ProofOverTwins(*device, settings);
            std::puts("input twins: a build over current twins took and reused a proof and lost it when a twin was outdated");
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
