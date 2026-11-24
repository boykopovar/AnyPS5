#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
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
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
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

constexpr std::uint32_t ImageSide = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint64_t UnreadBlock = 16 * Block;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};
// The same image store with a float texel, so a draw reads it back as a colour: 0.25 and 1.0.
constexpr std::uint32_t QuarterFloat = 0x3e800000u;
constexpr std::uint32_t OneFloat = 0x3f800000u;
alignas(256) constexpr std::array<std::uint32_t, 7> StoreQuarterCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, QuarterFloat, 0xf0200108, 0x00020102, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 7> StoreOneCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, OneFloat, 0xf0200108, 0x00020102, 0xbf810000,
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
#ifdef __APPLE__
        mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(at);
        raw = at == nullptr ? mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)
                            : (mach_vm_allocate(mach_task_self(), &address, bytes + Block, VM_FLAGS_FIXED) == KERN_SUCCESS ? reinterpret_cast<void*>(address) : MAP_FAILED);
#else
        raw = mmap(at, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | (at != nullptr ? MAP_FIXED_NOREPLACE : 0), -1, 0);
#endif
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

    void SetWritable(bool writable) const {
        GuestAllocations::Mutation mutation;
        for (std::uint64_t range = 0; range < ranges; ++range) {
            mutation.Remove(base + range * rangeBytes);
            mutation.Add(base + range * rangeBytes, rangeBytes, true, writable, true);
        }
    }

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

void StoreImage(AgcDriver::VulkanDevice& device, std::uint64_t address, std::uint64_t keys = 0, std::span<const std::uint32_t> code = StoreCode) {
    std::vector<std::uint32_t> userData(16, 0u);
    userData[8] = static_cast<std::uint32_t>(address >> 8u);
    userData[9] = static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((ImageSide - 1u) & 3u) << 30u);
    userData[10] = ((ImageSide - 1u) >> 2u) | ((ImageSide - 1u) << 14u);
    userData[11] = 0xfacu | (TileR64KBX << 20u) | (Type2D << 28u);
    if (keys != 0) {
        userData[14] = (1u << 20u) | (1u << 21u) | (static_cast<std::uint32_t>((keys >> 8u) & 0xffu) << 24u);
        userData[15] = static_cast<std::uint32_t>(keys >> 16u);
    }
    Run(device, code, userData);
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
    const auto settled = AgcDriver::Graphics::MirrorCounters();
    AgcDriver::GuestMemory::MarkWritten(read.Address() + 8, 16);
    Dispatch(device, read, written, output);
    Require(AgcDriver::Graphics::MirrorCounters().blocksCopied == settled.blocksCopied, "resident heap mirror: a block stamped without a byte change was copied into its mirror again");
    CheckReads(output, "a stamped block that did not change", true);
    CheckStores(written, "a stamped block that did not change");
}

void StagingChunkTests(AgcDriver::VulkanDevice& device) {
    constexpr std::uint64_t LargeBytes = 48u << 20u;
    constexpr std::uint64_t ChunkBoundary = 32u << 20u;
    constexpr std::uint64_t First = ChunkBoundary - Threads / 2 * ThreadStride;
    GuestHeap read(false, 1, LargeBytes);
    GuestHeap written(true);
    const OutputBlock output;
    for (std::uint64_t block = 0; block < LargeBytes; block += Block) read.Store(block + 8, 0x11u);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) read.Store(First + thread * ThreadStride, Pattern(thread));
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    const auto before = AgcDriver::Graphics::MirrorCounters();
    Dispatch(device, read.Address() + First, written.Address(), output);
    const auto made = AgcDriver::Graphics::MirrorCounters();
    Require(made.heapBytes - before.heapBytes > First + Threads * ThreadStride, "resident heap mirror: a " + std::to_string(LargeBytes >> 20u) + " MiB heap mirror does not reach past the read range (" + std::to_string((made.heapBytes - before.heapBytes) >> 10u) + " KiB held)");
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Require(output.Data()[thread] == Pattern(thread), "resident heap mirror: an upload split into staging chunks: thread " + std::to_string(thread) + " read " + Hex(output.Data()[thread]) + " at heap offset " + Hex(First + thread * ThreadStride) + ", expected " + Hex(Pattern(thread)));
    }
}

