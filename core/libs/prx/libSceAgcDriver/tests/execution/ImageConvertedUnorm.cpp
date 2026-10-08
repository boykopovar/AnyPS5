#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
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
constexpr std::uint32_t LoadWidth = 2048;
constexpr std::uint32_t LoadGroups = LoadWidth / Threads;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t UnormFormat = 30;
constexpr std::uint32_t UintFormat = 34;
constexpr std::uint32_t FloatFormat = 43;
constexpr std::size_t FloatOffset = 40960;
constexpr std::size_t VolumeOffset = 49152;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t SwizzleXYZ1 = 0x3acu;
constexpr std::uint32_t SwizzleXZY1 = 0x374u;
constexpr std::uint32_t SwizzleZYX1 = 0x32eu;
constexpr std::uint32_t SwizzleX011 = 0x244u;
constexpr std::uint32_t SwizzleYXZ1 = 0x3a5u;
constexpr std::uint32_t SwizzleXYZW = 0xfacu;
constexpr std::uint32_t D16 = 0x80000000u;
constexpr std::uint32_t DivisionUlps = 3;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t RoundTripOffset = 16384;
constexpr std::size_t StoreOffset = 32768;
constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};

alignas(256) std::array<std::uint32_t, LoadWidth * 4> Output{};
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};

