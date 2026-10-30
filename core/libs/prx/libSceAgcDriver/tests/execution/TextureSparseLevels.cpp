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
constexpr std::uint32_t Size = 2048;
constexpr std::uint32_t Levels = 8;
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
    return 3u + level * 4u;
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

    void Commit(std::size_t through) {
        Require(through > committed && through <= bytes && through % PageBytes == 0, "sparse texture levels: the newly committed range does not end on a page past the committed one");
        const auto added = through - committed;
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(block + committed, added, PAGE_READWRITE, added);
#else
        Require(mprotect(block + committed, added, PROT_READ | PROT_WRITE) == 0, "sparse texture levels: cannot commit a streamed level");
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block + committed, added);
#endif
        GuestAllocations::Mutation().Remove(block);
        committed = through;
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

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t minLodLevel) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | ((minLodLevel * 256u) << 8u) | (FormatBc1Unorm << 20u) | (((Size - 1u) & 3u) << 30u),
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

void Dispatch(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::uint32_t minLodLevel) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor(texels, minLodLevel);
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

void Check(const std::string& surface, std::uint32_t minLodLevel) {
    constexpr std::array<const char*, 4> names{"image_sample_d_cl", "image_sample_cl", "image_sample_d_cl_o (1, 2)", "image_sample_d_cl_o (-1, -2)"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto clamped = std::max({sample.level, sample.clamp, minLodLevel});
        const std::array<std::uint32_t, 4> levels{clamped, std::max(sample.clamp, minLodLevel), clamped, clamped};
        for (std::uint32_t index = 0; index < levels.size(); ++index) {
            const float red = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u]) * 31.0f;
            const float alpha = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u + 3u]);
            const auto expected = LevelRed(levels[index]);
            Require(std::lround(red) == static_cast<long>(expected) && std::lround(alpha) == 1, surface + ": " + names[index] + " in thread " + std::to_string(tid) + " read red " + std::to_string(red) + " and alpha " + std::to_string(alpha) + ", expected level " + std::to_string(levels[index]) + " (red " + std::to_string(expected) + ")");
        }
    }
}

struct Sampled {
    std::uint64_t bytes;
    std::array<std::uint32_t, Threads * Words> results;
};

Sampled SampleTexture(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::uint32_t minLodLevel, const std::string& surface) {
    FillInput();
    const auto before = AgcDriver::Graphics::SampledTextureMemory();
    Dispatch(device, texels, minLodLevel);
    const auto after = AgcDriver::Graphics::SampledTextureMemory();
    Check(surface + " (MIN_LOD " + std::to_string(minLodLevel) + ")", minLodLevel);
    return {after - before, Buffer};
}

VkSparseImageMemoryRequirements Requirement(VkImageAspectFlags aspect, VkExtent3D granularity, VkSparseImageFormatFlags flags, std::uint32_t tailFirst, VkDeviceSize tailSize, VkDeviceSize tailOffset, VkDeviceSize tailStride) {
    return {{aspect, granularity, flags}, tailFirst, tailSize, tailOffset, tailStride};
}

