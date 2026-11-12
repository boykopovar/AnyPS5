#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

bool ImportsEnabled = true;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 64;
constexpr std::uint32_t Bytes = Words * 4u;
constexpr std::uint64_t Block = 65536;
constexpr std::uint64_t HeapBytes = 8u << 20u;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;

alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{0x34060082, 0xe0301000, 0x80000403, 0xbf8c3f70, 0xe0701000, 0x80010403, 0xbf810000};

class OutputBlock {
public:
    explicit OutputBlock(std::uint64_t bytes = Block) : bytes(bytes) {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(Block, bytes));
#endif
        Require(block != nullptr, "sparse heap import: cannot allocate the output block");
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
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
    std::uint64_t bytes = 0;
    std::uint32_t* block = nullptr;
};

class SparseHeap {
public:
    SparseHeap(bool writable, std::uint64_t committed) : writable(writable), committed(committed) {
#ifdef _WIN32
        base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(HeapBytes, Block));
        Require(base != nullptr, "sparse heap import: cannot reserve the guest heap");
        GuestArena::GuestArenaCommit_nid_postfix(base, committed, PAGE_READWRITE, Block);
#else
        base = static_cast<std::uint8_t*>(mmap(nullptr, HeapBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
        Require(base != MAP_FAILED, "sparse heap import: cannot reserve the guest heap");
        Require(mprotect(base, committed, PROT_READ | PROT_WRITE) == 0, "sparse heap import: cannot commit the guest heap");
#endif
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(base, HeapBytes);
        GuestAllocations::Mutation().Add(base, committed, true, writable, true);
    }

    ~SparseHeap() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(base);
        }
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(base, HeapBytes);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(base, HeapBytes);
        GuestArena::GuestArenaRelease_nid_postfix(base, HeapBytes);
#else
        munmap(base, HeapBytes);
#endif
    }

    SparseHeap(const SparseHeap&) = delete;
    SparseHeap& operator=(const SparseHeap&) = delete;

    void Commit(std::uint64_t bytes) {
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(base + committed, bytes - committed, PAGE_READWRITE, Block);
#else
        Require(mprotect(base + committed, bytes - committed, PROT_READ | PROT_WRITE) == 0, "sparse heap import: cannot grow the guest heap");
#endif
        GuestAllocations::Mutation mutation;
        mutation.Remove(base);
        mutation.Add(base, bytes, true, writable, true);
        committed = bytes;
    }

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(base); }
    std::uint64_t Committed() const { return committed; }
    void Store(std::uint64_t offset, std::uint32_t value) const { std::memcpy(base + offset, &value, sizeof(value)); }
    std::uint32_t Load(std::uint64_t offset) const {
        std::uint32_t value = 0;
        std::memcpy(&value, base + offset, sizeof(value));
        return value;
    }

private:
    std::uint8_t* base = nullptr;
    bool writable = false;
    std::uint64_t committed = 0;
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

void Fill(std::uint32_t* where, std::uint32_t first) {
    for (std::uint32_t index = 0; index < Words; ++index) where[index] = first + index;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device) {
    const auto input = BufferDescriptor(0x10000, Bytes);
    const auto output = BufferDescriptor(0x20000, Bytes);
    std::vector<std::uint32_t> userData(8, 0u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(CopyCode);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled, std::uint64_t source, std::uint32_t sourceBytes, std::uint64_t destination, std::uint32_t destinationBytes, const OutputBlock& output) {
    std::fill(output.Data(), output.Data() + Words, Sentinel);
    auto result = compiled;
    std::size_t elements = 0;
    for (auto& binding : result.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        Require(binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4u && binding.bufferWritten.size() == binding.count, "sparse heap import: the program's buffer bindings have another shape");
        for (std::uint32_t element = 0; element < binding.count; ++element, ++elements) {
            const auto words = binding.bufferWritten[element] ? BufferDescriptor(destination, destinationBytes) : BufferDescriptor(source, sourceBytes);
            std::copy(words.begin(), words.end(), binding.guestDescriptor.begin() + static_cast<std::ptrdiff_t>(element) * 4);
        }
    }
    Require(elements == 2, "sparse heap import: the program does not bind a source and a destination");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyCode.data()));
    device.WaitIdle();
}

void Expect(const OutputBlock& output, std::uint32_t first, const std::string& what) {
    for (std::uint32_t index = 0; index < Threads; ++index) {
        Require(output.Data()[index] == first + index, "sparse heap import: " + what + ": word " + std::to_string(index) + " is " + Hex(output.Data()[index]) + ", expected " + Hex(first + index));
    }
}

void ExpectHeap(const SparseHeap& heap, std::uint64_t offset, std::uint32_t first, const std::string& what) {
    for (std::uint32_t index = 0; index < Threads; ++index) {
        const auto value = heap.Load(offset + index * 4u);
        Require(value == first + index, "sparse heap import: " + what + ": heap word " + std::to_string(index) + " is " + Hex(value) + ", expected " + Hex(first + index));
    }
}

