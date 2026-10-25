#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint64_t Block = 65536;
constexpr std::uint64_t HeapBytes = 8u << 20u;
constexpr std::uint64_t JoinedBytes = 16u << 20u;
constexpr std::uint64_t ThreadStride = 16384;
constexpr std::uint64_t StoreBlock = 3 * Block;
constexpr std::uint32_t LateThread = 5;
constexpr std::uint64_t ZeroedBlock = 2 * Block;

alignas(256) constexpr std::array<std::uint32_t, 10> Code{
    0x3404008e, 0xdc388000, 0x04000002, 0x34060082, 0xbf8c3f70, 0xe0701000, 0x80010403, 0xe0701000, 0x80020303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{0x34060082, 0xe0301000, 0x80000403, 0xbf8c3f70, 0xe0701000, 0x80010403, 0xbf810000};

class OutputBlock {
public:
    OutputBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, Block, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(Block, Block));
#endif
        Require(block != nullptr, "resident heap mirror: cannot allocate the output block");
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
    explicit GuestHeap(bool writable, std::uint64_t ranges = 1, std::uint64_t rangeBytes = HeapBytes, void* at = nullptr) : ranges(ranges), rangeBytes(rangeBytes) {
        const auto bytes = rangeBytes * ranges;
#ifdef _WIN32
        raw = at == nullptr ? GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block)
                            : GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(reinterpret_cast<std::uintptr_t>(at), bytes, Block);
        Require(raw != nullptr && (at == nullptr || raw == at), "resident heap mirror: cannot allocate the guest heap");
        GuestArena::GuestArenaCommit_nid_postfix(raw, bytes, PAGE_READWRITE, bytes);
        base = static_cast<std::uint8_t*>(raw);
#else
        raw = mmap(at, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | (at != nullptr ? MAP_FIXED_NOREPLACE : 0), -1, 0);
        Require(raw != MAP_FAILED && (at == nullptr || raw == at), "resident heap mirror: cannot map the guest heap");
        base = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(raw) + Block - 1) / Block * Block);
#endif
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, bytes);
        GuestAllocations::Mutation mutation;
        for (std::uint64_t range = 0; range < ranges; ++range) mutation.Add(base + range * rangeBytes, rangeBytes, true, writable, true);
    }

    ~GuestHeap() {
        {
            GuestAllocations::Mutation mutation;
            for (std::uint64_t range = 0; range < ranges; ++range) mutation.Remove(base + range * rangeBytes);
        }
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, rangeBytes * ranges);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(raw, rangeBytes * ranges);
        GuestArena::GuestArenaRelease_nid_postfix(raw, rangeBytes * ranges);
#else
        munmap(raw, rangeBytes * ranges + Block);
#endif
    }

    GuestHeap(const GuestHeap&) = delete;
    GuestHeap& operator=(const GuestHeap&) = delete;

    std::uint8_t* Data() const { return base; }
    void* Raw() const { return raw; }
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(base); }

    void Store(std::uint64_t offset, std::uint32_t value) const { std::memcpy(base + offset, &value, sizeof(value)); }

    std::uint32_t Load(std::uint64_t offset) const {
        std::uint32_t value = 0;
        std::memcpy(&value, base + offset, sizeof(value));
        return value;
    }

private:
    std::uint64_t ranges = 1;
    std::uint64_t rangeBytes = HeapBytes;
    void* raw = nullptr;
    std::uint8_t* base = nullptr;
};