void StorageImageZeroStoreTests(AgcDriver::VulkanDevice& device) {
    GuestHeap heap(true);
    GuestHeap written(true);
    const OutputBlock output;
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    Dispatch(device, heap, written, output);
    StoreImage(device, heap.Address() + UnreadBlock);
    heap.Store(UnreadBlock, 0u);
    std::uint32_t texel = 1u;
    AgcDriver::GuestMemory::Read(heap.Address() + UnreadBlock, std::as_writable_bytes(std::span(&texel, 1)));
    Require(texel == 0u, "resident heap mirror: a CPU store of zero after an image store into a never-written block read " + Hex(texel));
}

constexpr std::uint32_t CpuWord = 0x0c0ffee0u;
constexpr std::uint32_t BlockThread = static_cast<std::uint32_t>(Block / ThreadStride);

std::uint32_t ReadGuest(const GuestHeap& heap, std::uint64_t offset) {
    std::uint32_t texel = 0;
    AgcDriver::GuestMemory::Read(heap.Address() + offset, std::as_writable_bytes(std::span(&texel, 1)));
    return texel;
}

void StorageImageTests(AgcDriver::VulkanDevice& device, bool published, bool copied) {
    using AgcDriver::Graphics::StorageTexture;
    const auto mode = std::string(published ? "published" : "stored") + " storage image: ";
    {
        GuestHeap heap(true);
        GuestHeap written(true);
        std::memset(heap.Data(), 0, HeapBytes);
        const OutputBlock output;
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        Dispatch(device, heap, written, output);
        Require(output.Data()[0] == 0u, mode + "the first texel read " + Hex(output.Data()[0]) + " before any image store");
        StoreImage(device, heap.Address());
        const auto before = StorageTexture::MirrorPublishTotals();
        Dispatch(device, heap, written, output);
        Require(output.Data()[0] == Stored, mode + "an address-based read after an image store inside the mirror read " + Hex(output.Data()[0]));
        if (copied && StorageTexture::MirrorPublishEnabled()) {
            const auto index = static_cast<std::size_t>(StorageTexture::MirrorFallback::Copied);
            Require(StorageTexture::MirrorPublishTotals().reasons[index] > before.reasons[index], mode + "a build over a copied heap mirror did not count the copied fallback");
        }
        if (published) {
            Require(heap.Load(0) == 0u, mode + "the address-based build stored the pending image to guest memory (raw guest word " + Hex(heap.Load(0)) + ") instead of publishing it into the heap mirror");
            Require(StorageTexture::AnyPending(), mode + "the address-based build left no storage results pending");
            Require(StorageTexture::MirrorPublishTotals().published > before.published, mode + "the address-based build published nothing into the heap mirror");
            const auto first = StorageTexture::MirrorPublishTotals();
            Dispatch(device, heap, written, output);
            const auto again = StorageTexture::MirrorPublishTotals();
            Require(output.Data()[0] == Stored, mode + "an unchanged second build read " + Hex(output.Data()[0]));
            Require(again.published == first.published && again.skipped > first.skipped, mode + "an unchanged second build published the image again (" + std::to_string(again.published - first.published) + " publishes, " + std::to_string(again.skipped - first.skipped) + " skips)");
            StoreImage(device, heap.Address());
            Dispatch(device, heap, written, output);
            const auto restored = StorageTexture::MirrorPublishTotals();
            Require(output.Data()[0] == Stored && restored.published > again.published, mode + "a build after a second image store did not publish the image again (" + std::to_string(restored.published - again.published) + " publishes, " + std::to_string(restored.skipped - again.skipped) + " skips)");
            heap.Store(Block, CpuWord);
            Dispatch(device, heap, written, output);
            Require(output.Data()[0] == Stored && output.Data()[BlockThread] == CpuWord, mode + "after a CPU write into another block of the image an address-based build read " + Hex(output.Data()[0]) + " and " + Hex(output.Data()[BlockThread]) + ", expected the texel and the CPU word");
        }
        Require(ReadGuest(heap, 0) == Stored, mode + "a CPU read through the hook after the address-based builds read " + Hex(ReadGuest(heap, 0)));
        StoreImage(device, heap.Address() + UnreadBlock);
        Require(ReadGuest(heap, UnreadBlock) == Stored, mode + "a CPU read after an image store inside the mirror read " + Hex(ReadGuest(heap, UnreadBlock)));
    }
    if (!published) return;
    {
        GuestHeap heap(true);
        GuestHeap written(true);
        std::memset(heap.Data(), 0, HeapBytes);
        const OutputBlock output;
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        Dispatch(device, heap, written, output);
        StoreImage(device, heap.Address());
        heap.Store(0, CpuWord);
        Dispatch(device, heap, written, output);
        Require(output.Data()[0] == CpuWord, mode + "a CPU write over the pending texel after the image store lost to the image: the address-based build read " + Hex(output.Data()[0]));
        Require(ReadGuest(heap, 0) == CpuWord, mode + "a CPU write over the pending texel lost to the image in guest memory: " + Hex(ReadGuest(heap, 0)));
    }
    {
        GuestHeap heap(true);
        std::memset(heap.Data(), 0, HeapBytes);
        const OutputBlock output;
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        Dispatch(device, heap, heap, output);
        StoreImage(device, heap.Address());
        const auto before = StorageTexture::MirrorPublishTotals();
        Dispatch(device, heap, heap, output);
        const auto after = StorageTexture::MirrorPublishTotals();
        const auto writtenReason = static_cast<std::size_t>(StorageTexture::MirrorFallback::Written);
        Require(after.reasons[writtenReason] > before.reasons[writtenReason], mode + "a build writing the image's range of a writable heap mirror did not fall back to storing the image for that reason (" + std::to_string(after.fallbacks - before.fallbacks) + " fallbacks of any reason)");
        Require(output.Data()[0] == Stored && heap.Load(0) == Stored, mode + "the writable-heap fallback read " + Hex(output.Data()[0]) + " and left " + Hex(heap.Load(0)) + " in guest memory, expected the texel in both");
        CheckStores(heap, "writable-heap fallback");
    }
    {
        constexpr std::uint64_t ImageOffset = 4 * Block;
        constexpr std::uint64_t KeysOffset = 8 * Block;
        constexpr std::uint32_t ImageThread = static_cast<std::uint32_t>(ImageOffset / ThreadStride);
        constexpr std::size_t ImageBytes = static_cast<std::size_t>(ImageSide) * ImageSide * sizeof(std::uint32_t);
        GuestHeap heap(true);
        GuestHeap written(true);
        std::memset(heap.Data(), 0, HeapBytes);
        std::memset(heap.Data() + KeysOffset, 0xff, ImageBytes / 256u);
        const OutputBlock output;
        const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        Dispatch(device, heap, written, output);
        StoreImage(device, heap.Address() + ImageOffset, heap.Address() + KeysOffset);
        const auto image = StorageTexture::FindPending(heap.Address() + ImageOffset, ImageBytes);
        Require(image != nullptr && image->Descriptor().dccAddress == heap.Address() + KeysOffset && image->UploadedKeys() == AgcDriver::Graphics::DccKeys::Uncompressed, mode + "the image store with uncompressed DCC keys left no pending DCC image uploaded under uncompressed keys (pending " + std::to_string(image != nullptr) + ", keys at " + Hex(image != nullptr ? image->Descriptor().dccAddress : 0u) + ", key state " + std::to_string(image != nullptr ? static_cast<int>(image->UploadedKeys()) : -1) + ")");
        const auto index = static_cast<std::size_t>(StorageTexture::MirrorFallback::DccKeys);
        const auto before = StorageTexture::MirrorPublishTotals();
        Dispatch(device, heap, written, output);
        const auto after = StorageTexture::MirrorPublishTotals();
        Require(output.Data()[ImageThread] == Stored, mode + "an address-based read after a DCC image store with uncompressed keys read " + Hex(output.Data()[ImageThread]));
        Require(after.published > before.published && after.reasons[index] == before.reasons[index] && heap.Load(ImageOffset) == 0u, mode + "a DCC image whose keys read uncompressed was not published into the heap mirror (" + std::to_string(after.published - before.published) + " publishes, " + std::to_string(after.reasons[index] - before.reasons[index]) + " DCC key fallbacks, raw guest word " + Hex(heap.Load(ImageOffset)) + ")");
        Require(ReadGuest(heap, ImageOffset) == Stored, mode + "a CPU read through the hook after the DCC image publish read " + Hex(ReadGuest(heap, ImageOffset)));
    }
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
    const auto beforeJoin = AgcDriver::Graphics::HostImportSetGeneration();
    Copy(device, begin + JoinedBytes - Threads * sizeof(std::uint32_t) / 2, output);
    Require(output.Data()[Threads / 2 - 1] == 0x41u && output.Data()[Threads / 2] == 0x42u, "resident heap mirror: a view across two adjacent heaps read " + Hex(output.Data()[Threads / 2 - 1]) + " " + Hex(output.Data()[Threads / 2]) + ", expected 0x41 0x42");
    const auto joined = AgcDriver::Graphics::HostImportLiveBytes() - live;
    Require(joined >= 2 * JoinedBytes, "resident heap mirror: a view across two adjacent heaps made no joined host import (" + std::to_string(joined >> 10u) + " KiB imported)");
    Require(AgcDriver::Graphics::HostImportSetGeneration() != beforeJoin, "resident heap mirror: creating a joined import did not move the import set generation");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
        const auto* imported = AgcDriver::Graphics::HostImportFor(context, begin, 2 * JoinedBytes);
        Require(imported != nullptr, "resident heap mirror: the joined host import was not reusable");
        const auto generation = AgcDriver::Graphics::HostImportSetGeneration();
        const auto identity = AgcDriver::Graphics::HostImportsIdentity(context);
        for (unsigned hit = 0; hit < 3; ++hit) {
            Require(AgcDriver::Graphics::HostImportFor(context, begin, 2 * JoinedBytes) == imported, "resident heap mirror: a repeated joined lookup replaced the import");
            Require(AgcDriver::Graphics::HostImportSetGeneration() == generation, "resident heap mirror: a joined import cache hit moved the generation");
            Require(AgcDriver::Graphics::HostImportsIdentity(context).inserted == identity.inserted, "resident heap mirror: a joined import cache hit changed insertion bookkeeping");
        }
    }
    Dispatch(device, begin, begin + JoinedBytes, output);
    CheckReads(output, "after the joined import", false);
    const auto released = AgcDriver::Graphics::HeapMirrorsWithin(begin, end);
    Require(released.mirrors == 0 && released.bytes == 0, "resident heap mirror: a joined host import over two heap-mirrored heaps left " + std::to_string(released.mirrors) + " of their " + std::to_string(mirrored.mirrors) + " heap mirrors and " + std::to_string(released.bytes >> 10u) + " of " + std::to_string(mirrored.bytes >> 10u) + " KiB held");
}