void StoreInPlaceTests(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled) {
    constexpr std::uint64_t Committed = 16 * Block;
    SparseHeap heap(true, Committed);
    OutputBlock source(Bytes);
    OutputBlock output;
    Fill(source.Data(), 0x5000u);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Committed + Block);
    const auto before = AgcDriver::Graphics::MirrorCounters();
    Run(device, compiled, reinterpret_cast<std::uintptr_t>(source.Data()), Bytes, heap.Address(), static_cast<std::uint32_t>(HeapBytes), output);
    const auto made = AgcDriver::Graphics::MirrorCounters();
    ExpectHeap(heap, 0, 0x5000u, "the GPU store into the run bound in place");
    Run(device, compiled, heap.Address(), static_cast<std::uint32_t>(HeapBytes), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0x5000u, "a later dispatch reading the run bound in place");
    const auto held = AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + HeapBytes);
    if (ImportsEnabled) {
        Require(held.mirrors == 0 && held.bytes == 0, "sparse heap import: the run bound in place was heap mirrored (" + std::to_string(held.bytes >> 10u) + " KiB held)");
        Require(made.sparseBinds == before.sparseBinds, "sparse heap import: the run bound in place took a sparse mirror bind");
        const auto* entry = AgcDriver::Graphics::ExistingHostImportFor(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), heap.Address(), Committed);
        Require(entry != nullptr, "sparse heap import: no host import covers the committed run");
    } else {
        Require(held.mirrors == 1, "sparse heap mirror: the run was not heap mirrored (" + std::to_string(held.mirrors) + " mirrors)");
    }

    const auto finds = AgcDriver::Graphics::ResourceCache::Finds();
    Run(device, compiled, heap.Address(), static_cast<std::uint32_t>(HeapBytes), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0x5000u, "a cached set reading the run bound in place");
    if (AgcDriver::Graphics::SharedResourceCache().Size() != 0) Require(AgcDriver::Graphics::ResourceCache::Finds() != finds, "sparse heap import: the repeated dispatch did not look the resource cache up");
}

void RunGrowsTests(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled) {
    constexpr std::uint64_t First = 16 * Block;
    constexpr std::uint64_t Second = 32 * Block;
    SparseHeap heap(true, First);
    OutputBlock source(Bytes);
    OutputBlock output;
    Fill(source.Data(), 0x6000u);
    for (std::uint32_t index = 0; index < Words; ++index) heap.Store(index * 4u, 0x7000u + index);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Second + Block);
    Run(device, compiled, heap.Address(), static_cast<std::uint32_t>(HeapBytes), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0x7000u, "the first committed run");

    heap.Commit(Second);
    Fill(source.Data(), 0x8000u);
    Run(device, compiled, reinterpret_cast<std::uintptr_t>(source.Data()), Bytes, heap.Address() + First, static_cast<std::uint32_t>(HeapBytes - First), output);
    ExpectHeap(heap, First, 0x8000u, "a store into the grown part");
    const auto before = AgcDriver::Graphics::MirrorCounters();
    const auto finds = AgcDriver::Graphics::ResourceCache::Finds();
    Run(device, compiled, heap.Address() + First, static_cast<std::uint32_t>(HeapBytes - First), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0x8000u, "a read from the grown part");
    if (AgcDriver::Graphics::SharedResourceCache().Size() != 0) Require(AgcDriver::Graphics::ResourceCache::Finds() != finds, "sparse heap import: the read from the grown part did not look the resource cache up");
    const auto made = AgcDriver::Graphics::MirrorCounters();
    if (ImportsEnabled) {
        Require(made.sparseBinds == before.sparseBinds, "sparse heap import: the grown run took a sparse mirror bind");
        const auto* entry = AgcDriver::Graphics::ExistingHostImportFor(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), heap.Address(), Second);
        Require(entry != nullptr && entry->base <= heap.Address() && entry->base + entry->bytes >= heap.Address() + Second, "sparse heap import: the second bind does not cover the new committed end");
    } else {
        Require(made.sparseBinds > before.sparseBinds, "sparse heap mirror: the grown run did not rebuild its sparse mirror");
    }
}

void GuestWriteBaseChangeTests(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled) {
    constexpr std::uint64_t Committed = 32 * Block;
    constexpr std::uint64_t Moved = 16 * Block;
    SparseHeap heap(true, Committed);
    OutputBlock source(Bytes);
    OutputBlock output;
    Fill(source.Data(), 0x9000u);
    Run(device, compiled, reinterpret_cast<std::uintptr_t>(source.Data()), Bytes, heap.Address(), static_cast<std::uint32_t>(HeapBytes), output);
    ExpectHeap(heap, 0, 0x9000u, "the first base");
    Run(device, compiled, heap.Address(), static_cast<std::uint32_t>(HeapBytes), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0x9000u, "a read at the first base");

    Fill(source.Data(), 0xa000u);
    for (std::uint32_t index = 0; index < Words; ++index) heap.Store(Moved + index * 4u, 0xa000u + index);
    Run(device, compiled, heap.Address() + Moved, static_cast<std::uint32_t>(HeapBytes - Moved), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0xa000u, "a guest write at the moved base");
}

