#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
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
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
constexpr std::size_t KeysOffset = SurfaceBytes;
constexpr std::size_t BlockBytes = 2 * 65536 + 2048;
constexpr std::size_t ArenaBytes = 3 * 65536;

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
        Require(block != nullptr, "sampled clear keys: cannot allocate the guest block");
        Require(AgcDriver::GuestMemory::Watched(AddressOf(block), BlockBytes), "sampled clear keys: the guest block is not write-watched");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
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

void Sample(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, std::vector<std::uint32_t>& output, const std::string& what, float expected) {
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto results = BufferDescriptor(output.data(), Threads * 16u);
    std::copy(results.begin(), results.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(PointSampler.begin(), PointSampler.end(), userData.begin() + 12);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Dispatch(device, SampleSecondTexelCode, userData);
    for (std::uint32_t index = 0; index < Threads * 4u; ++index) {
        const auto value = std::bit_cast<float>(output[index]);
        char message[256];
        std::snprintf(message, sizeof(message), "%s: component %u of thread %u is %g (0x%08x), expected %g", what.c_str(), index % 4u, index / 4u, value, output[index], expected);
        Require(std::fabs(value - expected) < 1e-6f, message);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        auto* keys = texels + KeysOffset;
        std::memset(texels, 0x5a, SurfaceBytes);
        const auto texture = TextureDescriptor(texels, keys);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(texture);
        std::vector<std::uint32_t> output(Threads * 4u, 0u);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            std::memset(keys, 0xff, KeyBytes);
            AgcDriver::GuestMemory::BumpCollectEpoch();
            Require(AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes) == DccKeys::Uncompressed, "the 0xff key bytes do not read uncompressed");
            const auto before = AgcDriver::Graphics::SampledTexturesCleared();
            Sample(*device, texture, output, "uncompressed texels", 90.0f / 255.0f);
            Require(AgcDriver::Graphics::SampledTexturesCleared() == before, "an uncompressed surface was made by a clear instead of from its texels");

            std::memset(keys, 0xc0, KeyBytes);
            AgcDriver::GuestMemory::BumpCollectEpoch();
            Require(AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes) == DccKeys::Clear1111, "the 0xc0 key bytes do not read a 1111 clear");
            Sample(*device, texture, output, "1111 fast clear", 1.0f);
            Require(AgcDriver::Graphics::SampledTexturesCleared() == before + 1, "the 1111 fast-cleared surface was not made by a clear on the GPU");

            std::memset(keys, 0x00, KeyBytes);
            AgcDriver::GuestMemory::BumpCollectEpoch();
            Require(AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes) == DccKeys::Clear0000, "the 0x00 key bytes do not read a 0000 clear");
            Sample(*device, texture, output, "0000 fast clear", 0.0f);
            Require(AgcDriver::Graphics::SampledTexturesCleared() == before + 2, "the 0000 fast-cleared surface was not made by a clear on the GPU");
        }
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::FlushCachedTextures(device->Device());
        }
        device->WaitIdle();
        std::puts("sampled clear keys tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
