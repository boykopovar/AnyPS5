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
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format32x2Uint = 62;
constexpr std::uint32_t Format32x4Uint = 75;
constexpr std::uint32_t FormatBc1Unorm = 169;
constexpr std::uint32_t FormatBc7Unorm = 181;
constexpr std::uint32_t ForceDegamma = 1u << 20u;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::uint32_t Texels = 256;

struct Shape {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
};

struct Case {
    std::string name;
    Shape blocks;
    Shape compressed;
    std::span<const std::uint32_t> store;
};

alignas(256) constexpr std::array<std::uint32_t, 10> StoreQuadCode{
    0x7e28020c, 0x7e2a0300, 0x7e2c020c, 0x7e2e0300, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> StorePairCode{
    0x7e28020c, 0x7e2a0300, 0x7e3c0300, 0x7e3e0280, 0xf0201308, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> SampleCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf09c0f08, 0x00820402, 0xbf8c3f70,
    0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000,
};

std::span<float> Input;
std::span<std::uint32_t> Output;

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
        Require(block != nullptr, "bc block copy: cannot allocate the guest block");
        Require(AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(block), BlockBytes), "bc block copy: the guest block is not write-watched");
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
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t forced) {
    return {ClampEdge | (ClampEdge << 3u) | (ClampEdge << 6u) | forced, 0u, 0u, 0u};
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

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Case& test, std::uint32_t value) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, test.blocks);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = value;
    const auto result = Compile(device, test.store, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(test.store.data()));
    device.WaitIdle();
}

std::uint64_t SurfaceBytes(std::uint8_t* texels, const Shape& shape) {
    return AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels, shape))).guestBytes;
}

void Settle(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint64_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), bytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(texels), bytes);
}

std::vector<std::uint32_t> Sample(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::uint32_t forced = 0u) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size_bytes()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size_bytes()));
    const auto texture = TextureDescriptor(texels, shape);
    const auto sampler = SamplerDescriptor(forced);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const auto program = Compile(device, SampleCode, userData);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 3u + 0u] = 0.0f;
        Input[tid * 3u + 1u] = (static_cast<float>(tid) * 4.0f + 1.5f) / static_cast<float>(shape.width);
        Input[tid * 3u + 2u] = 1.5f / static_cast<float>(shape.height);
    }
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    device.WaitIdle();
    return {Output.begin(), Output.end()};
}

std::vector<std::uint32_t> Reference(AgcDriver::VulkanDevice& device, const Case& test, std::uint32_t value, std::uint32_t forced = 0u) {
    GuestBlock block;
    auto* texels = block.Data();
    Store(device, texels, test, value);
    Settle(device, texels, SurfaceBytes(texels, test.blocks));
    return Sample(device, texels, test.compressed, forced);
}

void RequireSame(const std::vector<std::uint32_t>& sampled, const std::vector<std::uint32_t>& expected, const std::string& what) {
    for (std::size_t word = 0; word < expected.size(); ++word) {
        Require(sampled[word] == expected[word], what + ": thread " + std::to_string(word / 4u) + " component " + std::to_string(word % 4u) + " read " + std::to_string(sampled[word]) + ", expected " + std::to_string(expected[word]));
    }
}

void PendingBlocksCase(AgcDriver::VulkanDevice& device, const Case& test) {
    constexpr std::uint32_t First = 0x13572440u;
    constexpr std::uint32_t Second = 0x9bdf6840u;
    const auto first = Reference(device, test, First);
    const auto second = Reference(device, test, Second);
    const auto forcedFirst = Reference(device, test, First, ForceDegamma);
    const auto forcedSecond = Reference(device, test, Second, ForceDegamma);
    Require(first != second, test.name + ": the two block patterns sample alike");
    Require(forcedFirst != first && forcedSecond != second, test.name + ": a sampler that forces sRGB decoding samples the stored blocks like a plain one");
    Require(forcedFirst != forcedSecond, test.name + ": the two block patterns sample alike through a sampler that forces sRGB decoding");
    GuestBlock block;
    auto* texels = block.Data();
    const auto bytes = SurfaceBytes(texels, test.compressed);
    Require(bytes == SurfaceBytes(texels, test.blocks), test.name + ": the block view and the compressed view disagree on the surface size");
    Store(device, texels, test, First);
    Settle(device, texels, bytes);
    RequireSame(Sample(device, texels, test.compressed), first, test.name + ": sample of stored blocks");
    Store(device, texels, test, Second);
    const auto pending = Sample(device, texels, test.compressed);
    Require(AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(texels), bytes) != nullptr, test.name + ": sampling the compressed view stored the pending blocks to guest memory");
    RequireSame(pending, second, test.name + ": sample of pending blocks");
    Store(device, texels, test, First);
    RequireSame(Sample(device, texels, test.compressed), first, test.name + ": sample of blocks stored again");
    Store(device, texels, test, Second);
    RequireSame(Sample(device, texels, test.compressed, ForceDegamma), forcedSecond, test.name + ": sRGB-forced sample of pending blocks");
    Require(AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(texels), bytes) != nullptr, test.name + ": the sRGB-forced sample stored the pending blocks to guest memory");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock buffers;
        Input = std::span<float>(reinterpret_cast<float*>(buffers.Data()), Threads * 3u);
        Output = std::span<std::uint32_t>(reinterpret_cast<std::uint32_t*>(buffers.Data() + 4096u), Threads * 4u);
        PendingBlocksCase(*device, {"BC7 through 32x4 uint blocks", {Format32x4Uint, Texels / 4u, Texels / 4u}, {FormatBc7Unorm, Texels, Texels}, StoreQuadCode});
        PendingBlocksCase(*device, {"BC1 through 32x2 uint blocks", {Format32x2Uint, Texels / 4u, Texels / 4u}, {FormatBc1Unorm, Texels, Texels}, StorePairCode});
        std::puts("bc block copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
