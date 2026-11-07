#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
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
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint64_t Address = 0x10000000ull;
constexpr std::uint32_t FormatBc1Unorm = 169;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileS64KB = 0x09;
constexpr std::uint32_t Type2D = 9;

struct Shape {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t maxMip;
    std::uint32_t lastLevel;
    std::uint32_t minLod = 0;
};

struct Expected {
    std::uint32_t levels;
    std::uint32_t width;
    std::uint32_t height;
};

std::array<std::uint32_t, 8> Descriptor(const Shape& shape) {
    return {
        static_cast<std::uint32_t>(Address >> 8u),
        static_cast<std::uint32_t>((Address >> 40u) & 0xffu) | (shape.minLod << 8u) | (shape.format << 20u) | (((shape.width - 1u) & 3u) << 30u),
        ((shape.width - 1u) >> 2u) | ((shape.height - 1u) << 14u),
        0xfacu | (shape.lastLevel << 16u) | (TileS64KB << 20u) | (Type2D << 28u),
        0u,
        shape.maxMip << 4u,
        0u,
        0u,
    };
}

struct Made {
    std::uint32_t levels;
    VkExtent3D extent;
    VkDeviceSize bytes;
    float minLod;
};

Made Make(const Context& context, TextureDetiler& detiler, const Shape& shape) {
    const auto words = Descriptor(shape);
    const auto descriptor = DecodeTextureResource(words);
    const std::vector<std::byte> snapshot(static_cast<std::size_t>(DescribeSurface(descriptor).guestBytes));
    Texture texture(context, detiler, descriptor, VkComponentMapping{}, snapshot);
    return {texture.SampledViewRange(false).levels, texture.ImageExtent(), texture.AllocationBytes(), texture.ViewMinLod()};
}

std::string Text(const Shape& shape) {
    return std::to_string(shape.width) + "x" + std::to_string(shape.height) + " format " + std::to_string(shape.format) + " levels 0.." + std::to_string(shape.lastLevel) + " of " + std::to_string(shape.maxMip + 1u);
}

Made Check(const Context& context, TextureDetiler& detiler, std::uint32_t n, const Shape& shape, const Expected& expected) {
    const auto made = Make(context, detiler, shape);
    const auto what = "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ", " + Text(shape) + ": ";
    Require(made.levels == expected.levels, what + "the view has " + std::to_string(made.levels) + " levels, expected " + std::to_string(expected.levels));
    Require(made.extent.width == expected.width && made.extent.height == expected.height && made.extent.depth == 1u, what + "the image is " + std::to_string(made.extent.width) + "x" + std::to_string(made.extent.height) + "x" + std::to_string(made.extent.depth) + ", expected " + std::to_string(expected.width) + "x" + std::to_string(expected.height) + "x1");
    return made;
}

void Run(const Context& context, TextureDetiler& detiler, std::uint32_t n) {
    const auto side = 1024u >> n;
    const auto dropped = Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 10}, {11u - n, side, side});
    const auto reference = Check(context, detiler, n, {FormatBc1Unorm, side, side, 10u - n, 10u - n}, {11u - n, side, side});
    Require(dropped.bytes == reference.bytes, "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ": a downscaled 1024x1024 BC1 texture holds " + std::to_string(dropped.bytes) + " bytes, a " + std::to_string(side) + "x" + std::to_string(side) + " one below the minimum " + std::to_string(reference.bytes));

    Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 1}, {1, 512, 512});
    Check(context, detiler, n, {FormatBc1Unorm, 1024, 8, 10, 10}, n == 1u ? Expected{10, 512, 4} : Expected{11, 1024, 8});

    Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 0, 0}, {1, 1024, 1024});
    Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 0}, {1, 1024, 1024});
    Check(context, detiler, n, {Format8888Unorm, 1024, 1024, 10, 10}, {11, 1024, 1024});

    if (context.imageViewMinLod) {
        const auto clamped = Check(context, detiler, n, {FormatBc1Unorm, 1024, 1024, 10, 10, 10u * 256u}, {11u - n, side, side});
        Require(clamped.minLod == static_cast<float>(10u - n), "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ": a minimum LOD clamp of 10.0 on 11 levels became " + std::to_string(clamped.minLod) + " on the view, expected " + std::to_string(10u - n));
    }
}

namespace sampling {

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

using MipChain = std::vector<AgcDriver::Graphics::TileMipLayout>;

const MipChain& Mips() {
    static const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, FormatBc1Unorm, Size, Size, Levels);
    return mips;
}

std::size_t Padding(const MipChain& mips) {
    const auto unmapped = static_cast<std::size_t>(mips[MinLodLevel - 1u].tiledOffset);
    return (PageBytes - unmapped % PageBytes) % PageBytes;
}

std::size_t BlockBytes(const MipChain& mips) {
    const auto bytes = Padding(mips) + static_cast<std::size_t>(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1));
    return (bytes + BlockAlignment - 1u) / BlockAlignment * BlockAlignment;
}

std::size_t ResidentBytes(const MipChain& mips) {
    return Padding(mips) + static_cast<std::size_t>(mips[MinLodLevel - 1u].tiledOffset);
}

class GuestBlock {
public:
    GuestBlock(const MipChain& mips, std::size_t committed) : bytes(BlockBytes(mips)), committed(committed), padding(Padding(mips)) {
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
            Require(mprotect(block, committed, PROT_READ | PROT_WRITE) == 0, "texture downscale: cannot commit the resident levels");
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, committed);
        }
