#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
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
#include <cmath>
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
constexpr std::uint32_t Size = 4;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Srgb8 = 128;
constexpr std::uint32_t Srgb8_8 = 129;
constexpr std::uint32_t SwizzleX = 0xfacu;
constexpr std::uint32_t SwizzleYAsX = 0xfadu;
constexpr std::uint32_t FilterPoint = 0;
constexpr std::uint32_t FilterBilinear = 1;
constexpr std::array<std::uint8_t, Size> Codes{0, 255, 128, 64};
constexpr std::size_t BlockBytes = 2u << 20u;
constexpr std::uint32_t StoreSize = 1024;

alignas(256) std::array<float, Threads * 3> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 4096> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 13> SampleCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf0900108, 0x00820401, 0xbf8c3f70,
    0x34160082, 0xe0701000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) std::array<std::uint32_t, 11> StoreCode{
    0x7e280d00, 0x102828ff, 0x3d000000, 0x062a28ff, 0x41000000, 0x7e3c0300, 0x7e3e0280, 0xf0201108, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> GatherCode{
    0x1614008c, 0xe03c1000, 0x8000010a, 0xbf8c3f70, 0xf11c0108, 0x00820401, 0xbf8c3f70,
    0x34160084, 0xe0781000, 0x8001040b, 0xbf810000, 0xbf810000, 0xbf810000,
};


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
        Require(block != nullptr, "image sRGB sample copy: cannot allocate the guest block");
        Require(AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(block), BlockBytes), "image sRGB sample copy: the guest block is not write-watched");
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

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t format, std::uint32_t swizzle, const void* data = Texels.data(), std::uint32_t extent = Size) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((extent - 1u) & 3u) << 30u),
        ((extent - 1u) >> 2u) | ((extent - 1u) << 14u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(std::uint32_t filter) {
    return {0x92u, 0x00fff000u, (filter << 20u) | (filter << 22u) | (1u << 24u), 0u};
}

double Decode(std::uint32_t code) {
    const double encoded = code / 255.0;
    return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

std::uint32_t Code(std::uint32_t format, bool second, std::uint32_t x) {
    const std::uint32_t code = Codes.at(std::min(x, Size - 1u));
    return second && format == Srgb8_8 ? 255u - code : code;
}

void FillTexels(std::uint32_t format) {
    const std::uint32_t bytes = format == Srgb8 ? 1u : 2u;
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format, Size, Size, 1);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "image sRGB sample copy: the texture does not fit the texel storage");
    Texels.fill(0);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            auto* texel = &Texels[mips[0].tiledOffset + static_cast<std::uint64_t>(y) * mips[0].pitchBytes + x * bytes];
            texel[0] = static_cast<std::uint8_t>(Code(format, false, x));
            if (bytes == 2u) texel[1] = static_cast<std::uint8_t>(Code(format, true, x));
        }
    }
}

void FillInput(bool gather, bool between) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t base = tid % (Size - 1u);
        float u = (static_cast<float>(tid % Size) + 0.5f) / static_cast<float>(Size);
        if (gather) u = (static_cast<float>(base) + 0.6f) / static_cast<float>(Size);
        else if (between) u = static_cast<float>(base + 1u) / static_cast<float>(Size);
        Input[tid * 3u + 0u] = u;
        Input[tid * 3u + 1u] = 0.6f / static_cast<float>(Size);
        Input[tid * 3u + 2u] = 0.0f;
    }
}

bool Close(double actual, double expected) {
    return std::fabs(actual - expected) <= 0.01 + std::fabs(expected) * 0.04;
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, float scale) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(Srgb8, SwizzleX, texels, StoreSize);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &scale, sizeof(bits));
    StoreCode[2] = bits;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(StoreCode.data()), std::as_bytes(std::span<const std::uint32_t>(StoreCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(StoreCode.data()), std::span<const std::uint32_t>(StoreCode), 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
    device.WaitIdle();
}

void CheckStored(AgcDriver::VulkanDevice& device, std::uint8_t* texels, float scale, const std::string& what) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 3u + 0u] = (static_cast<float>(tid) + 0.5f) / static_cast<float>(StoreSize);
        Input[tid * 3u + 1u] = 0.5f / static_cast<float>(StoreSize);
        Input[tid * 3u + 2u] = 0.0f;
    }
    std::vector<std::uint32_t> userData(28, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(Srgb8, SwizzleX, texels, StoreSize);
    const auto sampler = SamplerDescriptor(FilterPoint);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(SampleCode.data()), std::as_bytes(std::span<const std::uint32_t>(SampleCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(SampleCode.data()), std::span<const std::uint32_t>(SampleCode), 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Output.fill(-1.0f);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(SampleCode.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const double code = std::round(std::min(1.0, static_cast<double>(tid) * scale) * 255.0);
        const double expected = Decode(static_cast<std::uint32_t>(code));
        Require(Close(Output[tid], expected), what + ": thread " + std::to_string(tid) + " sampled " + std::to_string(Output[tid]) + ", expected " + std::to_string(expected));
    }
}

void CheckStorageSource(AgcDriver::VulkanDevice& device) {
    GuestBlock block;
    auto* texels = block.Data();
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(Srgb8, SwizzleX, texels, StoreSize)));
    Require(surface.guestBytes >= (1u << 20u) && surface.guestBytes <= BlockBytes, "8_SRGB storage source: the surface does not fit the guest block or is below the storage image floor");
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), surface.guestBytes, nullptr, "test");
    AgcDriver::Graphics::FlushCachedTextures(device.Device());
    Store(device, texels, 1.0f / 32.0f);
    CheckStored(device, texels, 1.0f / 32.0f, "8_SRGB sample of a pending storage image");
    const auto before = AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(texels), surface.guestBytes);
    Require(before != nullptr, "8_SRGB storage source: no storage image holds the surface's results");
    const auto version = before->Version();
    Store(device, texels, 1.0f / 8.0f);
    const auto after = AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(texels), surface.guestBytes);
    Require(after != nullptr && after->Version() > version, "8_SRGB storage source: the second store did not advance the storage image");
    CheckStored(device, texels, 1.0f / 8.0f, "8_SRGB sample of a pending storage image after a second store");
}