struct HeapMirrorBudget {
    explicit HeapMirrorBudget(std::uint64_t bytes) : previous(current) {
        current = bytes;
        AgcDriver::Graphics::SetHeapMirrorBudget(bytes);
    }
    ~HeapMirrorBudget() {
        current = previous;
        AgcDriver::Graphics::SetHeapMirrorBudget(previous);
    }
    HeapMirrorBudget(const HeapMirrorBudget&) = delete;
    HeapMirrorBudget& operator=(const HeapMirrorBudget&) = delete;

private:
    static inline std::uint64_t current = 0;
    std::uint64_t previous = 0;
};

void BudgetEvictionTests(AgcDriver::VulkanDevice& device, bool sparse) {
    const auto mirrorUnit = sparse ? Block : HeapBytes;
    {
        GuestHeap written(true);
        written.Store(16, 0x77u);
        const OutputBlock output;
        const ImportBudget importBudget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        Dispatch(device, written, written, output);
        const auto baseline = AgcDriver::Graphics::MirrorCounters();

        GuestHeap first(false);
        for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) first.Store(thread * ThreadStride, Pattern(thread));
        const auto firstAddress = first.Address();
        const HeapMirrorBudget budget(baseline.heapBytes + mirrorUnit);
        Dispatch(device, first, written, output);
        CheckReads(output, "budget eviction: first dispatch", false);
        CheckStores(written, "budget eviction: first dispatch");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(firstAddress, firstAddress + HeapBytes).mirrors == 1, "budget eviction: first heap mirror missing");

        auto hold = AgcDriver::Graphics::HoldHeapMirror(firstAddress);
        Require(hold != nullptr, "budget eviction: could not acquire mirror handle to test reference retention");
        auto holdWritten = AgcDriver::Graphics::HoldHeapMirror(written.Address());
        Require(holdWritten != nullptr, "budget eviction: could not acquire written mirror handle to test reference retention");
        bool heldThrew = false;
        try {
            GuestHeap blocked(false);
            for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) blocked.Store(thread * ThreadStride, ~Pattern(thread));
            Dispatch(device, blocked, written, output);
        } catch (const std::runtime_error& error) {
            heldThrew = std::string(error.what()).find("past the budget") != std::string::npos;
        }
        Require(heldThrew, "budget eviction: referenced mirror was evicted under budget constraint");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(firstAddress, firstAddress + HeapBytes).mirrors == 1, "budget eviction: referenced mirror dropped prematurely");
        holdWritten.reset();
        hold.reset();

        const auto beforeEviction = AgcDriver::Graphics::MirrorCounters();
        GuestHeap second(false);
        for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) second.Store(thread * ThreadStride, ~Pattern(thread));
        Dispatch(device, second, written, output);
        for (std::uint32_t thread = 0; thread < Threads; ++thread) {
            const auto expected = thread * ThreadStride < Block ? ~Pattern(thread) : 0u;
            Require(output.Data()[thread] == expected, "budget eviction: second dispatch thread " + std::to_string(thread) + " mismatch");
        }
        Require(AgcDriver::Graphics::HeapMirrorsWithin(firstAddress, firstAddress + HeapBytes).mirrors == 0, "budget eviction: first heap mirror was not evicted");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(second.Address(), second.Address() + HeapBytes).mirrors == 1, "budget eviction: second heap mirror missing");
        Require(AgcDriver::Graphics::MirrorCounters().evictions == beforeEviction.evictions + 1, "budget eviction: eviction counter was not incremented");

        Dispatch(device, first, written, output);
        CheckReads(output, "budget eviction: re-upload first dispatch", false);
        CheckStores(written, "budget eviction: re-upload first dispatch");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(firstAddress, firstAddress + HeapBytes).mirrors == 1, "budget eviction: first heap mirror re-upload missing");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(second.Address(), second.Address() + HeapBytes).mirrors == 0, "budget eviction: second heap mirror was not evicted on re-upload");
        Require(AgcDriver::Graphics::MirrorCounters().evictions == beforeEviction.evictions + 2, "budget eviction: re-upload eviction counter mismatch");
    }

    {
        const ImportBudget importBudget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        const HeapMirrorBudget buildBudget(mirrorUnit);
        GuestHeap heapA(false);
        GuestHeap heapB(false);
        const bool aIsLower = heapA.Address() < heapB.Address();
        auto& freshRead = aIsLower ? heapA : heapB;
        auto& freshWritten = aIsLower ? heapB : heapA;
        freshWritten.SetWritable(true);
        std::memset(freshRead.Data(), 0, HeapBytes);
        for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) freshRead.Store(thread * ThreadStride, Pattern(thread));
        std::memset(freshWritten.Data(), 0, HeapBytes);
        freshWritten.Store(StoreBlock + 512, 0x77u);
        const OutputBlock output;
        const auto beforeBuild = AgcDriver::Graphics::MirrorCounters();
        Dispatch(device, freshRead, freshWritten, output);
        CheckReads(output, "budget eviction: build-time eviction", false);
        CheckStores(freshWritten, "budget eviction: build-time eviction");
        Require(freshWritten.Load(StoreBlock + 512) == 0x77u, "budget eviction: build-time eviction: guest bytes changed");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(freshRead.Address(), freshRead.Address() + HeapBytes).mirrors == 0, "budget eviction: build-time read-only mirror was not evicted");
        Require(AgcDriver::Graphics::HeapMirrorsWithin(freshWritten.Address(), freshWritten.Address() + HeapBytes).mirrors == 1, "budget eviction: build-time written mirror missing");
        Require(AgcDriver::Graphics::MirrorCounters().evictions >= beforeBuild.evictions + 1, "budget eviction: build-time eviction counter mismatch");
    }

    bool threw = false;
    try {
        const ImportBudget importBudget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
        const HeapMirrorBudget lowBudget(mirrorUnit / 2);
        GuestHeap fail(false);
        for (std::uint32_t thread = 0; thread * ThreadStride < Block; ++thread) fail.Store(thread * ThreadStride, Pattern(thread));
        GuestHeap failWritten(true);
        const OutputBlock output;
        Dispatch(device, fail, failWritten, output);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("past the budget") != std::string::npos;
    }
    Require(threw, "budget eviction negative control: dispatch under insufficient budget did not throw");
}

}