struct ImportBudget {
    explicit ImportBudget(std::uint64_t bytes) { AgcDriver::Graphics::SetHostImportBudget(bytes); }
    ~ImportBudget() { AgcDriver::Graphics::SetHostImportBudget(0); }
    ImportBudget(const ImportBudget&) = delete;
    ImportBudget& operator=(const ImportBudget&) = delete;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::uint32_t Pattern(std::uint32_t thread) {
    return 0x5a000000u + thread * 0x00010203u + 1u;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
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
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint64_t read, std::uint64_t written, const OutputBlock& output) {
    std::fill(output.Data(), output.Data() + Threads, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = static_cast<std::uint32_t>(read);
    userData[1] = static_cast<std::uint32_t>(read >> 32u);
    const auto out = BufferDescriptor(reinterpret_cast<std::uintptr_t>(output.Data()), static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    const auto store = BufferDescriptor(written + StoreBlock, static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    std::copy(out.begin(), out.end(), userData.begin() + 4);
    std::copy(store.begin(), store.end(), userData.begin() + 8);
    Run(device, Code, userData);
}

void Dispatch(AgcDriver::VulkanDevice& device, const GuestHeap& read, const GuestHeap& written, const OutputBlock& output) {
    Dispatch(device, read.Address(), written.Address(), output);
}

void Copy(AgcDriver::VulkanDevice& device, std::uint64_t source, const OutputBlock& output) {
    std::fill(output.Data(), output.Data() + Threads, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto in = BufferDescriptor(source, static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    const auto out = BufferDescriptor(reinterpret_cast<std::uintptr_t>(output.Data()), static_cast<std::uint32_t>(Threads * sizeof(std::uint32_t)));
    std::copy(in.begin(), in.end(), userData.begin());
    std::copy(out.begin(), out.end(), userData.begin() + 4);
    Run(device, CopyCode, userData);
}

void CheckReads(const OutputBlock& output, const std::string& step, bool late) {
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto expected = thread * ThreadStride < Block ? Pattern(thread) : late && thread == LateThread ? Pattern(thread) : 0u;
        Require(output.Data()[thread] == expected, "resident heap mirror: " + step + ": thread " + std::to_string(thread) + " read " + Hex(output.Data()[thread]) + " at heap offset " + Hex(thread * ThreadStride) + ", expected " + Hex(expected));
    }
}

void CheckStores(const GuestHeap& written, const std::string& step) {
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto value = written.Load(StoreBlock + thread * sizeof(std::uint32_t));
        Require(value == thread * 4u, "resident heap mirror: " + step + ": the GPU store of thread " + std::to_string(thread) + " into a never-written heap block reached the guest as " + Hex(value) + ", expected " + Hex(thread * 4u));
    }
}

void CheckHeld(std::uint64_t before, std::uint64_t written, const std::string& step) {
    const auto held = AgcDriver::Graphics::MirrorCounters().heapBytes - before;
    Require(held == written, "resident heap mirror: " + step + ": the heap mirrors of two " + std::to_string(HeapBytes >> 10u) + " KiB heaps hold " + std::to_string(held >> 10u) + " KiB of device memory; the guest and the GPU wrote " + std::to_string(written >> 10u) + " KiB of them");
}

void ResidentTests(AgcDriver::VulkanDevice& device) {
    GuestHeap read(false);
    GuestHeap written(true);
    const OutputBlock output;
    for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) read.Store(thread * ThreadStride, Pattern(thread));
    written.Store(16, 0x77u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto before = AgcDriver::Graphics::MirrorCounters();
    Dispatch(device, read, written, output);
    const auto made = AgcDriver::Graphics::MirrorCounters();
    Require(made.heapMirrors == before.heapMirrors + 2, "resident heap mirror: the two heaps were not heap mirrored (" + std::to_string(made.heapMirrors - before.heapMirrors) + " mirrors made)");
    CheckHeld(before.heapBytes, 3 * Block, "first dispatch");
    CheckReads(output, "first dispatch", false);
    CheckStores(written, "first dispatch");
    Require(written.Load(16) == 0x77u, "resident heap mirror: the written heap's guest bytes changed");
    read.Store(LateThread * ThreadStride, Pattern(LateThread));
    std::memset(read.Data() + ZeroedBlock, 0, Block);
    Dispatch(device, read, written, output);
    CheckHeld(before.heapBytes, 4 * Block, "after a guest write into a never-written block and zeros written into another");
    CheckReads(output, "after a guest write into a never-written block", true);
    const auto steady = AgcDriver::Graphics::MirrorCounters();
    Dispatch(device, read, written, output);
    const auto again = AgcDriver::Graphics::MirrorCounters();
    Require(again.rebuilds == steady.rebuilds && again.heapBytes == steady.heapBytes, "resident heap mirror: an unchanged dispatch rebuilt or grew the heap mirrors");
    CheckReads(output, "unchanged dispatch", true);
    CheckStores(written, "unchanged dispatch");
}

struct AddressSpaceLimit {
    explicit AddressSpaceLimit(std::uint64_t bytes) { AgcDriver::Graphics::SetResidentAddressSpaceLimit(bytes); }
    ~AddressSpaceLimit() { AgcDriver::Graphics::SetResidentAddressSpaceLimit(0); }
    AddressSpaceLimit(const AddressSpaceLimit&) = delete;
    AddressSpaceLimit& operator=(const AddressSpaceLimit&) = delete;
};

void CopiedTests(AgcDriver::VulkanDevice& device) {
    const AddressSpaceLimit limit(1);
    GuestHeap read(false);
    GuestHeap written(true);
    const OutputBlock output;
    for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) read.Store(thread * ThreadStride, Pattern(thread));
    written.Store(16, 0x77u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto before = AgcDriver::Graphics::MirrorCounters();
    Dispatch(device, read, written, output);
    const auto made = AgcDriver::Graphics::MirrorCounters();
    Require(made.heapMirrors == before.heapMirrors + 2, "copied heap mirror: the two heaps were not heap mirrored without sparse address space (" + std::to_string(made.heapMirrors - before.heapMirrors) + " mirrors made)");
    Require(made.heapBytes - before.heapBytes == 2 * HeapBytes, "copied heap mirror: two " + std::to_string(HeapBytes >> 10u) + " KiB heaps without sparse address space hold " + std::to_string((made.heapBytes - before.heapBytes) >> 10u) + " KiB, expected whole copies");
    CheckReads(output, "copied, first dispatch", false);
    CheckStores(written, "copied, first dispatch");
    Require(written.Load(16) == 0x77u, "copied heap mirror: the written heap's guest bytes changed");
    read.Store(LateThread * ThreadStride, Pattern(LateThread));
    Dispatch(device, read, written, output);
    CheckReads(output, "copied, after a guest write", true);
    CheckStores(written, "copied, after a guest write");
}

struct ImportWindow {
    explicit ImportWindow(std::uint64_t bytes) { AgcDriver::Graphics::SetHostImportRangeWindow(bytes); }
    ~ImportWindow() { AgcDriver::Graphics::SetHostImportRangeWindow(std::uint64_t{64} << 20u); }
    ImportWindow(const ImportWindow&) = delete;
    ImportWindow& operator=(const ImportWindow&) = delete;
};

void PartialImportTests(AgcDriver::VulkanDevice& device, bool copied, bool readFirst) {
    const AddressSpaceLimit limit(copied ? 1 : 0);
    const ImportWindow window(2 * Block);
    GuestHeap first(!readFirst);
    GuestHeap second(readFirst);
    const auto& read = readFirst ? first : second;
    const auto& written = readFirst ? second : first;
    const OutputBlock output;
    const auto tail = HeapBytes - Block;
    for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) read.Store(thread * ThreadStride, Pattern(thread));
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        read.Store(tail + thread * sizeof(std::uint32_t), Pattern(thread));
        written.Store(tail + thread * sizeof(std::uint32_t), Pattern(thread));
    }
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + 3 * Block);
    Dispatch(device, read, written, output);
    CheckReads(output, "partial heap import", false);
    const auto& selected = read.Address() < written.Address() ? read : written;
    const auto* prefix = AgcDriver::Graphics::ExistingHostImportFor(device.graphicsContext(), selected.Address(), 2 * Block);
    Require(prefix != nullptr && prefix->base == selected.Address() && prefix->bytes == 2 * Block, "partial heap import: the first registered heap has no exact two-block imported prefix");
    const auto held = AgcDriver::Graphics::HeapMirrorsWithin(selected.Address(), selected.Address() + HeapBytes);
    const auto expected = copied ? HeapBytes - prefix->bytes : (selected.Address() == read.Address() ? Block : 2 * Block);
    Require(held.mirrors == 1 && held.bytes == expected, "partial heap import: its mirror holds " + std::to_string(held.bytes) + " bytes, expected " + std::to_string(expected));
    Copy(device, selected.Address() + tail, output);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) Require(output.Data()[thread] == Pattern(thread), "partial heap import: GPU read of the mirrored tail lost its contents");
    selected.Store(tail + 2 * sizeof(std::uint32_t), 0x12345678u);
    Copy(device, selected.Address() + tail, output);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) Require(output.Data()[thread] == (thread == 2 ? 0x12345678u : Pattern(thread)), "partial heap import: refresh lost a CPU write or its unchanged neighbors");
    const auto refreshed = AgcDriver::Graphics::HeapMirrorsWithin(selected.Address(), selected.Address() + HeapBytes);
    Require(refreshed.mirrors == 1 && refreshed.bytes == expected, "partial heap import: refreshing the selected tail changed its accounted extent");
}

