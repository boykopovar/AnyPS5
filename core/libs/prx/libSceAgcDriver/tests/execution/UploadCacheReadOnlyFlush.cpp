#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
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

constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 0x60000;
constexpr std::size_t ImageBytes = Side * Side * 4u;
constexpr std::uint32_t Initial = 0x11111111u;
constexpr std::uint32_t First = 0x12345678u;
constexpr std::uint32_t Second = 0x9abcdef0u;

class GuestBlock {
public:
    explicit GuestBlock(bool watched) : watched(watched) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "upload cache: cannot allocate the guest block");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), BlockBytes), "upload cache: the guest block is not write-watched");
        std::fill(reinterpret_cast<std::uint32_t*>(block), reinterpret_cast<std::uint32_t*>(block + BlockBytes), Initial);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), ImageBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    bool watched;
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::vector<std::uint32_t> ImageUserData(const void* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    return userData;
}

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Store(AgcDriver::VulkanDevice& device, const void* texels, std::uint32_t value) {
    std::array<std::uint32_t, 7> code{
        0x7e040300u, 0x7e060280u, 0x7e0202ffu, value, 0xf0200108u, 0x00020102u, 0xbf810000u,
    };
    const auto userData = ImageUserData(texels);
    const auto result = Compile(code, userData, device.Target());
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void FlushPending(const void* texels, std::size_t bytes) {
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Require(AgcDriver::Graphics::StorageTexture::FlushPending(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels)), bytes, nullptr, "upload cache test setup"), "the setup image store left no results to flush");
    }
}

std::vector<std::uint32_t> ReadUploaded(const AgcDriver::Graphics::Context& context, AgcDriver::Graphics::GuestBufferMemory& memory, std::uint64_t address, std::size_t bytes) {
    std::uint32_t adjustment = 0;
    const auto descriptor = memory.Descriptor(address, bytes, adjustment);
    AgcDriver::Graphics::Buffer readback(context, static_cast<std::size_t>(descriptor.range), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    AgcDriver::Graphics::CommandBatch batch(context);
    AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    AgcDriver::Graphics::CopyBuffer(context, batch.Handle(), descriptor.buffer, descriptor.offset, readback.Handle(), 0, descriptor.range);
    batch.SubmitAndWait();
    readback.Invalidate();
    std::vector<std::uint32_t> words(static_cast<std::size_t>(descriptor.range) / sizeof(std::uint32_t));
    std::memcpy(words.data(), readback.Bytes().data(), words.size() * sizeof(std::uint32_t));
    return words;
}

using SparsePieces = std::vector<std::pair<std::size_t, std::size_t>>;

void ReplaySparseLayout(const AgcDriver::Graphics::Context& context, const SparsePieces& pieces) {
    using namespace AgcDriver::Graphics;
    constexpr std::size_t page = 65536;
    Require(pieces.size() >= 2 && pieces.front().first == 0, "sparse replay requires at least two pieces starting at zero");
    std::size_t size = 0;
    for (const auto& [offset, bytes] : pieces) {
        Require(offset >= size && bytes && offset % page == 0 && bytes % page == 0 && offset <= SIZE_MAX - bytes, "invalid sparse replay layout");
        size = offset + bytes;
    }
#ifdef _WIN32
    auto* data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS));
    Require(data != nullptr, "sparse replay reservation failed");