constexpr std::uint32_t DrawSide = 64;
constexpr std::size_t DrawVertexOffset = 0;
constexpr std::size_t DrawPixelOffset = 256;

alignas(256) constexpr std::array<std::uint32_t, 6> DrawVertexCode{
    0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};
// Loads the float at the address in the first two user data words and exports it to all four channels.
alignas(256) constexpr std::array<std::uint32_t, 7> DrawPixelCode{
    0x7e000280u, 0xdc308000u, 0x01000000u, 0xbf8c3f70u, 0xf800180fu, 0x01010101u, 0xbf810000u,
};
constexpr std::array<std::array<float, 4>, 3> DrawTriangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

// A full-screen draw whose pixel shader reads one float through an address-based buffer: the draw takes the
// cached address space of an earlier identical draw (RetakeSpace) and the float is the first texel of the heap.
class ReadDraw {
public:
    ReadDraw(AgcDriver::VulkanDevice& device, std::uint64_t source) : device(device), pixels(true, 1, Block), vertices(false, 1, Block), code(false, 1, Block) {
        using AgcDriver::Graphics::ColorTileMode;
        std::memcpy(vertices.Data() + DrawVertexOffset, DrawTriangle.data(), sizeof(DrawTriangle));
        std::memcpy(code.Data(), DrawVertexCode.data(), sizeof(DrawVertexCode));
        std::memcpy(code.Data() + DrawPixelOffset, DrawPixelCode.data(), sizeof(DrawPixelCode));
        const auto layout = AgcDriver::Graphics::ColorTargetLayout(DrawSide, DrawSide, ColorTileMode::RenderTarget, 4);
        bytes = layout.Bytes();
        Require(bytes <= Block, "resident heap mirror: the draw target does not fit one block");
        const auto fetch = vertices.Address();
        const std::vector<std::uint32_t> fetchData{static_cast<std::uint32_t>(fetch), static_cast<std::uint32_t>((fetch >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Block / 16u), 0x01016facu};
        const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{code.Address(), std::as_bytes(std::span(DrawVertexCode))}}};
        ShaderRecompiler::RecompileRequest vertex{
            {ShaderStage::Vertex, code.Address(), DrawVertexCode, 0, {}},
            {64, 0, fetchData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
            device.Target(),
            {0, 0, 0, 64}
        };
        vertex.useCache = false;
        vertexResult = ShaderRecompiler::Recompile(vertex);
        const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
        const auto target = device.Target();
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
        pixel.posX = true;
        pixel.posY = true;
        pixel.targetOutputMode[0] = 9;
        pixel.targetExportMapping.fill(0xe4u);
        const std::vector<std::uint32_t> pixelData{static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), 0u, 0u};
        const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{(code.Address() + DrawPixelOffset), std::as_bytes(std::span(DrawPixelCode))}}};
        ShaderRecompiler::RecompileRequest fragment{
            {ShaderStage::Fragment, (code.Address() + DrawPixelOffset), DrawPixelCode, 0, {}},
            {64, 0, pixelData, std::nullopt, pixel, std::nullopt, pixelMemory},
            target,
            PixelPushLayout(vertexPush, target)
        };
        fragment.useCache = true;
        pixelResult = ShaderRecompiler::Recompile(fragment);
        Require(std::any_of(pixelResult.bindings.begin(), pixelResult.bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; }), "resident heap mirror: the draw's pixel shader does not read through the address table");
        shaders[0] = {ShaderStage::Vertex, &vertexResult, 0};
        shaders[1] = {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)};
        snapshots = {
            {code.Address(), std::as_bytes(std::span(reinterpret_cast<const std::uint32_t*>(code.Data()), DrawVertexCode.size()))},
            {(code.Address() + DrawPixelOffset), std::as_bytes(std::span(reinterpret_cast<const std::uint32_t*>(code.Data() + DrawPixelOffset), DrawPixelCode.size()))},
        };
        state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
        state.color = {pixels.Address(), {DrawSide, DrawSide}, VK_FORMAT_R8G8B8A8_UNORM, bytes, 0xe4u, ColorTileMode::RenderTarget};
        state.colors = {state.color};
        state.hasColorTarget = true;
        state.renderExtent = {DrawSide, DrawSide};
        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        state.viewport = {0, static_cast<float>(DrawSide), static_cast<float>(DrawSide), -static_cast<float>(DrawSide), 0, 1};
        state.negativeOneToOne = false;
        state.scissor = {{0, 0}, {DrawSide, DrawSide}};
        state.cullMode = VK_CULL_MODE_NONE;
        state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        state.blend.colorWriteMask = 15;
        state.blends = {state.blend};
        state.blendConstants = {};
        layoutOffset = layout.Offset(0, 0);
    }

    // Draws, waits, and returns the colour of the first texel; every other texel must hold the same value.
    std::uint32_t Draw() {
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(DrawTriangle.size()), 0, 1, 0, false};
            device.Draw(state, draw, shaders, snapshots, nullptr);
            device.WaitIdle();
        }
        AgcDriver::Graphics::StorageTexture::FlushPending(pixels.Address(), bytes, nullptr, "test read-back");
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            device.WaitIdle();
        }
        std::vector<std::uint8_t> texels(bytes);
        AgcDriver::GuestMemory::Read(pixels.Address(), std::as_writable_bytes(std::span(texels)), 1);
        const auto first = texels[layoutOffset];
        for (std::size_t channel = 1; channel < 4; ++channel) Require(texels[layoutOffset + channel] == first, "resident heap mirror: the draw's channels differ");
        return first;
    }

