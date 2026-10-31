#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
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
#include <span>
#include <string>
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
constexpr std::array<std::pair<std::uint64_t, std::uint64_t>, 3> SparseRuns{{{0, 2 * Block}, {4 * Block, 8 * Block}, {LastRun, LastRun + 2 * Block}}};

alignas(256) constexpr std::array<std::uint32_t, 6> StoreCode{0x34020082, 0x380402ff, Marker, 0xdc708000, 0x00000201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 8> MixedCode{0x34020082, 0x380402ff, Marker, 0xdc708000, 0x00000201, 0xe0701000, 0x80010201, 0xbf810000};

alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{0x34060082, 0xe0301000, 0x80000403, 0xbf8c3f70, 0xe0701000, 0x80010403, 0xbf810000};

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
    explicit GuestHeap(bool writable, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs = {}) {
#ifdef _WIN32
        raw = GuestArena::GuestArenaAllocate_nid_postfix(HeapBytes, Block);
        Require(raw != nullptr, "BDA store heap mirror: cannot allocate the guest heap");
        base = static_cast<std::uint8_t*>(raw);
        if (runs.empty()) GuestArena::GuestArenaCommit_nid_postfix(raw, HeapBytes, PAGE_READWRITE, HeapBytes);
        for (const auto& [begin, end] : runs) GuestArena::GuestArenaCommit_nid_postfix(base + begin, end - begin, PAGE_READWRITE, Block);
#else
        raw = mmap(nullptr, HeapBytes + Block, runs.empty() ? PROT_READ | PROT_WRITE : PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        Require(raw != MAP_FAILED, "BDA store heap mirror: cannot map the guest heap");
        base = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(raw) + Block - 1) / Block * Block);
        for (const auto& [begin, end] : runs) Require(mprotect(base + begin, end - begin, PROT_READ | PROT_WRITE) == 0, "BDA store heap mirror: cannot commit a run of the sparse guest heap");
#endif
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, HeapBytes);
        GuestAllocations::Mutation().Add(base, HeapBytes, true, writable, true);
    }

    ~GuestHeap() {
        GuestAllocations::Mutation().Remove(base);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, HeapBytes);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(raw, HeapBytes);
        GuestArena::GuestArenaRelease_nid_postfix(raw, HeapBytes);
#else
        munmap(raw, HeapBytes + Block);
#endif
    }

    GuestHeap(const GuestHeap&) = delete;
    GuestHeap& operator=(const GuestHeap&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(base); }

    void Store(std::uint64_t offset, std::uint32_t value) const { std::memcpy(base + offset, &value, sizeof(value)); }

    std::uint32_t Load(std::uint64_t offset) const {
        std::uint32_t value = 0;
        std::memcpy(&value, base + offset, sizeof(value));
        return value;
    }

private:
    void* raw = nullptr;
    std::uint8_t* base = nullptr;
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

std::string Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const std::function<void()>& during = {}) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, regions},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
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

void ReadOnlyTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(false);
    heap.Store(StoreOffset, 0x33333333u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto log = Store(device, heap.Address() + StoreOffset);
    Require(log.find("read-only in the BDA table") != std::string::npos, "BDA store heap mirror: a store into a read-only heap mirror was not refused:\n" + log);
    Require(heap.Load(StoreOffset) == 0x33333333u, "BDA store heap mirror: a refused store changed the read-only heap");
}

}

int main() {
    try {
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
        WritableTests(*device);
        MixedTests(*device);
        SparseTests(*device);
        ReadOnlyTests(*device);
        std::puts("BDA store heap mirror tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
