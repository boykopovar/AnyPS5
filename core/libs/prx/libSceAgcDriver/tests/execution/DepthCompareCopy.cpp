#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t LessEqual = 3;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::size_t BlockBytes = 2u << 20u;

struct Shape {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t layers;
    std::uint32_t mips;
    bool array;
};

alignas(256) constexpr std::array<std::uint32_t, 9> StoreHalfCode{
    0x7e280d00, 0x102828ff, 0x3d000000, 0x7e3c0300, 0x7e3e0280, 0xf0201108, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> StoreQuarterCode{
    0x7e280d00, 0x102828ff, 0x3c800000, 0x7e3c0300, 0x7e3e0280, 0xf0201108, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> StoreLayerMipCode{
    0x7e280d00, 0x1028280e, 0x7e3c0300, 0x7e3e0280, 0x7e40020c, 0x7e42020d, 0xf0241128, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> SampleCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0bc0108, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 15> SampleLayerMipCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0x7e080214, 0x7e0a0215, 0xbf8c3f70, 0xf0b00128, 0x00820601, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001060b, 0xbf810000, 0xbf810000, 0xbf810000,
};

std::span<float> Input;
std::span<float> Output;

class GuestBlock {
public:
    GuestBlock() {
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
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "depth compare copy: cannot allocate the guest block");
        Require(AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(block), BlockBytes), "depth compare copy: the guest block is not write-watched");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, const Shape& shape) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (shape.format << 20u) | (((shape.width - 1u) & 3u) << 30u),
        ((shape.width - 1u) >> 2u) | ((shape.height - 1u) << 14u),
        0xfacu | ((shape.mips - 1u) << 16u) | ((shape.array ? Type2DArray : Type2D) << 28u),
        shape.array ? shape.layers - 1u : 0u,
        (shape.mips - 1u) << 4u,
        0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(bool mips) {
    return {ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u) | (LessEqual << 12u), mips ? 0xf00u << 12u : 0u, mips ? 1u << 26u : 0u, 0u};
}

std::uint32_t Bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::span<const std::uint32_t> code, std::uint32_t layer = 0, std::uint32_t mip = 0, float scale = 0.0f) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, shape);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = layer;
    userData[13] = mip;
    userData[14] = Bits(scale);
    const auto result = Compile(device, code, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

ShaderRecompiler::RecompileResult SampleProgram(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::uint32_t layer = 0, std::uint32_t mip = 0) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size_bytes()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size_bytes()));
    const auto texture = TextureDescriptor(texels, shape);
    const auto sampler = SamplerDescriptor(shape.mips > 1u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    userData[20] = Bits(static_cast<float>(layer));
    userData[21] = Bits(static_cast<float>(mip));
    return Compile(device, shape.array ? std::span<const std::uint32_t>(SampleLayerMipCode) : std::span<const std::uint32_t>(SampleCode), userData);
}

void Sample(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& program, const Shape& shape, float scale, const std::string& what, std::uint32_t layer = 0, std::uint32_t mip = 0) {
    const auto width = std::max(shape.width >> mip, 1u);
    const auto height = std::max(shape.height >> mip, 1u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float value = static_cast<float>(tid) * scale;
        Input[tid * 3u + 0u] = (tid & 1u) != 0u ? value - scale / 4.0f : value + scale / 4.0f;
        Input[tid * 3u + 1u] = (static_cast<float>(tid) + 0.5f) / static_cast<float>(width);
        Input[tid * 3u + 2u] = 0.5f / static_cast<float>(height);
    }
    std::fill(Output.begin(), Output.end(), -1.0f);
    const auto* code = shape.array ? SampleLayerMipCode.data() : SampleCode.data();
    device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float expected = (tid & 1u) != 0u ? 1.0f : 0.0f;
        Require(Output[tid] == expected, what + ": thread " + std::to_string(tid) + " compared to " + std::to_string(Output[tid]) + ", expected " + std::to_string(expected));
    }
}

std::uint64_t SurfaceBytes(std::uint8_t* texels, const Shape& shape) {
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels, shape)));
    Require(surface.guestBytes >= (1u << 20u) && surface.guestBytes <= BlockBytes, "depth compare copy: the surface does not fit the guest block or is below the depth copy floor");
    return surface.guestBytes;
}

std::uint64_t PendingVersion(std::uint8_t* texels, std::uint64_t bytes, const std::string& what) {
    const auto source = AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(texels), bytes);
    Require(source != nullptr, what + ": no storage image holds the surface's results");
    return source->Version();
}

void Settle(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint64_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), bytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(texels), bytes);
}

void RequireCounts(std::uint64_t textures, std::uint64_t refreshes, const std::string& what) {
    const auto counts = AgcDriver::Graphics::Texture::TakeDepthCopyCounts();
    Require(counts.textures == textures && counts.refreshes == refreshes, what + ": " + std::to_string(counts.textures) + " depth copy textures and " + std::to_string(counts.refreshes) + " refreshes, expected " + std::to_string(textures) + " and " + std::to_string(refreshes));
}