void ReusedAddressTests(AgcDriver::VulkanDevice& device) {
    GuestHeap written(true);
    const OutputBlock output;
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    void* at = nullptr;
    std::uint64_t address = 0;
    {
        GuestHeap first(false);
        for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) first.Store(thread * ThreadStride, Pattern(thread));
        Dispatch(device, first, written, output);
        CheckReads(output, "the first heap", false);
        at = first.Raw();
        address = first.Address();
    }
    GuestHeap second(false, 1, HeapBytes, at);
    Require(second.Address() == address, "resident heap mirror: the second heap did not take the first heap's address");
    for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) second.Store(thread * ThreadStride, ~Pattern(thread));
    Dispatch(device, second, written, output);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto expected = thread * ThreadStride < Block ? ~Pattern(thread) : 0u;
        Require(output.Data()[thread] == expected, "resident heap mirror: a heap mapped where a mirrored heap was freed: thread " + std::to_string(thread) + " read " + Hex(output.Data()[thread]) + ", expected " + Hex(expected));
    }
    const auto held = AgcDriver::Graphics::HeapMirrorsWithin(address, address + HeapBytes);
    Require(held.mirrors == 1 && held.bytes == Block, "resident heap mirror: a heap mapped where a mirrored heap was freed holds " + std::to_string(held.mirrors) + " live heap mirrors (" + std::to_string(held.bytes >> 10u) + " KiB), expected 1 (64 KiB)");
}

