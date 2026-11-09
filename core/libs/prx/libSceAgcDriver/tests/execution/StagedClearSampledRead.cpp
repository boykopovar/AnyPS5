#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
constexpr std::size_t KeysOffset = SurfaceBytes;
constexpr std::size_t SampleTexelBytes = 4;
constexpr std::size_t UnalignedBlockBytes = 2 * 65536 + 2048;
constexpr std::size_t ArenaBytes = 3 * 65536;

alignas(256) constexpr std::array<std::uint32_t, 8> WriteRowCode{
    0x7e020281, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a02f2, 0xf0201f08, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleSecondTexelCode{
    0x34060084, 0x7e2802ff, 0x3c400000, 0x7e2a02ff, 0x3b800000, 0xf09c0f08, 0x00610a14, 0xbf8c3f70,
    0xe0781000, 0x80000a03, 0xbf810000,
};

constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(ArenaBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, ArenaBytes, PAGE_READWRITE, ArenaBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, ArenaBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + ArenaBytes != begin + ArenaBytes + alignment) munmap(reinterpret_cast<void*>(aligned + ArenaBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, ArenaBytes);
        }
#endif
        Require(block != nullptr, "staged clear sampled read: cannot allocate the guest block");
        Require(AgcDriver::GuestMemory::Watched(AddressOf(block), UnalignedBlockBytes), "staged clear sampled read: the guest block is not write-watched");
        GuestAllocations::Mutation().Add(block, UnalignedBlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, ArenaBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, ArenaBytes);
#else
        munmap(block, ArenaBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, ArenaBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, const std::uint8_t* keys) {
    const auto address = AddressOf(texels);
    const auto meta = AddressOf(keys) >> 8u;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Unorm << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileRenderTarget64KB << 20u) | (Type2D << 28u),
        0u,
        0u,
        (1u << 20u) | (1u << 21u) | (static_cast<std::uint32_t>(meta & 0xffu) << 24u),
        static_cast<std::uint32_t>(meta >> 8u),
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::size_t Count(const std::uint8_t* bytes, std::size_t size, std::uint8_t value) {
    return static_cast<std::size_t>(std::count(bytes, bytes + size, value));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        auto* keys = texels + KeysOffset;
        std::memset(texels, 0x00, SurfaceBytes);
        const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
        std::memset(texels + layout.Offset(1, 0), 0xff, SampleTexelBytes);
        std::memset(keys, 0xff, KeyBytes);
        const auto surface = AddressOf(texels);
        const auto texture = TextureDescriptor(texels, keys);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(texture);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        std::vector<std::uint32_t> userData(24, 0u);
        std::vector<std::uint32_t> outputData(Threads * 4u, 0xdeadbeefu);
        const auto results = BufferDescriptor(outputData.data(), Threads * 16u);
        std::copy(results.begin(), results.end(), userData.begin());
        std::copy(texture.begin(), texture.end(), userData.begin() + 4);
        std::copy(PointSampler.begin(), PointSampler.end(), userData.begin() + 12);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            std::vector<std::uint32_t> writeData(16, 0u);
            std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
            AgcDriver::GuestMemory::BumpCollectEpoch();
            Dispatch(*device, WriteRowCode, writeData);
            const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
            Require(image != nullptr, "the pass left no pending results at the surface");
            Require(image->UploadedKeys() == DccKeys::Uncompressed, "the pass did not draw under the settled keys");
            std::memset(keys, 0x00, KeyBytes);
            AgcDriver::GuestMemory::BumpCollectEpoch();
            Require(AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes) == DccKeys::Clear0000, "the key bytes do not read a staged 0000 clear");
            Dispatch(*device, SampleSecondTexelCode, userData);
            Require(StorageTexture::FindPending(surface, SurfaceBytes) == nullptr, "the sampled read did not store the pending results: the lookup serves without them in guest memory");
            const auto written = Count(texels, SurfaceBytes, 0xff);
            const auto cleared = Count(texels, SurfaceBytes, 0x00);
            char message[320];
            std::snprintf(message, sizeof(message), "the sampled read stored %zu written and %zu cleared bytes of %zu, expected %zu written", written, cleared, SurfaceBytes, Threads * 4u + SampleTexelBytes);
            Require(written == Threads * 4u + SampleTexelBytes && cleared == SurfaceBytes - written, message);
            for (std::uint32_t index = 0; index < Threads * 4u; ++index) {
                std::snprintf(message, sizeof(message), "component %u of thread %u is 0x%08x, expected the drawn 0x3f800000 the staged clear does not cover", index % 4u, index / 4u, outputData[index]);
                Require(outputData[index] == 0x3f800000u, message);
            }
        }
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::FlushCachedTextures(device->Device());
        }
        device->WaitIdle();
        std::puts("staged clear sampled read tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