std::string ForcedMode() {
    const char* forced = std::getenv("APS5_SRGB_SHADER_DECODE");
    return forced == nullptr ? std::string{} : std::string{forced};
}

void VKAPI_PTR NativeSrgbFormatProperties(VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
    *properties = {};
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
}

void VKAPI_PTR NoSrgbFormatProperties(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    *properties = {};
    if (format == VK_FORMAT_R8_SRGB || format == VK_FORMAT_R8G8_SRGB) return;
    properties->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
}

std::uint32_t SrgbBits() {
    return AgcDriver::Graphics::SrgbDecodeFormats(NoSrgbFormatProperties, VK_NULL_HANDLE);
}

void CheckSelection() {
    const auto mode = ForcedMode();
    Require(SrgbBits() != 0u, "a device that cannot sample R8_SRGB or R8G8_SRGB selected no sRGB decode formats");
    const auto decode = AgcDriver::Graphics::SrgbDecodeFormats(NativeSrgbFormatProperties, VK_NULL_HANDLE);
    const auto copy = AgcDriver::Graphics::SrgbCopyFormats(NativeSrgbFormatProperties, VK_NULL_HANDLE, decode);
    const std::uint32_t expected = mode == "copy" ? SrgbBits() : 0u;
    Require(copy == expected, "a device that samples R8_SRGB and R8G8_SRGB selected sRGB copy formats " + std::to_string(copy) + " with APS5_SRGB_SHADER_DECODE='" + mode + "', expected " + std::to_string(expected));
}

void Check(AgcDriver::VulkanDevice& device, std::uint32_t format, bool second, std::uint32_t filter, bool gather, const std::string& what) {
    FillTexels(format);
    const bool between = filter == FilterBilinear;
    FillInput(gather, between);
    const auto& code = gather ? GatherCode : SampleCode;
    std::vector<std::uint32_t> userData(28, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(format, second ? SwizzleYAsX : SwizzleX);
    const auto sampler = SamplerDescriptor(filter);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span<const std::uint32_t>(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), std::span<const std::uint32_t>(code), 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Output.fill(-1.0f);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t base = tid % (Size - 1u);
        if (gather) {
            const double low = Decode(Code(format, second, base));
            const double high = Decode(Code(format, second, base + 1u));
            const std::array<double, 4> expected{low, high, high, low};
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const double actual = Output[tid * 4u + component];
                Require(Close(actual, expected[component]), what + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " gathered " + std::to_string(actual) + ", expected " + std::to_string(expected[component]));
            }
            continue;
        }
        double expected = Decode(Code(format, second, tid % Size));
        if (between) expected = (Decode(Code(format, second, base)) + Decode(Code(format, second, base + 1u))) / 2.0;
        const double actual = Output[tid];
        Require(Close(actual, expected), what + ": thread " + std::to_string(tid) + " sampled " + std::to_string(actual) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        CheckSelection();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (ForcedMode() == "copy") Require((device->Target().srgbDecodeFormats & SrgbBits()) == 0u, "APS5_SRGB_SHADER_DECODE=copy: the device cannot blit R8 or RG8 to RGBA8, so 8 and 8_8 sRGB samples have no copy");
        for (const auto format : {Srgb8, Srgb8_8}) {
            const std::string name = format == Srgb8 ? "8_SRGB" : "8_8_SRGB";
            Check(*device, format, false, FilterPoint, false, name + " point sample");
            Check(*device, format, false, FilterBilinear, false, name + " bilinear sample, decoded before filtering");
            Check(*device, format, false, FilterPoint, true, name + " gather4");
            if (format == Srgb8_8) {
                Check(*device, format, true, FilterPoint, false, name + " point sample of the second channel");
                Check(*device, format, true, FilterBilinear, false, name + " bilinear sample of the second channel, decoded before filtering");
                Check(*device, format, true, FilterPoint, true, name + " gather4 of the second channel");
            }
        }
        CheckStorageSource(*device);
        std::puts("image sRGB sample copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
