#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
#include <bit>
#include <cmath>
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
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t ResultWord = 8;
constexpr std::uint32_t Size = 512;
constexpr std::uint32_t Levels = 6;
constexpr std::uint32_t Gradients = 4;
constexpr std::uint32_t Clamps = 4;
constexpr std::uint32_t MinLodLevel = 2;
constexpr std::uint32_t FormatBc1Unorm = 169;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockAlignment = 65536;
constexpr std::size_t PageBytes = 16384;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};

alignas(256) constexpr std::array<std::uint32_t, 61> Code{
    0x34020087, 0xe0301000, 0x80000501, 0xe0301004, 0x80000601, 0xe0301008, 0x80000701, 0xe030100c,
    0x80000801, 0xe0301010, 0x80000901, 0xe0301014, 0x80000a01, 0xe0301018, 0x80000b01, 0xbf8c3f70,
    0xf08c0f08, 0x00610c05, 0xe0701020, 0x80000c01, 0xe0701024, 0x80000d01, 0xe0701028, 0x80000e01,
    0xe070102c, 0x80000f01, 0xf0840f08, 0x00610c09, 0xe0701030, 0x80000c01, 0xe0701034, 0x80000d01,
    0xe0701038, 0x80000e01, 0xe070103c, 0x80000f01, 0x7e0802ff, 0x00000201, 0xf0cc0f08, 0x00610c04,
    0xe0701040, 0x80000c01, 0xe0701044, 0x80000d01, 0xe0701048, 0x80000e01, 0xe070104c, 0x80000f01,
    0x7e0802ff, 0x00003e3f, 0xf0cc0f08, 0x00610c04, 0xe0701050, 0x80000c01, 0xe0701054, 0x80000d01,
    0xe0701058, 0x80000e01, 0xe070105c, 0x80000f01, 0xbf810000,
};

struct Sample {
    std::uint32_t level;
    std::uint32_t gradient;
    std::uint32_t clamp;
    std::uint32_t x;
    std::uint32_t y;
};

Sample SampleOf(std::uint32_t tid) {
    return {tid % 5u, (tid / 5u) % Gradients, (tid / 3u) % Clamps, (tid * 7u + 3u) % Size, (tid * 5u + 1u) % Size};
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t LevelRed(std::uint32_t level) {
    return 4u + level * 4u;
}

const std::vector<AgcDriver::Graphics::TileMipLayout>& Mips() {
    static const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, FormatBc1Unorm, Size, Size, Levels);
    return mips;
}

std::size_t Padding() {
    const auto unmapped = static_cast<std::size_t>(Mips()[MinLodLevel - 1u].tiledOffset);
    return (PageBytes - unmapped % PageBytes) % PageBytes;
}

std::size_t BlockBytes() {
    const auto bytes = Padding() + static_cast<std::size_t>(AgcDriver::Graphics::ComputeSurfaceSize(Mips(), 1));
    return (bytes + BlockAlignment - 1u) / BlockAlignment * BlockAlignment;
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t committed) : bytes(BlockBytes()), committed(committed) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockAlignment));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, committed, PAGE_READWRITE, committed);
#else
        void* mapped = mmap(nullptr, bytes + BlockAlignment, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockAlignment - 1) & ~(static_cast<std::uintptr_t>(BlockAlignment) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockAlignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockAlignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            Require(mprotect(block, committed, PROT_READ | PROT_WRITE) == 0, "sparse texture levels: cannot commit the resident levels");
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, committed);
        }
#endif
        Require(block != nullptr, "sparse texture levels: cannot allocate the guest block");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block), bytes);
        GuestAllocations::Mutation().Add(block, committed, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
        munmap(block, bytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, committed);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Texels() { return block + Padding(); }

private:
    std::uint8_t* block = nullptr;
    std::size_t bytes = 0;
    std::size_t committed = 0;
};

void FillInput() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        auto* words = &Buffer[tid * Words];
        for (std::uint32_t gradient = 0; gradient < Gradients; ++gradient) {
            words[gradient] = Bits(gradient == sample.gradient ? std::exp2(static_cast<float>(sample.level) - 0.25f) / static_cast<float>(Size) : 0.0f);
        }
        words[4] = Bits((static_cast<float>(sample.x) + 0.5f) / static_cast<float>(Size));
        words[5] = Bits((static_cast<float>(sample.y) + 0.5f) / static_cast<float>(Size));
        words[6] = Bits(static_cast<float>(sample.clamp));
    }
}