constexpr std::array<std::uint32_t, 15> LoadCode(std::uint32_t dmask, std::uint32_t d16) {
    return {
        0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x7e3e0280u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
        0x7e1a0280u, 0xf0001008u | (dmask << 8u), 0x00010a1eu | d16, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 9> StoreCode(std::uint32_t dmask, std::uint32_t d16) {
    return {
        0x34060084u, 0xe0381000u, 0x80030a03u, 0x7e3c0300u, 0x7e3e0280u, 0xbf8c3f70u, 0xf0201008u | (dmask << 8u), 0x00010a1eu | d16,
        0xbf810000u,
    };
}

constexpr std::array<std::uint32_t, 9> SamplerCode(std::uint32_t word0) {
    return {
        0x34060084u, 0x7e280280u, 0x7e2a0280u, word0, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u,
        0xbf810000u,
    };
}

alignas(256) constexpr std::array<std::uint32_t, 10> RoundTrip{
    0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x7e3e0280u, 0xf0001708u, 0x00010a1eu, 0xbf8c3f70u, 0xf0201708u,
    0x00040a1eu, 0xbf810000u,
};

alignas(256) constexpr auto LoadXyzw = LoadCode(0xfu, 0u);
alignas(256) constexpr auto LoadXyz = LoadCode(0x7u, 0u);
alignas(256) constexpr auto LoadXy = LoadCode(0x3u, 0u);
alignas(256) constexpr auto LoadD16 = LoadCode(0xfu, D16);
alignas(256) constexpr auto StoreXyz = StoreCode(0x7u, 0u);
alignas(256) constexpr auto StoreXz = StoreCode(0x5u, 0u);
alignas(256) constexpr auto StoreD16 = StoreCode(0x7u, D16);
alignas(256) constexpr auto SampleLz = SamplerCode(0xf09c0f08u);
alignas(256) constexpr auto Gather4Lz = SamplerCode(0xf11c0108u);
alignas(256) constexpr auto GetLod = SamplerCode(0xf1800308u);
alignas(256) constexpr std::array<std::uint32_t, 13> SpreadSampleLz{
    0x34060084u, 0x7e280d00u, 0x062828ffu, 0x3e800000u, 0x102828ffu, 0x3d000000u, 0x7e2a02f0u, 0xf09c0f08u,
    0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
};
constexpr std::array<std::uint32_t, 4> WrapBilinearSampler{0x00000000u, 0x00fff000u, 0x00500000u, 0u};
constexpr std::array<std::uint32_t, 4> EdgeBilinearSampler{0x00000092u, 0x00fff000u, 0x00500000u, 0u};
constexpr std::array<std::uint32_t, 4> MirrorBilinearSampler{0x00000049u, 0x00fff000u, 0x00500000u, 0u};
constexpr std::array<std::uint32_t, 4> UnnormalizedBilinearSampler{0x00008092u, 0x00fff000u, 0x00500000u, 0u};
alignas(256) constexpr std::array<std::uint32_t, 11> TexelSpreadSampleLz{
    0x34060084u, 0x7e280d00u, 0x062828ffu, 0x3e800000u, 0x7e2a02f0u, 0xf09c0f08u, 0x00610a14u, 0xbf8c3f70u,
    0xe0781000u, 0x80000a03u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 14> VolumeSampleLz{
    0x34060084u, 0x7e280d00u, 0x062828ffu, 0x3e800000u, 0x102828ffu, 0x3d000000u, 0x7e2a02f0u, 0x7e2c02f0u,
    0xf09c0f10u, 0x00610a14u, 0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
};
constexpr std::array<std::uint32_t, 4> HalfBorderBilinearSampler{0x00000124u, 0x00fff000u, 0x00500000u, 0x80000000u};
constexpr std::array<std::uint32_t, 4> MirrorOnceSampler{0x000000dbu, 0x00fff000u, 0u, 0u};

constexpr std::array<std::uint32_t, 11> StoreValues{
    0x00000000u, 0x3f800000u, 0x3f000000u, 0x3eaaaaabu, 0x80000000u, 0xbf800000u,
    0x40000000u, 0x7f800000u, 0x7fc00000u, 0x000116c2u, 0x3f7fbe77u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image converted unorm: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle, std::uint32_t width) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        (width - 1u) >> 2u,
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::uint32_t LoadTexel(std::uint32_t index) {
    return index | ((2047u - index) << 11u) | ((index & 1023u) << 22u);
}

std::uint32_t Field(std::uint32_t texel, std::uint32_t component) {
    return component == 2u ? texel >> 22u : (texel >> (component * 11u)) & 2047u;
}

struct Expected {
    std::uint32_t bits;
    std::uint32_t ulps;
};

Expected Selected(std::uint32_t texel, std::uint32_t swizzle, std::uint32_t component, bool unorm) {
    const auto selector = (swizzle >> (component * 3u)) & 7u;
    if (selector == 0u) return {0u, 0u};
    if (selector == 1u) return {unorm ? 0x3f800000u : 1u, 0u};
    const auto field = Field(texel, selector - 4u);
    const auto maximum = selector == 6u ? 1023u : 2047u;
    if (!unorm) return {field, 0u};
    return {std::bit_cast<std::uint32_t>(static_cast<float>(static_cast<double>(field) / maximum)), field == 0u || field == maximum ? 0u : DivisionUlps};
}

std::uint32_t StoreField(std::uint32_t bits, std::uint32_t maximum) {
    const auto value = std::bit_cast<float>(bits);
    if (!(value > 0.0f)) return 0u;
    if (value >= 1.0f) return maximum;
    return static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(value) * maximum));
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, const std::array<std::uint32_t, 4>& extra, std::uint32_t groups, const std::array<std::uint32_t, 8>& target = {}) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(24, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(extra.begin(), extra.end(), userData.begin() + 12);
    std::copy(target.begin(), target.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckLoad(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::span<const std::uint32_t> code, std::uint32_t dmask, std::uint32_t format, std::uint32_t swizzle, const std::string& what) {
    Run(device, code, TextureDescriptor(texels, format, swizzle, LoadWidth), {}, LoadGroups);
    for (std::uint32_t index = 0; index < LoadWidth; ++index) {
        std::uint32_t slot = 0;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            if (((dmask >> component) & 1u) == 0u) continue;
            const auto expected = Selected(LoadTexel(index), swizzle, component, format == UnormFormat);
            const auto actual = Output[index * 4u + slot++];
            const auto distance = actual > expected.bits ? actual - expected.bits : expected.bits - actual;
            Require(distance <= expected.ulps, what + ": texel " + std::to_string(index) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(expected.bits) + " within " + std::to_string(expected.ulps) + " ULP");
        }
    }
}

void CheckRoundTrip(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    auto* target = texels + RoundTripOffset;
    std::fill(target, target + LoadWidth * 4u, static_cast<std::uint8_t>(0xa5u));
    Run(device, RoundTrip, TextureDescriptor(texels, UnormFormat, SwizzleXYZ1, LoadWidth), {}, LoadGroups, TextureDescriptor(target, UnormFormat, SwizzleXYZ1, LoadWidth));
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(target), LoadWidth * 4u, nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t index = 0; index < LoadWidth; ++index) {
        std::uint32_t actual = 0;
        std::memcpy(&actual, target + index * 4u, 4u);
        Require(actual == LoadTexel(index), "image_load then image_store: texel " + std::to_string(index) + " is " + Hex(actual) + ", expected " + Hex(LoadTexel(index)));
    }
}

void CheckStore(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::span<const std::uint32_t> code, std::uint32_t dmask, const std::string& what) {
    std::fill(texels, texels + Threads * 4u, static_cast<std::uint8_t>(0xa5u));
    Input.fill(0u);
    std::array<std::uint32_t, Threads> expected{};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 3> values{StoreValues[tid % 11u], StoreValues[(tid + 4u) % 11u], StoreValues[(tid + 8u) % 11u]};
        std::uint32_t slot = 0;
        for (std::uint32_t component = 0; component < 3u; ++component) {
            if (((dmask >> component) & 1u) == 0u) continue;
            Input[tid * 4u + slot++] = values[component];
            expected[tid] |= StoreField(values[component], component == 2u ? 1023u : 2047u) << (component * 11u);
        }
    }
    Run(device, code, TextureDescriptor(texels, UnormFormat, SwizzleXYZ1, Threads), BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)), 1);
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), Threads * 4u, nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::uint32_t actual = 0;
        std::memcpy(&actual, texels + tid * 4u, 4u);
        Require(actual == expected[tid], what + ": texel " + std::to_string(tid) + " is " + Hex(actual) + ", expected " + Hex(expected[tid]));
    }
}