void JoinedImportTests(AgcDriver::VulkanDevice& device) {
    GuestHeap pair(true, 2, JoinedBytes);
    const OutputBlock output;
    const auto begin = pair.Address();
    const auto end = begin + 2 * JoinedBytes;
    for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) pair.Store(thread * ThreadStride, Pattern(thread));
    pair.Store(JoinedBytes - sizeof(std::uint32_t), 0x41u);
    pair.Store(JoinedBytes, 0x42u);
    const auto before = AgcDriver::Graphics::HeapMirrorsWithin(begin, end);
    {
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        Dispatch(device, begin, begin + JoinedBytes, output);
        CheckReads(output, "two adjacent heaps", false);
    }
    const auto mirrored = AgcDriver::Graphics::HeapMirrorsWithin(begin, end);
    Require(before.mirrors == 0 && mirrored.mirrors == 2 && mirrored.bytes == 4 * Block, "resident heap mirror: two adjacent heaps hold " + std::to_string(before.mirrors) + " heap mirrors before the first dispatch and " + std::to_string(mirrored.mirrors) + " (" + std::to_string(mirrored.bytes >> 10u) + " KiB) after it, expected 0 and 2 (256 KiB); " + std::to_string(AgcDriver::Graphics::HostImportLiveBytes() >> 10u) + " KiB host imports live");
    const auto live = AgcDriver::Graphics::HostImportLiveBytes();
    Copy(device, begin + JoinedBytes - Threads * sizeof(std::uint32_t) / 2, output);
    Require(output.Data()[Threads / 2 - 1] == 0x41u && output.Data()[Threads / 2] == 0x42u, "resident heap mirror: a view across two adjacent heaps read " + Hex(output.Data()[Threads / 2 - 1]) + " " + Hex(output.Data()[Threads / 2]) + ", expected 0x41 0x42");
    const auto joined = AgcDriver::Graphics::HostImportLiveBytes() - live;
    Require(joined >= 2 * JoinedBytes, "resident heap mirror: a view across two adjacent heaps made no joined host import (" + std::to_string(joined >> 10u) + " KiB imported)");
    Dispatch(device, begin, begin + JoinedBytes, output);
    CheckReads(output, "after the joined import", false);
    const auto released = AgcDriver::Graphics::HeapMirrorsWithin(begin, end);
    Require(released.mirrors == 0 && released.bytes == 0, "resident heap mirror: a joined host import over two heap-mirrored heaps left " + std::to_string(released.mirrors) + " of their " + std::to_string(mirrored.mirrors) + " heap mirrors and " + std::to_string(released.bytes >> 10u) + " of " + std::to_string(mirrored.bytes >> 10u) + " KiB held");
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
        ResidentTests(*device);
        ReusedAddressTests(*device);
        JoinedImportTests(*device);
        CopiedTests(*device);
        for (const bool copied : {false, true}) {
            for (const bool readFirst : {false, true}) PartialImportTests(*device, copied, readFirst);
        }
        std::puts("resident heap mirror tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