void CheckPlans() {
    using AgcDriver::Graphics::PlanSparseLevels;
    constexpr VkDeviceSize Block = 65536;
    const VkExtent3D extent{1024, 1024, 1};
    const auto color = Requirement(VK_IMAGE_ASPECT_COLOR_BIT, {128, 128, 1}, 0, 3, 65536, 0x100000, 0x10000);
    const auto singleMetadata = Requirement(VK_IMAGE_ASPECT_METADATA_BIT, {0, 0, 0}, VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT, 0, 4096, 0x200000, 0);
    const auto layerMetadata = Requirement(VK_IMAGE_ASPECT_METADATA_BIT, {0, 0, 0}, 0, 0, 8192, 0x300000, 0x4000);
    const std::array colorOnly{color};
    const auto plain = PlanSparseLevels(extent, 8, 2, Block, colorOnly, 1);
    Require(plain.has_value() && plain->levels.size() == 4u && plain->tails.size() == 2u && plain->bytes == 2u * (20u * Block + Block), "sparse level plan: two layers of levels 1 and 2 (16 and 4 blocks) and a mip tail per layer are not what was planned");
    Require(plain->levels[0].subresource.mipLevel == 1u && plain->levels[1].subresource.mipLevel == 2u && plain->levels[2].subresource.arrayLayer == 1u && plain->levels[1].memoryOffset == 16u * Block, "sparse level plan: the level binds are out of place");
    Require(plain->tails[1].resourceOffset == 0x110000u && plain->tails[1].flags == 0u, "sparse level plan: the second layer's mip tail is not at the tail offset plus the stride");
    const std::array withSingle{color, singleMetadata};
    const auto single = PlanSparseLevels(extent, 8, 2, Block, withSingle, 1);
    Require(single.has_value() && single->tails.size() == 3u && single->bytes == plain->bytes + Block, "sparse level plan: a single metadata mip tail is not bound once");
    Require(single->tails[2].flags == VK_SPARSE_MEMORY_BIND_METADATA_BIT && single->tails[2].resourceOffset == 0x200000u && single->tails[2].size == 4096u && single->tails[2].memoryOffset == plain->bytes, "sparse level plan: the metadata bind does not carry VK_SPARSE_MEMORY_BIND_METADATA_BIT at the metadata tail");
    const std::array withLayers{layerMetadata, color};
    const auto layered = PlanSparseLevels(extent, 8, 2, Block, withLayers, 1);
    Require(layered.has_value() && layered->tails.size() == 4u && layered->tails[2].resourceOffset == 0x300000u && layered->tails[3].resourceOffset == 0x304000u && layered->tails[3].flags == VK_SPARSE_MEMORY_BIND_METADATA_BIT && layered->bytes == plain->bytes + 2u * Block, "sparse level plan: a per-layer metadata mip tail is not bound for each layer");
    const std::array tailOnly{color};
    const auto tail = PlanSparseLevels(extent, 8, 1, Block, tailOnly, 4);
    Require(tail.has_value() && tail->levels.empty() && tail->tails.size() == 1u && tail->bytes == Block, "sparse level plan: levels past the mip tail's first level do not bind the tail alone");
    const std::array withDepth{color, Requirement(VK_IMAGE_ASPECT_DEPTH_BIT, {128, 128, 1}, 0, 3, 65536, 0, 0)};
    Require(!PlanSparseLevels(extent, 8, 1, Block, withDepth, 1).has_value(), "sparse level plan: an aspect other than color and metadata was planned");
    const std::array metadataOnly{singleMetadata};
    Require(!PlanSparseLevels(extent, 8, 1, Block, metadataOnly, 1).has_value(), "sparse level plan: requirements without a color aspect were planned");
}

std::string AspectText(VkImageAspectFlags aspects) {
    if (aspects == VK_IMAGE_ASPECT_COLOR_BIT) return "color";
    if (aspects == VK_IMAGE_ASPECT_METADATA_BIT) return "metadata";
    return "aspects " + std::to_string(aspects);
}