constexpr std::array<float, 11> FloatValues{0.0f, 1.0f, 0.5f, 2.0f, 0.25f, 3.0f, 1.5f, 10.0f, 0.75f, 6.5f, 0.125f};

std::uint32_t SmallFloat(float value, std::uint32_t mantissaBits) {
    if (value == 0.0f) return 0u;
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto exponent = ((bits >> 23u) & 0xffu) - 127u + 15u;
    return (exponent << mantissaBits) | ((bits & 0x7fffffu) >> (23u - mantissaBits));
}

std::uint32_t FloatTexel(std::uint32_t index) {
    return SmallFloat(FloatValues[index % 11u], 5u) | (SmallFloat(FloatValues[(index + 4u) % 11u], 6u) << 10u) | (SmallFloat(FloatValues[(index + 8u) % 11u], 6u) << 21u);
}

void CheckFloatLoad(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    for (std::uint32_t index = 0; index < LoadWidth; ++index) {
        const auto texel = FloatTexel(index);
        std::memcpy(texels + index * 4u, &texel, 4u);
    }
    Run(device, LoadXyzw, TextureDescriptor(texels, FloatFormat, SwizzleXYZ1, LoadWidth), {}, LoadGroups);
    for (std::uint32_t index = 0; index < LoadWidth; ++index) {
        const std::array<float, 4> expected{FloatValues[index % 11u], FloatValues[(index + 4u) % 11u], FloatValues[(index + 8u) % 11u], 1.0f};
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto actual = Output[index * 4u + component];
            Require(actual == std::bit_cast<std::uint32_t>(expected[component]), "image_load of R10_G11_B11_FLOAT: texel " + std::to_string(index) + " component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(std::bit_cast<std::uint32_t>(expected[component])));
        }
    }
}

void CheckFloatStore(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    std::fill(texels, texels + Threads * 4u, static_cast<std::uint8_t>(0xa5u));
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(FloatValues[tid % 11u]);
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(FloatValues[(tid + 4u) % 11u]);
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(FloatValues[(tid + 8u) % 11u]);
    }
    Run(device, StoreXyz, TextureDescriptor(texels, FloatFormat, SwizzleXYZ1, Threads), BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)), 1);
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), Threads * 4u, nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::uint32_t actual = 0;
        std::memcpy(&actual, texels + tid * 4u, 4u);
        Require(actual == FloatTexel(tid), "image_store of R10_G11_B11_FLOAT: texel " + std::to_string(tid) + " is " + Hex(actual) + ", expected " + Hex(FloatTexel(tid)));
    }
}