private:
    AgcDriver::VulkanDevice& device;
    GuestHeap pixels;
    GuestHeap vertices;
    GuestHeap code;
    std::size_t bytes = 0;
    std::size_t layoutOffset = 0;
    ShaderRecompiler::RecompileResult vertexResult;
    ShaderRecompiler::RecompileResult pixelResult;
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{};
    std::vector<AgcDriver::Graphics::GuestMemorySnapshot> snapshots;
    AgcDriver::Graphics::State state{};
};

// A storage image deferred into a heap mirror reaches a later draw only through publishIntoMirrors(). A draw that
// takes the cached address space of an identical earlier draw (RetakeSpace) collects the deferred images while it
// refreshes the heap and drains them in RecordStagingCopies, so a lost drain leaves the mirror with the older
// texels and the draw reads them: the colour is the previous build's, with no other symptom.
void ReusedBuildPublishTests(AgcDriver::VulkanDevice& device) {
    using AgcDriver::Graphics::AddressDrawReuse;
    using AgcDriver::Graphics::AddressDrawReuseCounters;
    const auto name = std::string("reused build publish: ");
    GuestHeap heap(true);
    std::memset(heap.Data(), 0, HeapBytes);
    const ImportBudget budget(AgcDriver::Graphics::HostImportLiveBytes() + Block);
    ReadDraw draw(device, heap.Address());
    const auto reused = [] { return AddressDrawReuseCounters()[static_cast<std::size_t>(AddressDrawReuse::Reused)]; };
    const auto colour = [](float value) { return static_cast<std::uint32_t>(value * 255.0f + 0.5f); };
    const auto close = [](std::uint32_t actual, std::uint32_t expected) { return actual + 1u >= expected && actual <= expected + 1u; };

    const auto first = draw.Draw();
    Require(first == 0u, name + "the first draw over a zeroed heap read colour " + std::to_string(first));
    StoreImage(device, heap.Address(), 0, StoreQuarterCode);
    auto before = reused();
    const auto second = draw.Draw();
    Require(close(second, colour(0.25f)), name + "a draw after an image store (build 1: the image is deferred into the mirror) read colour " + std::to_string(second) + ", expected " + std::to_string(colour(0.25f)));
    const auto reuses = reused() - before;
    Require(reuses == 1, name + "the draw after the image store was not a reuse of the cached address space (" + std::to_string(reuses) + " reuses); this test needs build 2 to take RetakeSpace");

    StoreImage(device, heap.Address(), 0, StoreOneCode);
    before = reused();
    const auto third = draw.Draw();
    Require(reused() - before == 1, name + "the draw after the second image store was not a reuse of the cached address space");
    Require(close(third, colour(1.0f)), name + "a reused build (build 2: it takes the cached address space and writes new texels) read colour " + std::to_string(third) + " from the heap mirror, expected " + std::to_string(colour(1.0f)) + "; the previous build's colour is " + std::to_string(colour(0.25f)));
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
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const bool sparse = context.sparseBinding && context.sparseResidencyBuffer && context.sparseResidencyAliased;
        if (!sparse && std::getenv("ANYPS5_TEST_EXPECT_MIRROR_PUBLISH") != nullptr) {
            std::puts("skipped, the device has no sparse residency: the publish negative control needs resident heap mirrors");
            return VulkanTestSkipped;
        }
        if (sparse) {
            ResidentTests(*device);
            StagingChunkTests(*device);
            ReusedAddressTests(*device);
            JoinedImportTests(*device);
        } else {
            std::puts("the device has no sparseBinding, sparseResidencyBuffer and sparseResidencyAliased, so every heap mirror is a whole copy; the resident heap mirror tests are skipped");
        }
        CopiedTests(*device);
        BudgetEvictionTests(*device, sparse);
        if (sparse) StorageImageTests(*device, AgcDriver::Graphics::StorageTexture::MirrorPublishEnabled() || std::getenv("ANYPS5_TEST_EXPECT_MIRROR_PUBLISH") != nullptr, false);
        if (sparse && AgcDriver::Graphics::StorageTexture::MirrorPublishEnabled()) ReusedBuildPublishTests(*device);
        {
            const AddressSpaceLimit limit(1);
            StorageImageTests(*device, false, true);
        }
        StorageImageZeroStoreTests(*device);
        for (const bool copied : {false, true}) {
            if (!copied && !sparse) continue;
            for (const bool readFirst : {false, true}) PartialImportTests(*device, copied, readFirst);
        }
        std::puts("resident heap mirror tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