#endif
        Require(block != nullptr, "texture downscale: cannot allocate the guest block");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block), bytes);
        GuestAllocations::Mutation().Add(block, committed, true, true, true);
    }

    void Commit(std::size_t through) {
        Require(through > committed && through <= bytes && through % PageBytes == 0, "texture downscale: the newly committed range does not end on a page past the committed one");
        const auto added = through - committed;
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(block + committed, added, PAGE_READWRITE, added);
#else
        Require(mprotect(block + committed, added, PROT_READ | PROT_WRITE) == 0, "texture downscale: cannot commit a streamed level");
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

    std::uint8_t* Texels() { return block + padding; }

private:
    std::uint8_t* block = nullptr;
    std::size_t bytes = 0;
    std::size_t committed = 0;
    std::size_t padding = 0;
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

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t minLodLevel, std::uint32_t format = FormatBc1Unorm) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | ((minLodLevel * 256u) << 8u) | (format << 20u) | (((Size - 1u) & 3u) << 30u),
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

void Check(const std::string& surface, std::uint32_t n, std::uint32_t minLodLevel) {
    constexpr std::array<const char*, 4> names{"image_sample_d_cl", "image_sample_cl", "image_sample_d_cl_o (1, 2)", "image_sample_d_cl_o (-1, -2)"};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sample = SampleOf(tid);
        const auto clamped = std::max({sample.level, sample.clamp + n, minLodLevel});
        const std::array<std::uint32_t, 4> levels{clamped, std::max(sample.clamp + n, minLodLevel), clamped, clamped};
        for (std::uint32_t index = 0; index < levels.size(); ++index) {
            const float red = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u]) * 31.0f;
            const float alpha = std::bit_cast<float>(Buffer[tid * Words + ResultWord + index * 4u + 3u]);
            const auto expected = LevelRed(levels[index]);
            Require(std::lround(red) == static_cast<long>(expected) && std::lround(alpha) == 1, "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ", " + surface + ": " + names[index] + " in thread " + std::to_string(tid) + " (level " + std::to_string(sample.level) + ", clamp " + std::to_string(sample.clamp) + ") read red " + std::to_string(red) + " and alpha " + std::to_string(alpha) + ", expected guest level " + std::to_string(levels[index]) + " (red " + std::to_string(expected) + ")");
        }
    }
}

std::array<std::uint32_t, Threads * Words> Sampled(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::uint32_t n, std::uint32_t minLodLevel, const std::string& surface) {
    FillInput();
    Dispatch(device, texels, minLodLevel);
    Check(surface + " (MIN_LOD " + std::to_string(minLodLevel) + ")", n, minLodLevel);
    return Buffer;
}

bool SparseBc1(const AgcDriver::Graphics::Context& context) {
    if (!context.sparseResidencyImage2D || context.sparseImageFormatProperties == nullptr) return false;
    std::uint32_t count = 0;
    context.sparseImageFormatProperties(context.physical, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_IMAGE_TYPE_2D, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, &count, nullptr);
    return count != 0;
}

void Run(AgcDriver::VulkanDevice& device, const Context& context, std::uint32_t n) {
    GuestBlock whole(Mips(), BlockBytes(Mips()));
    FillLevels(whole.Texels(), 0);
    Sampled(device, whole.Texels(), n, 0u, "every level mapped");
    const auto clamped = Sampled(device, whole.Texels(), n, MinLodLevel, "every level mapped");
    if (!context.imageViewMinLod || !SparseBc1(context)) {
        std::puts("texture downscale: no sparse residency for 2D BC1 images or VK_EXT_image_view_min_lod, the unmapped-levels case is not run");
        return;
    }
    GuestBlock streamed(Mips(), ResidentBytes(Mips()));
    FillLevels(streamed.Texels(), MinLodLevel);
    const auto unmapped = Sampled(device, streamed.Texels(), n, MinLodLevel, "levels above the minimum LOD unmapped");
    Require(unmapped == clamped, "APS5_TEXTURE_DOWNSCALE=" + std::to_string(n) + ": the texture with levels 0 and 1 unmapped samples differently from the one with every level mapped (MIN_LOD 2)");
}

}

void Refused(const Context& context, TextureDetiler& detiler, std::string_view name) {
    try {
        Make(context, detiler, {FormatBc1Unorm, 1024, 1024, 10, 10});
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(name) != std::string_view::npos, std::string("the refusal does not name ") + std::string(name) + ": " + error.what());
        return;
    }
    throw std::runtime_error(std::string("an invalid ") + std::string(name) + " made a texture");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    std::uint32_t n = 1;
    if (mode == "--levels" && argc > 2) n = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10));
    if (mode == "--refuse-levels") {
        SetEnvironment("APS5_TEXTURE_DOWNSCALE", "-1");
    } else if (mode == "--refuse-minimum") {
        SetEnvironment("APS5_TEXTURE_DOWNSCALE", "1");
        SetEnvironment("APS5_TEXTURE_DOWNSCALE_MIN", "large");
    } else {
        SetEnvironment("APS5_TEXTURE_DOWNSCALE", std::to_string(n).c_str());
    }
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto context = device->graphicsContext();
        if (!context.textureCompressionBC) {
            std::puts("skipped, the device has no BC texture compression");
            return VulkanTestSkipped;
        }
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            if (mode == "--refuse-levels") {
                Refused(context, detiler, "APS5_TEXTURE_DOWNSCALE");
            } else if (mode == "--refuse-minimum") {
                Refused(context, detiler, "APS5_TEXTURE_DOWNSCALE_MIN");
            } else {
                Run(context, detiler, n);
            }
            if (auto* recorder = Recorder::Active()) recorder->Sync();
        }
        if (mode.empty() || mode == "--levels") sampling::Run(*device, context, n);
        std::printf("texture downscale tests passed (%s)\n", mode.empty() ? "--levels 1" : (mode + (mode == "--levels" ? " " + std::to_string(n) : "")).c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