#else
    auto* data = static_cast<std::uint8_t*>(mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(data != MAP_FAILED, "sparse replay reservation failed");
#endif
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    for (const auto& [offset, bytes] : pieces) {
#ifdef _WIN32
        Require(VirtualAlloc(data + offset, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "sparse replay commit failed");
#else
        Require(mprotect(data + offset, bytes, PROT_READ | PROT_WRITE) == 0, "sparse replay commit failed");
#endif
        std::memset(data + offset, 0x5a, bytes);
        GuestAllocations::Mutation().Add(data + offset, bytes, true, true, true);
    }
    Require(AgcDriver::GuestMemory::DescribeCommitted(address, size).ranges.size() == pieces.size(), "sparse replay layout differs from input");
    VkBuffer poisonedHandle{};
    {
        Buffer poison(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memset(poison.Bytes().data(), 0xa5, size);
        poisonedHandle = poison.Handle();
    }
    const auto upload = [&](GuestBufferMemory& memory) {
        memory.AddReadable(address, size);
        memory.Upload(false);
    };
    const auto verify = [&](GuestBufferMemory& memory, bool unmapped, std::uint32_t firstWord) {
        const auto words = ReadUploaded(context, memory, address, size);
        Require(words.size() == size / 4, "unexpected sparse readback size");
        auto cursor = words.begin();
        for (std::size_t i = unmapped ? 1 : 0; i < pieces.size(); ++i) {
            const auto begin = words.begin() + pieces[i].first / 4;
            const auto end = begin + pieces[i].second / 4;
            Require(std::all_of(cursor, begin, [](auto value) { return value == 0; }), "nonzero sparse hole");
            if (i == 0) Require(*begin == firstWord, "stale backed write");
            Require(std::all_of(begin + (i == 0 ? 1 : 0), end, [](auto value) { return value == 0x5a5a5a5au; }), "incorrect backed data");
            cursor = end;
        }
    };
    {
        GuestBufferMemory held(context);
        upload(held);
        std::uint32_t adjustment = 0;
        Require(held.Descriptor(address, size, adjustment).buffer == poisonedHandle, "sparse replay did not take the poisoned pooled buffer");
        *reinterpret_cast<std::uint32_t*>(data) = Second;
        GuestBufferMemory changed(context);
        upload(changed);
        verify(held, false, 0x5a5a5a5au);
        verify(changed, false, Second);
    }
    std::vector<double> times;
    for (int iteration = 0; iteration < 18; ++iteration) {
        *reinterpret_cast<std::uint32_t*>(data) = First + iteration;
        const auto start = std::chrono::steady_clock::now();
        {
            GuestBufferMemory memory(context);
            upload(memory);
            if (iteration == 0) verify(memory, false, First);
        }
        if (iteration >= 2) times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(times.begin(), times.end());
    std::printf("sparse replay: %zu bytes, %zu pieces, median of 16 uploads %.3f ms\n", size, pieces.size(), (times[7] + times[8]) / 2);
    {
        std::array<std::unique_ptr<GuestBufferMemory>, 3> live;
        std::array<std::uint32_t, 3> values{};
        std::vector<double> overlapping;
        VkBuffer last = VK_NULL_HANDLE;
        unsigned shared = 0;
        for (unsigned iteration = 0; iteration < 20; ++iteration) {
            const auto slot = iteration % live.size();
            values[slot] = Second + iteration / 4;
            *reinterpret_cast<std::uint32_t*>(data) = values[slot];
            live[slot] = std::make_unique<GuestBufferMemory>(context);
            const auto start = std::chrono::steady_clock::now();
            upload(*live[slot]);
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            std::uint32_t adjustment = 0;
            const auto handle = live[slot]->Descriptor(address, size, adjustment).buffer;
            if (iteration >= 4) {
                overlapping.push_back(elapsed);
                shared += handle == last;
            }
            last = handle;
            if (iteration == 8) {
                for (std::size_t i = 0; i < live.size(); ++i) verify(*live[i], false, values[i]);
            }
        }
        std::sort(overlapping.begin(), overlapping.end());
        std::printf("overlapping sparse replay: %zu bytes, %zu pieces, median %.3f ms, shared %u of 16\n", size, pieces.size(), (overlapping[7] + overlapping[8]) / 2, shared);
    }
    GuestAllocations::Mutation().Unmap(data, pieces.front().second, [&](const void*, std::size_t bytes, const void*, bool) {
#ifdef _WIN32
        Require(VirtualFree(data, bytes, MEM_DECOMMIT) != 0, "sparse replay decommit failed");
#else
        Require(munmap(data, bytes) == 0, "sparse replay unmap failed");
#endif
    });
    {
        GuestBufferMemory changedLayout(context);
        upload(changedLayout);
        verify(changedLayout, true, 0);
    }
    for (std::size_t i = 1; i < pieces.size(); ++i) GuestAllocations::Mutation().Remove(data + pieces[i].first);
#ifdef _WIN32
    VirtualFree(data, 0, MEM_RELEASE);
#else
    munmap(data + pieces.front().second, size - pieces.front().second);
#endif
}

void SparsePendingWrite(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context) {
    constexpr std::size_t page = 65536;
    GuestBlock block(true);
    GuestAllocations::Mutation().Protect(block.Data() + 4 * page, page, false, false, false, [&] {
#ifdef _WIN32
        DWORD prior = 0;
        Require(VirtualProtect(block.Data() + 4 * page, page, PAGE_NOACCESS, &prior) != 0, "cannot protect sparse flush test gap");
#else
        Require(mprotect(block.Data() + 4 * page, page, PROT_NONE) == 0, "cannot protect sparse flush test gap");
#endif
    });
    AgcDriver::GuestMemory::Unwatch(block.Address() + 5 * page, page);
    Require(AgcDriver::GuestMemory::Watched(block.Address(), ImageBytes) && !AgcDriver::GuestMemory::Watched(block.Address() + 5 * page, page), "sparse flush fixture requires mixed write tracking");
    const auto upload = [&](AgcDriver::Graphics::GuestBufferMemory& memory) {
        memory.AddReadable(block.Address(), BlockBytes);
        memory.Upload(false);
        return ReadUploaded(context, memory, block.Address(), BlockBytes);
    };
    AgcDriver::Graphics::GuestBufferMemory before(context), after(context);
    const auto original = upload(before);
    Store(device, block.Data(), Second);
    Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "sparse flush fixture requires a pending GPU store");
    const auto current = upload(after);
    Require(std::find(current.begin(), current.begin() + ImageBytes / 4, Second) != current.begin() + ImageBytes / 4, "sparse validation missed the pending GPU store");
    Require(std::all_of(current.begin() + 4 * page / 4, current.begin() + 5 * page / 4, [](auto value) { return value == 0; }), "sparse flush exposed nonzero gap bytes");
    Require(ReadUploaded(context, before, block.Address(), BlockBytes) == original, "a later GPU store changed a live sparse snapshot");
    auto* later = reinterpret_cast<std::uint32_t*>(block.Data() + 5 * page);
    *later = First;
    AgcDriver::Graphics::GuestBufferMemory untracked(context);
    const auto withWrite = upload(untracked);
    Require(withWrite[5 * page / 4] == First && ReadUploaded(context, after, block.Address(), BlockBytes) == current, "an unwatched write changed a live predecessor or missed the next upload");
    const auto index = 2 * page / sizeof(std::uint32_t);
    AgcDriver::GuestMemory::Unwatch(block.Address() + 2 * page, page);
    Require(AgcDriver::GuestMemory::Watched(block.Address(), page) && !AgcDriver::GuestMemory::Watched(block.Address() + 2 * page, page) && AgcDriver::GuestMemory::Watched(block.Address() + 3 * page, page), "sparse fixture requires an unwatched page inside a backed piece");
    auto* words = reinterpret_cast<std::uint32_t*>(block.Data());
    words[index] = First;
    AgcDriver::Graphics::GuestBufferMemory mixed(context);
    const auto mixedWords = upload(mixed);
    Require(mixedWords[index] == First && ReadUploaded(context, untracked, block.Address(), BlockBytes) == withWrite, "a change of watch coverage corrupted a live sparse snapshot");
    words[index] = Second;
    AgcDriver::Graphics::GuestBufferMemory changed(context);
    const auto latest = upload(changed);
    Require(latest[index] == Second && ReadUploaded(context, mixed, block.Address(), BlockBytes) == mixedWords, "an interior unwatched write changed a live predecessor or missed the next upload");
}

bool BlockContains(const std::uint8_t* block, std::size_t bytes, std::uint32_t value) {
    const auto* words = reinterpret_cast<const std::uint32_t*>(block);
    return std::find(words, words + bytes / sizeof(std::uint32_t), value) != words + bytes / sizeof(std::uint32_t);
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_LEASE_MIRROR", "1");
#else
        setenv("APS5_NO_LEASE_MIRROR", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool watched = AgcDriver::GuestMemory::WriteWatched();
        if (!watched) {
            std::puts("skipped, guest memory has no write watch");
            return VulkanTestSkipped;
        }

        GuestBlock block(watched);
        Store(*device, block.Data(), First);
        Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "the first image store left no pending results");
        FlushPending(block.Data(), ImageBytes);
        device->WaitIdle();
        Require(BlockContains(block.Data(), ImageBytes, First), "the setup image store did not reach guest memory");

        auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        context.hostImportAlignment = 0;
        SparsePendingWrite(*device, context);
        ReplaySparseLayout(context, {{0, 65536}, {131072, 131072}});
        if (const auto path = std::getenv("APS5_SPARSE_FIXTURE_LAYOUT")) {
            std::ifstream input(path);
            Require(input.is_open(), "cannot open replay layout");
            SparsePieces pieces;
            std::size_t offset, bytes;
            while (input >> offset) {
                Require(static_cast<bool>(input >> bytes), "missing length in replay layout");
                pieces.emplace_back(offset, bytes);
            }
            Require(input.eof(), "cannot parse replay layout");
            ReplaySparseLayout(context, pieces);
        }
        {
            AgcDriver::Graphics::GuestBufferMemory memory(context);
            memory.AddReadable(block.Address(), ImageBytes);
            memory.Upload(false);
            const auto uploaded = ReadUploaded(context, memory, block.Address(), ImageBytes);
            Require(std::find(uploaded.begin(), uploaded.end(), First) != uploaded.end(), "the first build did not upload the first image bytes");
            memory.WriteBack();
        }

        Store(*device, block.Data(), Second);
        Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "the second image store left no pending results");
        {
            AgcDriver::Graphics::GuestBufferMemory memory(context);
            memory.AddReadable(block.Address(), ImageBytes);
            memory.Upload(false);
            const auto uploaded = ReadUploaded(context, memory, block.Address(), ImageBytes);
            Require(BlockContains(block.Data(), ImageBytes, Second), "the second build did not flush the second image bytes");
            Require(std::find(uploaded.begin(), uploaded.end(), Second) != uploaded.end(), "the second build reused the stale upload");
            memory.WriteBack();
        }

        std::puts("upload cache read-only flush tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
