#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
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
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint64_t Block = 65536;
constexpr std::uint64_t HeapBytes = 2u << 20u;
constexpr std::uint64_t StoreOffset = 5 * Block + 0x200;
constexpr std::uint64_t Neighbor = StoreOffset + Threads * sizeof(std::uint32_t);
constexpr std::uint32_t Marker = 0x5a000000u;
constexpr std::uint64_t LastRun = 12 * Block;
constexpr std::uint64_t LabelOffset = 9 * Block + 0x40;
constexpr std::uint32_t Reused = 0x99999999u;
constexpr std::array<std::pair<std::uint64_t, std::uint64_t>, 3> SparseRuns{{{0, 2 * Block}, {4 * Block, 8 * Block}, {LastRun, LastRun + 2 * Block}}};

alignas(256) constexpr std::array<std::uint32_t, 6> StoreCode{0x34020082, 0x380402ff, Marker, 0xdc708000, 0x00000201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 8> MixedCode{0x34020082, 0x380402ff, Marker, 0xdc708000, 0x00000201, 0xe0701000, 0x80010201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{0x34060082, 0xe0301000, 0x80000403, 0xbf8c3f70, 0xe0701000, 0x80010403, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 11> StoreLoadCode{0x34020082, 0x380402ff, Marker, 0xdc708000, 0x00000201, 0xdc308000, 0x03000001, 0xbf8c3f70, 0xe0701000, 0x80010301, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 7> LoadCode{0x34020082, 0xdc308000, 0x03000001, 0xbf8c3f70, 0xe0701000, 0x80010301, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 6> StoreByteCode{0x34020082, 0x380402ff, Marker, 0xdc608000, 0x00000201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 5> ZeroStoreCode{0x34020082, 0x7e040280, 0xdc708000, 0x00000201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 5> ZeroByteStoreCode{0x34020082, 0x7e040280, 0xdc608000, 0x00000201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 9> PagesCode{0x3402008c, 0x380402ff, Marker, 0xdc708000, 0x00000201, 0x34060082, 0xdc708000, 0x00020203, 0xbf810000};

class OutputBlock {
public:
    OutputBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, Block, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(Block, Block));
#endif
        Require(block != nullptr, "BDA store heap mirror: cannot allocate the output block");
        GuestAllocations::Mutation().Add(block, Block, true, true, true);
    }

    ~OutputBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    OutputBlock(const OutputBlock&) = delete;
    OutputBlock& operator=(const OutputBlock&) = delete;

    std::uint32_t* Data() const { return block; }

private:
    std::uint32_t* block = nullptr;
};

class GuestHeap {
public:
    explicit GuestHeap(bool writable, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs = {}, std::uint64_t bytes = HeapBytes) : bytes(bytes) {
#ifdef _WIN32
        raw = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
        Require(raw != nullptr, "BDA store heap mirror: cannot allocate the guest heap");
        base = static_cast<std::uint8_t*>(raw);
        if (runs.empty()) GuestArena::GuestArenaCommit_nid_postfix(raw, bytes, PAGE_READWRITE, bytes);
        for (const auto& [begin, end] : runs) GuestArena::GuestArenaCommit_nid_postfix(base + begin, end - begin, PAGE_READWRITE, Block);
#else
        raw = mmap(nullptr, bytes + Block, runs.empty() ? PROT_READ | PROT_WRITE : PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        Require(raw != MAP_FAILED, "BDA store heap mirror: cannot map the guest heap");
        base = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(raw) + Block - 1) / Block * Block);
        for (const auto& [begin, end] : runs) Require(mprotect(base + begin, end - begin, PROT_READ | PROT_WRITE) == 0, "BDA store heap mirror: cannot commit a run of the sparse guest heap");
#endif
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, bytes);
        GuestAllocations::Mutation().Add(base, bytes, true, writable, true);
    }

    ~GuestHeap() {
        GuestAllocations::Mutation().Remove(base);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, bytes);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(raw, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(raw, bytes);
#else
        munmap(raw, bytes + Block);
#endif
    }

    GuestHeap(const GuestHeap&) = delete;
    GuestHeap& operator=(const GuestHeap&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(base); }
    std::uint64_t Bytes() const { return bytes; }

    void Store(std::uint64_t offset, std::uint32_t value) const { std::memcpy(base + offset, &value, sizeof(value)); }
    void StoreByte(std::uint64_t offset, std::uint8_t value) const { base[offset] = value; }
    std::uint8_t LoadByte(std::uint64_t offset) const { return base[offset]; }

    std::uint32_t Load(std::uint64_t offset) const {
        std::uint32_t value = 0;
        std::memcpy(&value, base + offset, sizeof(value));
        return value;
    }

private:
    std::uint64_t bytes = HeapBytes;
    void* raw = nullptr;
    std::uint8_t* base = nullptr;
};

struct AddressSpaceLimit {
    explicit AddressSpaceLimit(std::uint64_t bytes) { AgcDriver::Graphics::SetResidentAddressSpaceLimit(bytes); }
    ~AddressSpaceLimit() { AgcDriver::Graphics::SetResidentAddressSpaceLimit(0); }
    AddressSpaceLimit(const AddressSpaceLimit&) = delete;
    AddressSpaceLimit& operator=(const AddressSpaceLimit&) = delete;
};

struct ImportBudget {
    explicit ImportBudget(std::uint64_t bytes) { AgcDriver::Graphics::SetHostImportBudget(bytes); }
    ~ImportBudget() { AgcDriver::Graphics::SetHostImportBudget(0); }
    ImportBudget(const ImportBudget&) = delete;
    ImportBudget& operator=(const ImportBudget&) = delete;
};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }

    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::uint32_t Stored(std::uint32_t thread) {
    return Marker | (thread * 4u);
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, std::uint32_t threads = Threads) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, regions},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::string Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const std::function<void()>& during = {}, std::uint32_t threads = Threads) {
    const auto result = Compile(device, code, userData, threads);
    StderrCapture log;
    std::string thrown;
    try {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        if (during) during();
        device.WaitIdle();
    } catch (const std::exception& error) {
        thrown = error.what();
    }
    return log.Finish() + thrown;
}

std::vector<std::uint32_t> AddressData(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
}

std::string Store(AgcDriver::VulkanDevice& device, std::uint64_t address, const std::function<void()>& during = {}) {
    return Run(device, StoreCode, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)}, during);
}