void ReportDeviceRequirements(const AgcDriver::Graphics::Context& context) {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    info.extent = {Size, Size, 1};
    info.mipLevels = Levels;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImage image = VK_NULL_HANDLE;
    AgcDriver::Graphics::Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage sparse BC1 probe");
    std::uint32_t count = 0;
    context.Function<PFN_vkGetImageSparseMemoryRequirements>("vkGetImageSparseMemoryRequirements")(context.device, image, &count, nullptr);
    std::vector<VkSparseImageMemoryRequirements> requirements(count);
    context.Function<PFN_vkGetImageSparseMemoryRequirements>("vkGetImageSparseMemoryRequirements")(context.device, image, &count, requirements.data());
    VkMemoryRequirements memory{};
    context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &memory);
    context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
    for (const auto& entry : requirements) {
        std::printf("sparse texture levels: a %ux%u BC1 image with %u levels reports %s: block %ux%u, flags 0x%x, mip tail from level %u, %llu bytes at 0x%llx, stride 0x%llx; sparse block %llu bytes\n", Size, Size, Levels, AspectText(entry.formatProperties.aspectMask).c_str(), entry.formatProperties.imageGranularity.width, entry.formatProperties.imageGranularity.height, entry.formatProperties.flags, entry.imageMipTailFirstLod, static_cast<unsigned long long>(entry.imageMipTailSize), static_cast<unsigned long long>(entry.imageMipTailOffset), static_cast<unsigned long long>(entry.imageMipTailStride), static_cast<unsigned long long>(memory.alignment));
    }
    Require(AgcDriver::Graphics::PlanSparseLevels(info.extent, info.mipLevels, info.arrayLayers, memory.alignment, requirements, MinLodLevel).has_value(), "sparse texture levels: the device's sparse BC1 requirements cannot be planned");
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
        CheckPlans();
        const auto resident = Padding() + static_cast<std::size_t>(Mips()[MinLodLevel - 1u].tiledOffset);
        const auto streamedLevel = Padding() + static_cast<std::size_t>(Mips()[0].tiledOffset);
        Require(Mips()[MinLodLevel - 1u].tiledOffset > Mips()[MinLodLevel].tiledOffset && Mips()[0].tiledOffset >= Mips()[MinLodLevel - 1u].tiledOffset && MinLodLevel == 2u, "sparse texture levels: the linear mip chain does not keep its top levels last");
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
        if (!SparseBc1(context)) {
            std::puts("skipped, the device has no sparse residency for 2D BC1 images (sparseResidencyImage2D and vkGetPhysicalDeviceSparseImageFormatProperties), so textures keep every level");
            return VulkanTestSkipped;
        }
        ReportDeviceRequirements(context);
        const auto first = SampleTexture(*device, streamed.Texels(), MinLodLevel, "levels above the minimum LOD unmapped");
        const auto full = SampleTexture(*device, whole.Texels(), MinLodLevel, "every level mapped");
        Require(first.bytes != 0 && first.bytes < full.bytes, "sparse texture levels: the texture with unmapped levels above its minimum LOD holds " + std::to_string(first.bytes) + " bytes, the one with every level mapped " + std::to_string(full.bytes));
        Require(first.results == full.results, "sparse texture levels: the texture with unmapped levels samples differently from the one with every level mapped (MIN_LOD 2)");
        streamed.Commit(streamedLevel);
        FillLevels(streamed.Texels(), MinLodLevel - 1u);
        const auto mapped = SampleTexture(*device, streamed.Texels(), MinLodLevel, "level 1 mapped later");
        const auto lowered = SampleTexture(*device, streamed.Texels(), MinLodLevel - 1u, "level 1 mapped later");
        const auto fullLowered = SampleTexture(*device, whole.Texels(), MinLodLevel - 1u, "every level mapped");
        Require(mapped.results == full.results, "sparse texture levels: mapping level 1 changed what the texture samples at MIN_LOD 2");
        Require(lowered.results == fullLowered.results, "sparse texture levels: after level 1 was mapped and MIN_LOD lowered to 1, the texture samples differently from the one with every level mapped");
        Require(lowered.bytes != 0 && lowered.bytes < fullLowered.bytes, "sparse texture levels: at MIN_LOD 1 with level 0 unmapped the texture holds " + std::to_string(lowered.bytes) + " bytes, the one with every level mapped " + std::to_string(fullLowered.bytes));
        std::printf("sparse texture levels: MIN_LOD 2 holds %llu bytes with levels 0 and 1 unmapped and %llu with every level mapped; MIN_LOD 1 after level 1 was mapped holds %llu bytes, %llu with every level mapped\n", static_cast<unsigned long long>(first.bytes), static_cast<unsigned long long>(full.bytes), static_cast<unsigned long long>(lowered.bytes), static_cast<unsigned long long>(fullLowered.bytes));
        std::puts("sparse texture levels tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
