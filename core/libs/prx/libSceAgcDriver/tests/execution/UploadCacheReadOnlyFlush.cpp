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
    explicit GuestBlock(bool watched, std::size_t reserved = BlockBytes) : watched(watched), reserved(reserved) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(reserved, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, reserved + alignment, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + reserved != begin + reserved + alignment) munmap(reinterpret_cast<void*>(aligned + reserved), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (mprotect(block, BlockBytes, PROT_READ | PROT_WRITE) != 0) {
                munmap(block, reserved);
                block = nullptr;
            }
            if (block != nullptr && watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
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
        GuestArena::GuestArenaRelease_nid_postfix(block, reserved);
#else
        munmap(block, reserved);
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    bool watched;
    std::size_t reserved;
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

bool BlockContains(const std::uint8_t* block, std::size_t bytes, std::uint32_t value) {
    const auto* words = reinterpret_cast<const std::uint32_t*>(block);
    return std::find(words, words + bytes / sizeof(std::uint32_t), value) != words + bytes / sizeof(std::uint32_t);
}

void CheckSparseAddressReads(const AgcDriver::Graphics::Context& context) {
    using AgcDriver::Graphics::GuestBufferMemory;
    constexpr std::size_t extent = BlockBytes + 65536;
    GuestBlock block(true, extent);
    const auto address = block.Address();
    AgcDriver::GuestMemory::Unwatch(address, BlockBytes);
    Require(!AgcDriver::GuestMemory::CollectWritesUncached(address, BlockBytes), "sparse address reads need unwatched backing");
    const auto upload = [&](GuestBufferMemory& memory, bool writable) {
        memory.AddReadable(address, extent);
        if (writable) memory.AddWritable(address, sizeof(std::uint32_t));
        memory.Upload(true);
        std::uint32_t adjustment = 0;
        return memory.Descriptor(address, sizeof(std::uint32_t), adjustment).buffer;
    };
    {
        GuestBufferMemory dense(context), next(context);
        const auto first = upload(dense, false);
        Require(upload(next, false) != first, "a trimmed dense address upload took the sparse comparison path");
    }
    constexpr std::size_t page = 65536;
    auto* gap = block.Data() + page;
    GuestAllocations::Mutation().Protect(gap, page, false, false, false, [&] {
#ifdef _WIN32
        DWORD prior = 0;
        Require(VirtualProtect(gap, page, PAGE_NOACCESS, &prior) != 0, "cannot protect sparse address gap");
#else
        Require(mprotect(gap, page, PROT_NONE) == 0, "cannot protect sparse address gap");
#endif
    });
    GuestBufferMemory original(context), equal(context), changed(context), writer(context);
    const auto first = upload(original, false);
    Require(upload(equal, false) == first, "equal sparse address reads allocated another buffer");
    std::memcpy(block.Data(), &Second, sizeof(Second));
    const auto replacement = upload(changed, false);
    Require(replacement != first, "a changed address read reused a live snapshot");
    Require(ReadUploaded(context, original, address, sizeof(Initial)).front() == Initial, "a live address read lost its old bytes");
    Require(ReadUploaded(context, changed, address, sizeof(Second)).front() == Second, "a new address read missed a guest store");
    const auto writable = upload(writer, true);
    Require(writable != first && writable != replacement, "a descriptor write shared a read-only address snapshot");
    std::puts("sparse address snapshot readbacks passed");
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_LEASE_MIRROR", "1");
        _putenv_s("APS5_COMPARE_SPARSE_UPLOADS", "1");
#else
        setenv("APS5_NO_LEASE_MIRROR", "1", 1);
        setenv("APS5_COMPARE_SPARSE_UPLOADS", "1", 1);
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

        if (context.bufferDeviceAddress) CheckSparseAddressReads(context);
        std::puts("upload cache read-only flush tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