void Copy(AgcDriver::VulkanDevice& device, std::uint64_t source, const OutputBlock& output) {
    std::fill(output.Data(), output.Data() + Threads, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto in = BufferDescriptor(source, static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    const auto out = BufferDescriptor(reinterpret_cast<std::uintptr_t>(output.Data()), static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    std::copy(in.begin(), in.end(), userData.begin());
    std::copy(out.begin(), out.end(), userData.begin() + 4);
    const auto log = Run(device, CopyCode, userData);
    Require(log.find("failed") == std::string::npos, "BDA store heap mirror: the copy dispatch failed:\n" + log);
}

void WritableTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    const OutputBlock output;
    heap.Store(Neighbor, 0x11111111u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto log = Store(device, heap.Address() + StoreOffset, [&] { heap.Store(Neighbor + 2 * sizeof(std::uint32_t), 0x44444444u); });
    Require(AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + HeapBytes).mirrors == 1, "BDA store heap mirror: the writable heap was not heap mirrored");
    Require(log.find("BDA access failed") == std::string::npos && log.find("write-back failed") == std::string::npos, "BDA store heap mirror: a FLAT/GLOBAL store into a writable heap mirror was refused:\n" + log);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto value = heap.Load(StoreOffset + thread * sizeof(std::uint32_t));
        Require(value == Stored(thread), "BDA store heap mirror: the GLOBAL store of thread " + std::to_string(thread) + " into the writable heap mirror reached guest memory as " + Hex(value) + ", expected " + Hex(Stored(thread)));
    }
    Require(heap.Load(Neighbor) == 0x11111111u, "BDA store heap mirror: the write-back changed a guest dword no thread stored to");
    Require(heap.Load(Neighbor + 2 * sizeof(std::uint32_t)) == 0x44444444u, "BDA store heap mirror: the write-back rolled back a CPU store made during the dispatch");
    heap.Store(Neighbor + sizeof(std::uint32_t), 0x22222222u);
    Copy(device, heap.Address() + StoreOffset, output);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(output.Data()[thread] == Stored(thread), "BDA store heap mirror: after a CPU write into the same block the mirror holds " + Hex(output.Data()[thread]) + " for thread " + std::to_string(thread) + ", expected the stored " + Hex(Stored(thread)));
    }
    Copy(device, heap.Address() + Neighbor, output);
    Require(output.Data()[0] == 0x11111111u && output.Data()[1] == 0x22222222u, "BDA store heap mirror: the refreshed mirror lost the CPU's bytes next to the stores");
    const auto again = Store(device, heap.Address() + StoreOffset + Block);
    Require(again.find("BDA access failed") == std::string::npos, "BDA store heap mirror: a second store dispatch was refused:\n" + again);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(heap.Load(StoreOffset + Block + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the second dispatch's store of thread " + std::to_string(thread) + " did not reach guest memory");
        Require(heap.Load(StoreOffset + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the second dispatch's write-back rolled back the first dispatch's store of thread " + std::to_string(thread));
    }
    Require(heap.Load(Neighbor) == 0x11111111u && heap.Load(Neighbor + sizeof(std::uint32_t)) == 0x22222222u, "BDA store heap mirror: a later write-back rolled back the CPU's bytes");
}

void MixedTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto address = heap.Address() + StoreOffset;
    const auto declared = heap.Address() + StoreOffset + 3 * Block;
    std::vector<std::uint32_t> userData(8, 0u);
    userData[0] = static_cast<std::uint32_t>(address);
    userData[1] = static_cast<std::uint32_t>(address >> 32u);
    const auto view = BufferDescriptor(declared, static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    std::copy(view.begin(), view.end(), userData.begin() + 4);
    heap.Store(StoreOffset + 3 * Block + Threads * sizeof(std::uint32_t), 0x55555555u);
    const auto log = Run(device, MixedCode, userData);
    Require(log.find("BDA access failed") == std::string::npos && log.find("write-back failed") == std::string::npos, "BDA store heap mirror: a program storing through an address and a V# into one heap failed:\n" + log);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(heap.Load(StoreOffset + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the GLOBAL store of thread " + std::to_string(thread) + " next to a V# store was lost");
        Require(heap.Load(StoreOffset + 3 * Block + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the V# store of thread " + std::to_string(thread) + " into the heap mirror was lost");
    }
    Require(heap.Load(StoreOffset + 3 * Block + Threads * sizeof(std::uint32_t)) == 0x55555555u, "BDA store heap mirror: the V# range's write-back changed a dword past it");
}

void SparseTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true, SparseRuns);
    heap.Store(Neighbor, 0x11111111u);
    heap.Store(LastRun, 0x66666666u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto log = Store(device, heap.Address() + StoreOffset, [&] { heap.Store(LastRun + sizeof(std::uint32_t), 0x77777777u); });
    Require(AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + HeapBytes).mirrors == 1, "BDA store heap mirror: the sparse writable heap was not heap mirrored");
    Require(log.find("BDA access failed") == std::string::npos && log.find("write-back failed") == std::string::npos, "BDA store heap mirror: a FLAT/GLOBAL store into a committed run of a sparse heap mirror was refused:\n" + log);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto value = heap.Load(StoreOffset + thread * sizeof(std::uint32_t));
        Require(value == Stored(thread), "BDA store heap mirror: the GLOBAL store of thread " + std::to_string(thread) + " into a sparse heap mirror reached guest memory as " + Hex(value) + ", expected " + Hex(Stored(thread)));
    }
    Require(heap.Load(Neighbor) == 0x11111111u, "BDA store heap mirror: the sparse write-back changed a guest dword no thread stored to");
    Require(heap.Load(LastRun) == 0x66666666u && heap.Load(LastRun + sizeof(std::uint32_t)) == 0x77777777u, "BDA store heap mirror: the sparse write-back rolled back the CPU's bytes in another committed run");
    const auto again = Store(device, heap.Address() + LastRun + 0x200);
    Require(again.find("BDA access failed") == std::string::npos && again.find("write-back failed") == std::string::npos, "BDA store heap mirror: a store into the last committed run of a sparse heap mirror was refused:\n" + again);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(heap.Load(LastRun + 0x200 + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the store of thread " + std::to_string(thread) + " into the last committed run did not reach guest memory");
        Require(heap.Load(StoreOffset + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: a store into another committed run rolled back the first run's store of thread " + std::to_string(thread));
    }
    Require(heap.Load(LastRun + sizeof(std::uint32_t)) == 0x77777777u, "BDA store heap mirror: a later sparse write-back rolled back the CPU's bytes");
}

struct LabelOutcome {
    std::string log;
    std::uint64_t overLabels = 0;
    std::uint64_t countedLabels = 0;
    std::uint32_t value = 0;
};

LabelOutcome StoreBesideLabel(AgcDriver::VulkanDevice& device, const GuestHeap& heap, std::uint64_t labelOffset) {
    constexpr std::array<std::byte, 4> label{std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    using AgcDriver::Graphics::Recorder;
    LabelOutcome outcome;
    const auto overBefore = Recorder::WriteBacksOverLabels();
    outcome.log = Store(device, heap.Address() + StoreOffset, [&] {
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "BDA store heap mirror: no active recorder");
        heap.Store(labelOffset, Reused);
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto counted = Recorder::PendingCompletionLabels();
        recorder->AfterCompletions(heap.Address() + labelOffset, label, 1, 0, true);
        outcome.countedLabels = Recorder::PendingCompletionLabels() - counted;
    });
    outcome.overLabels = Recorder::WriteBacksOverLabels() - overBefore;
    outcome.value = heap.Load(labelOffset);
    return outcome;
}

void LabelTests(AgcDriver::VulkanDevice& device) {
    {
        GuestHeap heap(true);
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        const auto outcome = StoreBesideLabel(device, heap, LabelOffset);
        Require(outcome.log.find("BDA access failed") == std::string::npos && outcome.log.find("write-back failed") == std::string::npos, "BDA store heap mirror: a store beside a completion label failed:\n" + outcome.log);
        Require(outcome.countedLabels == 0, "BDA store heap mirror: a GPU-stored completion label counted as pending at its registration");
        for (std::uint32_t thread = 0; thread < Threads; ++thread) {
            Require(heap.Load(StoreOffset + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the GLOBAL store of thread " + std::to_string(thread) + " beside a completion label was lost");
        }
        Require(outcome.overLabels == 0, "BDA store heap mirror: the heap span's write-back was noted over a completion label no thread stored to (" + std::to_string(outcome.overLabels) + " write-backs over a tracked label)");
        Require(outcome.value == Reused, "BDA store heap mirror: the completion store of a label no thread stored to was forced by the heap span's write-back and left " + Hex(outcome.value) + " over the CPU's " + Hex(Reused));
    }
    {
        GuestHeap heap(true);
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        const auto outcome = StoreBesideLabel(device, heap, StoreOffset + 8 * sizeof(std::uint32_t));
        Require(outcome.log.find("BDA access failed") == std::string::npos && outcome.log.find("write-back failed") == std::string::npos, "BDA store heap mirror: a store over a completion label failed:\n" + outcome.log);
        Require(outcome.overLabels >= 1, "BDA store heap mirror: the write-back of a thread's store over a completion label was not noted over the label");
        Require(outcome.value == 1u, "BDA store heap mirror: the completion store of a label a thread stored over did not run after the write-back (the label holds " + Hex(outcome.value) + ")");
        Require(heap.Load(StoreOffset + 7 * sizeof(std::uint32_t)) == Stored(7) && heap.Load(StoreOffset + 9 * sizeof(std::uint32_t)) == Stored(9), "BDA store heap mirror: the stores next to a completion label were lost");
    }
}

constexpr std::uint64_t Page = 4096;

struct Compared {
    std::uint64_t span = 0;
    std::uint64_t compared = 0;
    std::uint64_t copiedReference = 0;
    std::uint64_t readReference = 0;
    std::uint64_t gathered = 0;
    std::uint64_t spilled = 0;
    std::uint64_t readback = 0;
    std::uint64_t masked = 0;
};

Compared CountCompared(const std::function<void()>& work) {
    const auto before = AgcDriver::Graphics::AddressStoreCounters();
    work();
    const auto after = AgcDriver::Graphics::AddressStoreCounters();
    return {after.spanBytes - before.spanBytes, after.comparedBytes - before.comparedBytes, after.copiedReferenceBytes - before.copiedReferenceBytes, after.readReferenceBytes - before.readReferenceBytes, after.gatheredBytes - before.gatheredBytes, after.spilledBytes - before.spilledBytes, after.readbackBytes - before.readbackBytes, after.maskedBytes - before.maskedBytes};
}

bool Resident(const GuestHeap& heap) {
    return AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + heap.Bytes()).resident != 0;
}

void RequireAccepted(const std::string& log, const std::string& what) {
    Require(log.find("BDA access failed") == std::string::npos && log.find("write-back failed") == std::string::npos && log.find("(APS5_HEAP_MIRROR_MIB)") == std::string::npos, "BDA store heap mirror: " + what + " was refused:\n" + log);
}

void RequireStored(const GuestHeap& heap, std::uint64_t offset, const std::string& what) {
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto value = heap.Load(offset + thread * sizeof(std::uint32_t));
        Require(value == Stored(thread), "BDA store heap mirror: " + what + ": the store of thread " + std::to_string(thread) + " reached guest memory as " + Hex(value) + ", expected " + Hex(Stored(thread)));
    }
}

void RequireCompared(const Compared& counted, std::uint64_t pages, std::uint64_t span, const std::string& what, bool resident) {
    Require(counted.compared == pages * Page && counted.span == span, "BDA store heap mirror: " + what + " compared " + std::to_string(counted.compared) + " of " + std::to_string(counted.span) + " span bytes, expected " + std::to_string(pages * Page) + " of " + std::to_string(span));
    Require(counted.copiedReference == 0 && counted.readReference == 0, "BDA store heap mirror: " + what + " took " + std::to_string(counted.copiedReference) + " reference bytes by a GPU copy and " + std::to_string(counted.readReference) + " by the CPU, expected none");
    if (resident) Require(counted.gathered + counted.spilled == pages * Page && counted.readback == 0 && counted.masked == 0, "BDA store heap mirror: " + what + " gathered " + std::to_string(counted.gathered) + " and spilled " + std::to_string(counted.spilled) + " bytes and read back " + std::to_string(counted.readback) + ", expected " + std::to_string(pages * Page) + " gathered or spilled and none read back");
    else Require(counted.masked == pages * Page && counted.gathered + counted.spilled + counted.readback == 0, "BDA store heap mirror: " + what + " wrote back " + std::to_string(counted.masked) + " bytes of a copy-backed heap mirror by their store masks, expected " + std::to_string(pages * Page));
}

void StoredPageTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    constexpr std::uint64_t Far = 20 * Block + 0x40;
    constexpr std::uint64_t Straddle = 6 * Block - 2 * Threads;
    constexpr std::uint64_t Tail = HeapBytes - Threads * sizeof(std::uint32_t);
    heap.Store(Straddle - sizeof(std::uint32_t), 0x12121212u);
    heap.Store(Tail - sizeof(std::uint32_t), 0x13131313u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    std::string log;
    const auto one = CountCompared([&] { log = Store(device, heap.Address() + StoreOffset, [&] { heap.Store(Far, 0x88888888u); }); });
    RequireAccepted(log, "a store into one block of a multi-block heap mirror");
    RequireStored(heap, StoreOffset, "a store into one block");
    Require(heap.Load(Far) == 0x88888888u, "BDA store heap mirror: the write-back rolled back a CPU store made into another block during the dispatch");
    const bool resident = Resident(heap);
    RequireCompared(one, 1, HeapBytes, "a store into one page", resident);
    const auto two = CountCompared([&] {
        log = Store(device, heap.Address() + Straddle, [&] {
            heap.Store(Far + sizeof(std::uint32_t), 0x89898989u);
            heap.Store(Straddle - sizeof(std::uint32_t), 0x16161616u);
        });
    });
    RequireAccepted(log, "a store across two blocks");
    RequireStored(heap, Straddle, "a store across two blocks");
    Require(heap.Load(Straddle - sizeof(std::uint32_t)) == 0x16161616u, "BDA store heap mirror: a store across two blocks rolled back a CPU store made during the dispatch into a page it stored to");
    Require(heap.Load(Far) == 0x88888888u && heap.Load(Far + sizeof(std::uint32_t)) == 0x89898989u, "BDA store heap mirror: a store across two blocks rolled back CPU stores into another block");
    RequireCompared(two, 2, HeapBytes, "a store across two blocks", resident);
    const auto tail = CountCompared([&] { log = Store(device, heap.Address() + Tail); });
    RequireAccepted(log, "a store ending at the heap's last dword");
    RequireStored(heap, Tail, "a store ending at the heap's last dword");
    Require(heap.Load(Tail - sizeof(std::uint32_t)) == 0x13131313u, "BDA store heap mirror: a store ending at the heap's last dword changed the dword before it");
    RequireCompared(tail, 1, HeapBytes, "a store ending at the heap's last dword", resident);
    RequireStored(heap, StoreOffset, "the first store after two more write-backs");
}

void SparseRunEndTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true, SparseRuns);
    constexpr std::uint64_t RunEnd = SparseRuns[1].second;
    constexpr std::uint64_t Ending = RunEnd - Threads * sizeof(std::uint32_t);
    heap.Store(Ending - sizeof(std::uint32_t), 0x14141414u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    std::string log;
    const auto counted = CountCompared([&] { log = Store(device, heap.Address() + Ending, [&] { heap.Store(LastRun + 2 * sizeof(std::uint32_t), 0x15151515u); }); });
    RequireAccepted(log, "a store ending at the last dword of a committed run of a sparse heap mirror");
    RequireStored(heap, Ending, "a store ending at the last dword of a committed run");
    Require(heap.Load(Ending - sizeof(std::uint32_t)) == 0x14141414u, "BDA store heap mirror: a store ending at a run's last dword changed the dword before it");
    Require(heap.Load(LastRun + 2 * sizeof(std::uint32_t)) == 0x15151515u, "BDA store heap mirror: a store ending at a run's last dword rolled back a CPU store into another run");
    std::uint64_t committed = 0;
    for (const auto& [begin, end] : SparseRuns) committed += end - begin;
    RequireCompared(counted, 1, committed, "a store ending at a run's last dword", Resident(heap));
}

void UnboundBlockTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    const OutputBlock output;
    constexpr std::uint64_t Unbound = 7 * Block + 0x300;
    heap.Store(Unbound - Block, 0x31313131u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    std::fill(output.Data(), output.Data() + Threads, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto address = AddressData(heap.Address() + Unbound);
    std::copy(address.begin(), address.end(), userData.begin());
    const auto out = BufferDescriptor(reinterpret_cast<std::uintptr_t>(output.Data()), static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    std::copy(out.begin(), out.end(), userData.begin() + 4);
    std::string log;
    const auto counted = CountCompared([&] { log = Run(device, StoreLoadCode, userData, [&] { heap.Store(Unbound + Threads * sizeof(std::uint32_t), 0x32323232u); }); });
    RequireAccepted(log, "a store into an unbound block of a heap mirror and a read of it in the same dispatch");
    const bool resident = Resident(heap);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(output.Data()[thread] == Stored(thread), "BDA store heap mirror: a read in the same dispatch of thread " + std::to_string(thread) + "'s store into an unbound block returned " + Hex(output.Data()[thread]) + ", expected " + Hex(Stored(thread)));
    }
    RequireStored(heap, Unbound, "a store into an unbound block");
    Require(heap.Load(Unbound + Threads * sizeof(std::uint32_t)) == 0x32323232u && heap.Load(Unbound - Block) == 0x31313131u, "BDA store heap mirror: the write-back of an unbound block rolled back the CPU's bytes");
    if (resident) {
        Require(counted.spilled == Page && counted.gathered == 0 && counted.readback == 0, "BDA store heap mirror: a store into an unbound block spilled " + std::to_string(counted.spilled) + " bytes and gathered " + std::to_string(counted.gathered) + ", expected one page spilled");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + HeapBytes).bytes == Block, "BDA store heap mirror: a store into an unbound block bound blocks of the heap mirror");
    }
    const auto created = device.ComputePipelinesCreated();
    RequireAccepted(Run(device, StoreLoadCode, userData), "a second store into an unbound block of a heap mirror");
    Require(device.ComputePipelinesCreated() == created, "BDA store heap mirror: a second dispatch storing into a heap mirror built " + std::to_string(device.ComputePipelinesCreated() - created) + " compute pipelines, expected the store page pipeline of the first");
    Copy(device, heap.Address() + Unbound, output);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(output.Data()[thread] == Stored(thread), "BDA store heap mirror: after the write-back the heap mirror holds " + Hex(output.Data()[thread]) + " for thread " + std::to_string(thread) + "'s store into a block that was unbound, expected " + Hex(Stored(thread)));
    }
}

void CpuByteTests(AgcDriver::VulkanDevice& device) {
    for (const bool bound : {true, false}) {
        GuestHeap heap(true);
        const std::uint64_t at = (bound ? 3 : 9) * Block + 0x700;
        const std::string where = bound ? "a bound block" : "an unbound block";
        if (bound) {
            for (std::uint64_t offset = 0; offset < Threads * sizeof(std::uint32_t); ++offset) heap.StoreByte(at + offset, 0x11);
        }
        const std::uint8_t fill = bound ? 0x11 : 0x00;
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        const auto log = Run(device, StoreByteCode, AddressData(heap.Address() + at), [&] {
            heap.StoreByte(at + 5 * sizeof(std::uint32_t) + 1, 0xab);
            heap.StoreByte(at + 9 * sizeof(std::uint32_t) + 3, 0xcd);
            heap.StoreByte(at + Threads * sizeof(std::uint32_t) + 2, 0xef);
        });
        RequireAccepted(log, "a byte store into " + where);
        for (std::uint32_t thread = 0; thread < Threads; ++thread) {
            const auto stored = heap.LoadByte(at + thread * sizeof(std::uint32_t));
            Require(stored == static_cast<std::uint8_t>(Stored(thread)), "BDA store heap mirror: the byte store of thread " + std::to_string(thread) + " into " + where + " reached guest memory as " + Hex(stored));
        }
        Require(heap.LoadByte(at + 5 * sizeof(std::uint32_t) + 1) == 0xab && heap.LoadByte(at + 9 * sizeof(std::uint32_t) + 3) == 0xcd, "BDA store heap mirror: the write-back of byte stores into " + where + " rolled back a CPU byte written during the dispatch into the same dword");
        Require(heap.LoadByte(at + Threads * sizeof(std::uint32_t) + 2) == 0xef, "BDA store heap mirror: the write-back of byte stores into " + where + " rolled back a CPU byte written during the dispatch into the same page");
        Require(heap.LoadByte(at + 5 * sizeof(std::uint32_t) + 2) == fill && heap.LoadByte(at + 9 * sizeof(std::uint32_t) + 1) == fill, "BDA store heap mirror: the write-back of byte stores into " + where + " changed bytes no store or CPU write touched");
    }
}

void ZeroStoreTests(AgcDriver::VulkanDevice& device) {
    constexpr std::uint64_t At = 11 * Block + 0x500;
    constexpr std::uint64_t Span = Threads * sizeof(std::uint32_t);
    for (const bool byte : {false, true}) {
        GuestHeap heap(true);
        const std::string what = std::string(byte ? "byte" : "dword") + " stores of zero into a zero block";
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        const auto code = byte ? std::span<const std::uint32_t>(ZeroByteStoreCode) : std::span<const std::uint32_t>(ZeroStoreCode);
        std::string log;
        const auto counted = CountCompared([&] {
            log = Run(device, code, AddressData(heap.Address() + At), [&] {
                for (std::uint64_t offset = 0; offset < Span + sizeof(std::uint32_t); ++offset) heap.StoreByte(At + offset, 0x77);
            });
        });
        RequireAccepted(log, what);
        const std::uint32_t expected = byte ? 0x77777700u : 0u;
        for (std::uint32_t thread = 0; thread < Threads; ++thread) {
            const auto value = heap.Load(At + thread * sizeof(std::uint32_t));
            Require(value == expected, "BDA store heap mirror: after " + what + " over bytes the CPU made non-zero during the dispatch, the dword of thread " + std::to_string(thread) + " reads " + Hex(value) + ", expected " + Hex(expected));
        }
        Require(heap.Load(At + Span) == 0x77777777u, "BDA store heap mirror: " + what + " changed the CPU's dword after them");
        if (Resident(heap)) Require(counted.spilled == Page && counted.gathered == 0 && counted.readback == 0, "BDA store heap mirror: " + what + " spilled " + std::to_string(counted.spilled) + " bytes and gathered " + std::to_string(counted.gathered) + ", expected one page spilled");
        else Require(counted.masked == Page, "BDA store heap mirror: " + what + " wrote back " + std::to_string(counted.masked) + " bytes by store masks, expected one page");
    }
}

void StorePageTests(AgcDriver::VulkanDevice& device) {
    const auto slots = AgcDriver::Graphics::BdaResources::StorePageSlots();
    Require(slots == Threads, "BDA store heap mirror: the store page test needs " + std::to_string(Threads) + " store pages (APS5_BDA_STORE_PAGES), the driver has " + std::to_string(slots));
    constexpr std::uint64_t First = 16 * Block;
    const auto run = [&](const GuestHeap& heap, std::uint64_t second) {
        auto userData = AddressData(heap.Address() + First);
        const auto other = AddressData(heap.Address() + First + second);
        userData.insert(userData.end(), other.begin(), other.end());
        return Run(device, PagesCode, userData);
    };
    {
        GuestHeap heap(true);
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        RequireAccepted(run(heap, 0), "a dispatch storing into as many pages of a heap mirror as there are store pages");
        for (std::uint32_t thread = 0; thread < Threads; ++thread) {
            const auto value = Marker | (thread << 12u);
            Require(heap.Load(First + thread * Page) == value && heap.Load(First + thread * sizeof(std::uint32_t)) == value, "BDA store heap mirror: a dispatch storing into " + std::to_string(slots) + " pages lost the stores of thread " + std::to_string(thread) + " (" + Hex(heap.Load(First + thread * Page)) + " and " + Hex(heap.Load(First + thread * sizeof(std::uint32_t))) + ")");
        }
    }
    GuestHeap heap(true);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    run(heap, Threads * Page);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(heap.Load(First + thread * Page) == 0, "BDA store heap mirror: a dispatch storing into " + std::to_string(slots + 1) + " pages of a heap mirror, more than the store pages hold, stored thread " + std::to_string(thread) + "'s value into its own page");
        Require(heap.Load(First + Threads * Page + thread * sizeof(std::uint32_t)) == 0, "BDA store heap mirror: a dispatch storing into " + std::to_string(slots + 1) + " pages of a heap mirror, more than the store pages hold, stored thread " + std::to_string(thread) + "'s value into the last page");
    }
}

constexpr std::uint32_t DefaultStorePages = 1024;

void DefaultStorePageTests(AgcDriver::VulkanDevice& device) {
    const auto slots = AgcDriver::Graphics::BdaResources::StorePageSlots();
    Require(slots == DefaultStorePages, "BDA store heap mirror: the default store page test needs the default " + std::to_string(DefaultStorePages) + " store pages (APS5_BDA_STORE_PAGES unset), the driver has " + std::to_string(slots));
    constexpr std::uint64_t First = Block;
    constexpr std::uint64_t Pages = DefaultStorePages + 1;
    constexpr std::uint64_t Last = First + (Pages - 1) * Page;
    GuestHeap heap(true, {}, (Last + Page + Block - 1) / Block * Block);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    auto userData = AddressData(heap.Address() + First);
    const auto other = AddressData(heap.Address() + Last);
    userData.insert(userData.end(), other.begin(), other.end());
    Run(device, PagesCode, userData, {}, DefaultStorePages);
    for (std::uint64_t page = 0; page + 1 < Pages; ++page) {
        Require(heap.Load(First + page * Page) == 0, "BDA store heap mirror: the refused dispatch's store into page " + std::to_string(page) + " reached guest memory as " + Hex(heap.Load(First + page * Page)));
    }
    for (std::uint32_t thread = 0; thread < DefaultStorePages; ++thread) {
        Require(heap.Load(Last + thread * sizeof(std::uint32_t)) == 0, "BDA store heap mirror: the refused dispatch's store of thread " + std::to_string(thread) + " into its last page reached guest memory");
    }
}

void TwoDispatchTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    constexpr std::uint64_t First = 2 * Block + 0x100;
    constexpr std::uint64_t Second = First + 2 * Threads * sizeof(std::uint32_t);
    constexpr std::uint64_t Overwritten = First + 5 * sizeof(std::uint32_t);
    heap.Store(First - sizeof(std::uint32_t), 0x41414141u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto first = Compile(device, StoreCode, AddressData(heap.Address() + First));
    const auto second = Compile(device, StoreCode, AddressData(heap.Address() + Second));
    StderrCapture capture;
    std::string thrown;
    try {
        device.Dispatch(first, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
        heap.Store(Second - sizeof(std::uint32_t), 0x42424242u);
        device.Dispatch(second, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
        RequireStored(heap, First, "the first of two storing dispatches with no wait, before the second's write-back");
        heap.Store(Overwritten, 0x43434343u);
        heap.Store(Second + Threads * sizeof(std::uint32_t), 0x44444444u);
        device.WaitIdle();
    } catch (const std::exception& error) {
        thrown = error.what();
    }
    RequireAccepted(capture.Finish() + thrown, "two storing dispatches into one page with no wait between them");
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        if (First + thread * sizeof(std::uint32_t) == Overwritten) continue;
        Require(heap.Load(First + thread * sizeof(std::uint32_t)) == Stored(thread), "BDA store heap mirror: the first of two storing dispatches lost the store of thread " + std::to_string(thread));
    }
    RequireStored(heap, Second, "the second of two storing dispatches into one page with no wait");
    Require(heap.Load(First - sizeof(std::uint32_t)) == 0x41414141u && heap.Load(Second - sizeof(std::uint32_t)) == 0x42424242u, "BDA store heap mirror: two storing dispatches rolled back the CPU's bytes written before them");
    Require(heap.Load(Overwritten) == 0x43434343u, "BDA store heap mirror: the second dispatch's write-back rolled back a CPU store, made after the first dispatch's write-back, over a dword the first dispatch stored (it holds " + Hex(heap.Load(Overwritten)) + ")");
    Require(heap.Load(Second + Threads * sizeof(std::uint32_t)) == 0x44444444u, "BDA store heap mirror: the second dispatch's write-back rolled back a CPU store made beside its stores between the two write-backs");
}

void ChainedDispatchTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    const OutputBlock output;
    constexpr std::uint64_t Unbound = 13 * Block + 0x500;
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    std::fill(output.Data(), output.Data() + Threads, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto address = AddressData(heap.Address() + Unbound);
    std::copy(address.begin(), address.end(), userData.begin());
    const auto out = BufferDescriptor(reinterpret_cast<std::uintptr_t>(output.Data()), static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    std::copy(out.begin(), out.end(), userData.begin() + 4);
    const auto store = Compile(device, StoreCode, address);
    const auto load = Compile(device, LoadCode, userData);
    StderrCapture capture;
    std::string thrown;
    try {
        device.Dispatch(store, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
        device.Dispatch(load, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadCode.data()));
        device.WaitIdle();
    } catch (const std::exception& error) {
        thrown = error.what();
    }
    RequireAccepted(capture.Finish() + thrown, "a dispatch reading what the dispatch before it stored into an unbound block, with no wait between them");
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(output.Data()[thread] == Stored(thread), "BDA store heap mirror: a dispatch read " + Hex(output.Data()[thread]) + " where the dispatch before it, with no wait between them, stored " + Hex(Stored(thread)) + " for thread " + std::to_string(thread) + " into a block that was unbound");
    }
    RequireStored(heap, Unbound, "a store into an unbound block read by the next dispatch");
}

void BudgetTests(AgcDriver::VulkanDevice& device, bool resident) {
    const std::uint64_t bytes = resident ? std::uint64_t{2} << 30u : std::uint64_t{48} << 20u;
    const std::uint64_t bound = resident ? (std::uint64_t{3} << 29u) + 0x200 : (std::uint64_t{40} << 20u) + 0x200;
    const std::uint64_t unbound = resident ? (std::uint64_t{1} << 30u) + 5 * Block + 0x400 : (std::uint64_t{20} << 20u) + 5 * Block + 0x400;
    const std::string what = resident ? "a resident 2 GiB heap mirror" : "a copy-backed 48 MiB heap mirror";
    const AddressSpaceLimit limit(resident ? 0 : 1);
    GuestHeap heap(true, {}, bytes);
    heap.Store(bound - sizeof(std::uint32_t), 0x51515151u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    std::string log;
    const auto first = CountCompared([&] { log = Store(device, heap.Address() + bound, [&] { heap.Store(bound + Threads * sizeof(std::uint32_t), 0x52525252u); }); });
    RequireAccepted(log, "a store into a block holding data of " + what);
    RequireStored(heap, bound, "a store into a block holding data of " + what);
    Require(heap.Load(bound - sizeof(std::uint32_t)) == 0x51515151u && heap.Load(bound + Threads * sizeof(std::uint32_t)) == 0x52525252u, "BDA store heap mirror: a store into " + what + " rolled back the CPU's bytes");
    const auto second = CountCompared([&] { log = Store(device, heap.Address() + unbound); });
    RequireAccepted(log, "a store into a zero block of " + what);
    RequireStored(heap, unbound, "a store into a zero block of " + what);
    const auto held = AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + bytes);
    Require(held.mirrors == 1, "BDA store heap mirror: " + what + " was not heap mirrored");
    Require((held.resident != 0) == resident, "BDA store heap mirror: " + what + " was mirrored " + (held.resident != 0 ? "resident" : "copy-backed"));
    if (resident) Require(held.bytes <= 2 * Block, "BDA store heap mirror: storing into " + what + " bound " + std::to_string(held.bytes) + " bytes of it");
    for (const auto& [counted, block] : {std::pair{first, "a block holding data"}, std::pair{second, "a zero block"}}) {
        const std::string where = std::string(block) + " of " + what;
        Require(counted.span == bytes && counted.compared == Page, "BDA store heap mirror: a store into " + where + " compared " + std::to_string(counted.compared) + " bytes of a " + std::to_string(counted.span) + "-byte span");
        Require(counted.copiedReference == 0 && counted.readReference == 0 && counted.readback == 0, "BDA store heap mirror: a store into " + where + " took " + std::to_string(counted.copiedReference + counted.readReference) + " reference bytes and read back " + std::to_string(counted.readback));
        Require((resident ? counted.gathered + counted.spilled : counted.masked) == Page, "BDA store heap mirror: a store into " + where + " gathered " + std::to_string(counted.gathered) + ", spilled " + std::to_string(counted.spilled) + " and wrote back " + std::to_string(counted.masked) + " bytes by store masks");
    }
    if (resident) Require(first.gathered == Page && second.spilled == Page, "BDA store heap mirror: " + what + "'s store into a bound block was not gathered or its store into an unbound block not spilled");
}

void ReadOnlyTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(false);
    heap.Store(StoreOffset, 0x33333333u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto log = Store(device, heap.Address() + StoreOffset);
    Require(log.find("read-only in the BDA table") != std::string::npos, "BDA store heap mirror: a store into a read-only heap mirror was not refused:\n" + log);
    Require(heap.Load(StoreOffset) == 0x33333333u, "BDA store heap mirror: a refused store changed the read-only heap");
}

}

int main(int argc, char** argv) {
    try {
        const bool budget = argc > 1 && std::string_view(argv[1]) == "--budget";
        const bool storePages = argc > 1 && std::string_view(argv[1]) == "--store-pages";
        const bool storePagesDefault = argc > 1 && std::string_view(argv[1]) == "--store-pages-default";
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
#ifdef _WIN32
        const bool watched = GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
        const bool watched = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
#endif
        if (!watched) {
            std::puts("skipped, no guest write watch: heap mirrors need one");
            return VulkanTestSkipped;
        }
        if (budget) {
            const auto context = device->graphicsContext();
            BudgetTests(*device, false);
            if (context.sparseBinding && context.sparseResidencyBuffer && context.sparseResidencyAliased) BudgetTests(*device, true);
            else std::puts("skipped the resident 2 GiB heap: the device has no sparse residency");
            std::puts("BDA store heap mirror budget tests passed");
            return 0;
        }
        if (storePages) {
            for (const bool copyBacked : {false, true}) {
                const AddressSpaceLimit limit(copyBacked ? 1 : 0);
                StorePageTests(*device);
            }
            std::puts("BDA store heap mirror store page tests passed");
            return 0;
        }
        if (storePagesDefault) {
            for (const bool copyBacked : {false, true}) {
                const AddressSpaceLimit limit(copyBacked ? 1 : 0);
                DefaultStorePageTests(*device);
            }
            std::puts("BDA store heap mirror default store page tests passed");
            return 0;
        }
        for (const bool copyBacked : {false, true}) {
            const AddressSpaceLimit limit(copyBacked ? 1 : 0);
            if (copyBacked) std::puts("copy-backed heap mirrors:");
            WritableTests(*device);
            MixedTests(*device);
            SparseTests(*device);
            LabelTests(*device);
            StoredPageTests(*device);
            SparseRunEndTests(*device);
            UnboundBlockTests(*device);
            CpuByteTests(*device);
            ZeroStoreTests(*device);
            TwoDispatchTests(*device);
            ChainedDispatchTests(*device);
        }
        ReadOnlyTests(*device);
        std::puts("BDA store heap mirror tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