void FillLevels(std::uint8_t* texels, std::uint32_t firstLevel) {
    for (std::uint32_t level = firstLevel; level < Levels; ++level) {
        const auto& mip = Mips()[level];
        const std::uint16_t color = static_cast<std::uint16_t>(LevelRed(level) << 11u);
        for (std::uint32_t row = 0; row < mip.height; ++row) {
            for (std::uint32_t column = 0; column < mip.width; ++column) {
                auto* block = texels + mip.tiledOffset + static_cast<std::uint64_t>(row) * mip.pitchBytes + column * 8u;
                std::memset(block, 0, 8);
                std::memcpy(block, &color, sizeof(color));
            }
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | ((MinLodLevel * 256u) << 8u) | (FormatBc1Unorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {0u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Dispatch(AgcDriver::VulkanDevice& device, const std::uint8_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(texels);
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
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

void Check(const char* surface) {
    constexpr std::array<const char*, 4> names{"image_sample_d_cl", "image_sample_cl", "image_sample_d_cl_o (1, 2)", "image_sample_d_cl_o (-1, -2)"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto clamped = std::max({sample.level, sample.clamp, MinLodLevel});
        const std::array<std::uint32_t, 4> levels{clamped, std::max(sample.clamp, MinLodLevel), clamped, clamped};
        for (std::uint32_t index = 0; index < levels.size(); ++index) {
            const float red = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u]) * 31.0f;
            const float alpha = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u + 3u]);
            const auto expected = LevelRed(levels[index]);
            Require(std::lround(red) == static_cast<long>(expected) && std::lround(alpha) == 1, std::string(surface) + ": " + names[index] + " in thread " + std::to_string(tid) + " read red " + std::to_string(red) + " and alpha " + std::to_string(alpha) + ", expected level " + std::to_string(levels[index]) + " (red " + std::to_string(expected) + ")");
        }
    }
}

std::uint64_t SampledBytes(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, const char* surface) {
    FillInput();
    const auto before = AgcDriver::Graphics::SampledTextureMemory();
    Dispatch(device, texels);
    const auto after = AgcDriver::Graphics::SampledTextureMemory();
    Check(surface);
    return after - before;
}

bool SparseBc1(const AgcDriver::Graphics::Context& context) {
    if (!context.sparseResidencyImage2D || context.sparseImageFormatProperties == nullptr) return false;
    std::uint32_t count = 0;
    context.sparseImageFormatProperties(context.physical, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, &count, nullptr);
    return count != 0;
}

}

int main() {
    try {
        const auto resident = Padding() + static_cast<std::size_t>(Mips()[MinLodLevel - 1u].tiledOffset);
        Require(Mips()[MinLodLevel - 1u].tiledOffset > Mips()[MinLodLevel].tiledOffset && Mips()[0].tiledOffset >= Mips()[MinLodLevel - 1u].tiledOffset, "sparse texture levels: the linear mip chain does not keep its top levels last");
        GuestBlock streamed(resident);
        GuestBlock whole(BlockBytes());
        FillLevels(streamed.Texels(), MinLodLevel);
        FillLevels(whole.Texels(), 0);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = device->graphicsContext();
        if (!context.textureCompressionBC || !context.imageViewMinLod) {
            std::puts("skipped, the device has no BC texture compression or VK_EXT_image_view_min_lod");
            return VulkanTestSkipped;
        }
        const auto streamedBytes = SampledBytes(*device, streamed.Texels(), "levels above the minimum LOD unmapped");
        const auto wholeBytes = SampledBytes(*device, whole.Texels(), "every level mapped");
        if (SparseBc1(context)) {
            Require(streamedBytes != 0 && streamedBytes < wholeBytes, "sparse texture levels: the texture with unmapped levels above its minimum LOD holds " + std::to_string(streamedBytes) + " bytes, the one with every level mapped " + std::to_string(wholeBytes));
            std::printf("sparse texture levels: %llu bytes with the levels above the minimum LOD unmapped, %llu with every level mapped\n", static_cast<unsigned long long>(streamedBytes), static_cast<unsigned long long>(wholeBytes));
        } else {
            std::printf("sparse texture levels: the device has no sparse BC1 residency, both textures hold every level (%llu and %llu bytes)\n", static_cast<unsigned long long>(streamedBytes), static_cast<unsigned long long>(wholeBytes));
        }
        std::puts("sparse texture levels tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