void RequireNewerVersion(std::uint64_t before, std::uint64_t after, const std::string& what) {
    Require(after > before, what + ": the storage image version stayed at " + std::to_string(before));
}

float Texel(const std::uint8_t* texels, std::uint32_t x) {
    float value = 0.0f;
    std::memcpy(&value, texels + static_cast<std::size_t>(x) * sizeof(float), sizeof(float));
    return value;
}

void RequireGuestTexels(const std::uint8_t* texels, float scale, const std::string& what) {
    for (std::uint32_t x = 0; x < Threads; ++x) {
        const float expected = static_cast<float>(x) * scale;
        Require(Texel(texels, x) == expected, what + ": texel " + std::to_string(x) + " is " + std::to_string(Texel(texels, x)) + ", expected " + std::to_string(expected));
    }
}

void SingleLevelCase(AgcDriver::VulkanDevice& device, const Shape& shape, const std::string& name) {
    GuestBlock block;
    auto* texels = block.Data();
    const auto bytes = SurfaceBytes(texels, shape);
    Store(device, texels, shape, StoreQuarterCode);
    Settle(device, texels, bytes);
    AgcDriver::Graphics::Texture::TakeDepthCopyCounts();
    Store(device, texels, shape, StoreHalfCode);
    const auto program = SampleProgram(device, texels, shape);
    Sample(device, program, shape, 1.0f / 32.0f, name + ": comparison sample of pending results");
    RequireCounts(1, 1, name + ": first comparison sample");
    const auto before = PendingVersion(texels, bytes, name);
    Store(device, texels, shape, StoreQuarterCode);
    RequireNewerVersion(before, PendingVersion(texels, bytes, name), name + ": second store");
    Sample(device, program, shape, 1.0f / 64.0f, name + ": comparison sample of the same descriptor set after a second store");
    RequireCounts(0, 1, name + ": comparison sample after a second store");
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), bytes, nullptr, "test");
    device.WaitIdle();
    if (shape.format == Format32Float) RequireGuestTexels(texels, 1.0f / 64.0f, name + ": flushed results");
}

void LayersAndMipsCase(AgcDriver::VulkanDevice& device) {
    const Shape shape{Format32Float, 256, 256, 4, 3, true};
    const std::string name = "R32F 256x256, 4 layers, 3 mips";
    GuestBlock block;
    auto* texels = block.Data();
    const auto bytes = SurfaceBytes(texels, shape);
    Store(device, texels, shape, StoreLayerMipCode, 0, 0, 1.0f / 512.0f);
    Settle(device, texels, bytes);
    AgcDriver::Graphics::Texture::TakeDepthCopyCounts();
    Store(device, texels, shape, StoreLayerMipCode, 0, 0, 1.0f / 32.0f);
    Store(device, texels, shape, StoreLayerMipCode, 2, 1, 1.0f / 64.0f);
    Store(device, texels, shape, StoreLayerMipCode, 3, 2, 1.0f / 128.0f);
    const auto first = SampleProgram(device, texels, shape, 0, 0);
    const auto second = SampleProgram(device, texels, shape, 2, 1);
    const auto third = SampleProgram(device, texels, shape, 3, 2);
    Sample(device, first, shape, 1.0f / 32.0f, name + ": layer 0 mip 0", 0, 0);
    Sample(device, second, shape, 1.0f / 64.0f, name + ": layer 2 mip 1", 2, 1);
    Sample(device, third, shape, 1.0f / 128.0f, name + ": layer 3 mip 2", 3, 2);
    RequireCounts(1, 1, name + ": first comparison samples");
    const auto before = PendingVersion(texels, bytes, name);
    Store(device, texels, shape, StoreLayerMipCode, 2, 1, 1.0f / 256.0f);
    RequireNewerVersion(before, PendingVersion(texels, bytes, name), name + ": second store");
    Sample(device, second, shape, 1.0f / 256.0f, name + ": layer 2 mip 1 after a second store", 2, 1);
    Sample(device, first, shape, 1.0f / 32.0f, name + ": layer 0 mip 0 after a second store", 0, 0);
    RequireCounts(0, 1, name + ": comparison samples after a second store");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock buffers;
        Input = std::span<float>(reinterpret_cast<float*>(buffers.Data()), Threads * 3u);
        Output = std::span<float>(reinterpret_cast<float*>(buffers.Data() + 4096u), Threads);
        SingleLevelCase(*device, {Format32Float, 512, 512, 1, 1, false}, "R32F 512x512");
        SingleLevelCase(*device, {Format16Unorm, 1024, 512, 1, 1, false}, "R16 unorm 1024x512");
        LayersAndMipsCase(*device);
        std::puts("depth compare copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