void CheckFloatSample(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const std::array<std::uint32_t, 4>& sampler, bool linear, bool wrap, const std::string& what, bool whiteBorder = false, std::span<const std::uint32_t> code = SpreadSampleLz) {
    for (std::uint32_t index = 0; index < Threads; ++index) {
        const auto texel = FloatTexel(index);
        std::memcpy(texels + index * 4u, &texel, 4u);
    }
    Run(device, code, TextureDescriptor(texels, FloatFormat, SwizzleXYZ1, Threads), sampler, 1);
    const auto value = [](std::int32_t index, std::uint32_t component) {
        return component == 3u ? 1.0f : FloatValues[(static_cast<std::uint32_t>(index) + component * 4u) % 11u];
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            float expected = value(static_cast<std::int32_t>(tid), component);
            if (linear) {
                const auto left = tid == 0u ? (wrap ? static_cast<std::int32_t>(Threads) - 1 : 0) : static_cast<std::int32_t>(tid) - 1;
                const float leftValue = tid == 0u && whiteBorder ? 1.0f : value(left, component);
                expected = leftValue * 0.25f + value(static_cast<std::int32_t>(tid), component) * 0.75f;
            }
            const float actual = std::bit_cast<float>(Output[tid * 4u + component]);
            Require(std::fabs(actual - expected) <= 1e-5f * std::max(1.0f, std::fabs(expected)), what + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void CheckFloatVolumeSample(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    auto texture = TextureDescriptor(texels, FloatFormat, SwizzleXYZ1, Threads);
    texture[3] = (texture[3] & ~(0xfu << 28u)) | (Type3D << 28u);
    texture[4] = 1u;
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(texture));
    const auto& mip = geometry.mips.at(0);
    const auto value = [](std::uint32_t slice, std::int32_t index, std::uint32_t component) {
        return component == 3u ? 1.0f : FloatValues[(static_cast<std::uint32_t>(index) + component * 4u + slice * 2u) % 11u];
    };
    for (std::uint32_t slice = 0; slice < 2u; ++slice) {
        for (std::uint32_t index = 0; index < Threads; ++index) {
            const auto texel = SmallFloat(value(slice, static_cast<std::int32_t>(index), 0u), 5u) | (SmallFloat(value(slice, static_cast<std::int32_t>(index), 1u), 6u) << 10u) | (SmallFloat(value(slice, static_cast<std::int32_t>(index), 2u), 6u) << 21u);
            std::memcpy(texels + geometry.GuestLayerOffset(slice) + mip.tiledOffset + index * 4u, &texel, 4u);
        }
    }
    Run(device, VolumeSampleLz, texture, EdgeBilinearSampler, 1);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto left = tid == 0u ? 0 : static_cast<std::int32_t>(tid) - 1;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            float expected = 0.0f;
            for (std::uint32_t slice = 0; slice < 2u; ++slice) expected += 0.5f * (value(slice, left, component) * 0.25f + value(slice, static_cast<std::int32_t>(tid), component) * 0.75f);
            const float actual = std::bit_cast<float>(Output[tid * 4u + component]);
            Require(std::fabs(actual - expected) <= 1e-5f * std::max(1.0f, std::fabs(expected)), "image_sample_lz 3d of R10_G11_B11_FLOAT, bilinear between two slices: thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::uint8_t* texels, std::uint32_t swizzle, const std::array<std::uint32_t, 4>& extra, const std::string& reason, const std::string& what, std::uint32_t format = UnormFormat) {
    std::string refusal;
    try {
        Run(device, code, TextureDescriptor(texels, format, swizzle, LoadWidth), extra, 1);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(reason) != std::string::npos, what + " of a converted image was not refused: " + refusal);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        for (std::uint32_t index = 0; index < LoadWidth; ++index) {
            const auto texel = LoadTexel(index);
            std::memcpy(texels + index * 4u, &texel, 4u);
        }
        CheckLoad(*device, texels, LoadXyzw, 0xfu, UnormFormat, SwizzleXYZ1, "image_load dmask:0xf X Y Z 1");
        CheckLoad(*device, texels, LoadXy, 0x3u, UnormFormat, SwizzleXZY1, "image_load dmask:0x3 X Z Y 1");
        CheckLoad(*device, texels, LoadXyzw, 0xfu, UnormFormat, SwizzleZYX1, "image_load dmask:0xf Z Y X 1");
        CheckLoad(*device, texels, LoadXyzw, 0xfu, UnormFormat, SwizzleX011, "image_load dmask:0xf X 0 1 1");
        CheckLoad(*device, texels, LoadXyz, 0x7u, UintFormat, SwizzleYXZ1, "image_load of 10_11_11_UINT dmask:0x7 Y X Z 1");
        CheckRoundTrip(*device, texels);
        CheckStore(*device, texels + StoreOffset, StoreXyz, 0x7u, "image_store dmask:0x7");
        CheckStore(*device, texels + StoreOffset, StoreXz, 0x5u, "image_store dmask:0x5");
        CheckFloatStore(*device, texels + FloatOffset);
        CheckFloatLoad(*device, texels + FloatOffset);
        CheckFloatSample(*device, texels + FloatOffset, PointSampler, false, false, "image_sample_lz of R10_G11_B11_FLOAT, point, clamp to edge");
        CheckFloatSample(*device, texels + FloatOffset, EdgeBilinearSampler, true, false, "image_sample_lz of R10_G11_B11_FLOAT, bilinear, clamp to edge");
        CheckFloatSample(*device, texels + FloatOffset, WrapBilinearSampler, true, true, "image_sample_lz of R10_G11_B11_FLOAT, bilinear, wrap");
        RequireRefused(*device, Gather4Lz, texels + FloatOffset, SwizzleXYZ1, PointSampler, "samples or gathers a converted float image", "image_gather4_lz of R10_G11_B11_FLOAT", FloatFormat);
        CheckFloatSample(*device, texels + FloatOffset, MirrorBilinearSampler, true, false, "image_sample_lz of R10_G11_B11_FLOAT, bilinear, mirror");
        CheckFloatSample(*device, texels + FloatOffset, HalfBorderBilinearSampler, true, false, "image_sample_lz of R10_G11_B11_FLOAT, bilinear, half border, white", true);
        CheckFloatSample(*device, texels + FloatOffset, UnnormalizedBilinearSampler, true, false, "image_sample_lz of R10_G11_B11_FLOAT, bilinear, unnormalized coordinates", false, TexelSpreadSampleLz);
        CheckFloatVolumeSample(*device, texels + VolumeOffset);
        RequireRefused(*device, SampleLz, texels, SwizzleXYZ1, MirrorOnceSampler, "clamp-to-half-border or clamp-to-border addressing", "image_sample_lz with mirror-once addressing");
        RequireRefused(*device, Gather4Lz, texels, SwizzleXYZ1, PointSampler, "samples or gathers a converted unorm image", "image_gather4_lz");
        RequireRefused(*device, GetLod, texels, SwizzleXYZ1, PointSampler, "queries the level of detail of a converted unorm image", "image_get_lod");
        RequireRefused(*device, LoadD16, texels, SwizzleXYZ1, {}, "converted unorm image with 16-bit data", "image_load d16");
        RequireRefused(*device, StoreD16, texels + StoreOffset, SwizzleXYZ1, BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)), "converted unorm image with 16-bit data", "image_store d16");
        RequireRefused(*device, LoadXyzw, texels, SwizzleXYZW, {}, "selects a channel the converted image format does not have", "image_load with DST_SEL X Y Z W");
        RequireRefused(*device, StoreXyz, texels + StoreOffset, SwizzleXYZW, BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)), "selects a channel the converted image format does not have", "image_store with DST_SEL X Y Z W");
        std::puts("image converted unorm tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