void StorePastEndTests(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled) {
    constexpr std::uint64_t Committed = 16 * Block;
    const auto store = [&](SparseHeap& heap) {
        OutputBlock source(Bytes);
        OutputBlock output;
        Fill(source.Data(), 0xc000u);
        heap.Store(Committed - 4, 0u);
        Run(device, compiled, reinterpret_cast<std::uintptr_t>(source.Data()), Bytes, heap.Address() + Committed - 4, 8, output);
        Require(heap.Load(Committed - 4) == 0xc000u, "sparse heap import: a store ending at the import's end wrote " + Hex(heap.Load(Committed - 4)) + ", expected the first source word 0xc000");
        Require(!AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(heap.Address() + Committed), 4), "sparse heap import: the store past the import's end committed or changed the uncommitted page");
    };
    {
        SparseHeap heap(true, Committed);
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Committed + Block);
        store(heap);
    }
    {
        SparseHeap heap(true, Committed);
        const ImportBudget budget(1);
        store(heap);
    }
}

void DescriptorClampTests(AgcDriver::VulkanDevice& device) {
    if (!ImportsEnabled) {
        std::puts("the switch is off, so the in-place view is not compared with the mirror's");
        return;
    }
    constexpr std::uint64_t Committed = 80 * Block;
    SparseHeap importedHeap(true, Committed);
    SparseHeap mirroredHeap(true, Committed);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto describe = [&](const SparseHeap& heap) {
        AgcDriver::Graphics::GuestBufferMemory memory(context);
        memory.AddWritable(heap.Address(), HeapBytes);
        memory.Upload(false);
        std::uint32_t adjustment = 0;
        const auto view = memory.Descriptor(heap.Address(), HeapBytes, adjustment);
        return std::pair<std::uint64_t, VkBuffer>{view.range, view.buffer};
    };
    const auto imported = describe(importedHeap);
    Require(imported.first == Committed, "sparse heap import: the in-place view is " + Hex(imported.first) + " bytes, expected the committed end " + Hex(Committed));
    const ImportBudget budget(1);
    const auto mirrored = describe(mirroredHeap);
    Require(mirrored.first == imported.first, "sparse heap import: the sparse mirror view is " + Hex(mirrored.first) + " bytes, the in-place view " + Hex(imported.first));
    Require(mirrored.second != imported.second, "sparse heap import: the refused import did not fall back to another buffer");
}

void BudgetFallbackTests(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled) {
    if (!ImportsEnabled) {
        std::puts("the switch is off, so the refused-import fallback has no import to refuse");
        return;
    }
    constexpr std::uint64_t Committed = 16 * Block;
    SparseHeap heap(false, Committed);
    OutputBlock output;
    for (std::uint32_t index = 0; index < Words; ++index) heap.Store(index * 4u, 0xb000u + index);
    const auto before = AgcDriver::Graphics::MirrorCounters();
    const ImportBudget budget(1);
    Run(device, compiled, heap.Address(), static_cast<std::uint32_t>(HeapBytes), reinterpret_cast<std::uintptr_t>(output.Data()), Bytes, output);
    Expect(output, 0xb000u, "a refused import");
    const auto made = AgcDriver::Graphics::MirrorCounters();
    Require(made.sparseBinds == before.sparseBinds + 1, "sparse heap import: a refused import did not fall back to the sparse mirror (" + std::to_string(made.sparseBinds - before.sparseBinds) + " sparse binds)");
    const auto held = AgcDriver::Graphics::HeapMirrorsWithin(heap.Address(), heap.Address() + HeapBytes);
    Require(held.mirrors == 1 && held.bytes >= Block, "sparse heap import: the refused import left no sparse mirror over the committed run (" + std::to_string(held.mirrors) + " mirrors, " + std::to_string(held.bytes >> 10u) + " KiB held)");
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
            std::puts("skipped, no guest write watch: sparse heap imports need one");
            return VulkanTestSkipped;
        }
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const bool mirrors = context.sparseBinding && context.sparseResidencyBuffer && context.sparseResidencyAliased;
        ImportsEnabled = std::getenv("APS5_NO_SPARSE_HEAP_IMPORT") == nullptr;
        const auto compiled = Compile(*device);
        StoreInPlaceTests(*device, compiled);
        RunGrowsTests(*device, compiled);
        GuestWriteBaseChangeTests(*device, compiled);
        StorePastEndTests(*device, compiled);
        DescriptorClampTests(*device);
        if (mirrors) BudgetFallbackTests(*device, compiled);
        else std::puts("the device has no sparse residency, so the refused-import fallback is not tested");
        std::puts("sparse heap import tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
