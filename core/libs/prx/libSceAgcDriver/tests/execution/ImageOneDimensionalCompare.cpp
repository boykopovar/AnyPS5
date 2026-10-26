#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t Size = 16;
constexpr std::uint32_t Layers = 3;
constexpr std::uint32_t Format16UNorm = 7;
constexpr std::uint32_t Type1D = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type1DArray = 12;
constexpr std::uint32_t ReferenceWord = 6;
constexpr std::uint32_t Result = 8;
constexpr std::uint32_t Step1D = 4000;
constexpr std::uint32_t StepArray = 1200;
constexpr std::uint32_t Step2D = 200;
constexpr std::uint32_t Row2D = 2;
constexpr std::uint32_t CompareLess = 1;

alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 16384> Line{};
alignas(4096) std::array<std::uint8_t, 32768> ArrayTexels{};
alignas(4096) std::array<std::uint8_t, 32768> PlaneTexels{};

alignas(256) constexpr std::array<std::uint32_t, 16> Code1D_Lz{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0xf0bc0100, 0x00610c14, 0xbf8c3f70, 0xe0701020,
    0x80000c01, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> Code1D_ImplicitLod{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0xf0a00100, 0x00610c14, 0xbf8c3f70, 0xe0701020,
    0x80000c01, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code1D_ExplicitLod{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0280, 0xf0b00100, 0x00610c14, 0xbf8c3f70,
    0xe0701020, 0x80000c01, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 18> Code1D_Lz_Offset{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e2802ff, 0x00000f00, 0x7e2a0308, 0x7e2c0302, 0xf0fc0100, 0x00610c14,
    0xbf8c3f70, 0xe0701020, 0x80000c01, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 19> Code1DArray_Lz_Offset{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e2802ff, 0x00000f00, 0x7e2a0308, 0x7e2c0302, 0x7e2e0303, 0xf0fc0120, 0x00610c14,
    0xbf8c3f70, 0xe0701020, 0x80000c01, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code1DArray_Lz{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf0bc0120, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code1DArray_ImplicitLod{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf0a00120, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code2D_Lz{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf0bc0108, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code2D_ImplicitLod{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf0a00108, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> Code1D_Gather_C_Lz{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0xf13c0100, 0x00610c14, 0xbf8c3f70, 0xe0701020,
    0x80000c01, 0xbf810000, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> Code1DArray_Gather_C_Lz{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301018, 0x80000801, 0xbf8c3f70,
    0x7e280308, 0x7e2a0302, 0x7e2c0303, 0xf13c0120, 0x00610c14, 0xbf8c3f70, 0xe0701020, 0x80000c01,
    0xbf810000,
};

std::uint32_t TexelOf(std::uint32_t tid) {
    return (tid * 7u + 3u) % Size;
}

bool ReferenceBelow(std::uint32_t tid) {
    return (tid & 1u) == 0u;
}

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void FillInput1D() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = TexelOf(tid);
        const float texel = static_cast<float>((x + 1u) * Step1D) / 65535.0f;
        const float half = static_cast<float>(Step1D / 2u) / 65535.0f;
        Buffer[tid * Words] = Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(Size));
        Buffer[tid * Words + ReferenceWord] = Bits(ReferenceBelow(tid) ? texel - half : texel + half);
    }
}

void FillInput1DArray() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = TexelOf(tid);
        const auto layer = (tid / 7u) % Layers;
        const float texel = static_cast<float>((layer * Size + x + 1u) * StepArray) / 65535.0f;
        const float half = static_cast<float>(StepArray / 2u) / 65535.0f;
        Buffer[tid * Words] = Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(Size));
        Buffer[tid * Words + 1u] = Bits(static_cast<float>(layer));
        Buffer[tid * Words + ReferenceWord] = Bits(ReferenceBelow(tid) ? texel - half : texel + half);
    }
}

void FillInput2D() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = TexelOf(tid);
        const float texel = static_cast<float>((Row2D * Size + x + 1u) * Step2D) / 65535.0f;
        const float half = static_cast<float>(Step2D / 2u) / 65535.0f;
        Buffer[tid * Words] = Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(Size));
        Buffer[tid * Words + 1u] = Bits((static_cast<float>(Row2D) + 0.5f) / static_cast<float>(Size));
        Buffer[tid * Words + ReferenceWord] = Bits(ReferenceBelow(tid) ? texel - half : texel + half);
    }
}

void FillInputArrayFirstLayer() {
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto x = TexelOf(tid);
        const float texel = static_cast<float>((0u * Size + x + 1u) * StepArray) / 65535.0f;
        const float half = static_cast<float>(StepArray / 2u) / 65535.0f;
        Buffer[tid * Words] = Bits((static_cast<float>(x) + 0.5f) / static_cast<float>(Size));
        Buffer[tid * Words + ReferenceWord] = Bits(ReferenceBelow(tid) ? texel - half : texel + half);
    }
}

std::array<std::uint32_t, 8> LineDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Line.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format16UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        (Size - 1u) >> 2u,
        0xfacu | (Type1D << 28u),
        0u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 8> ArrayDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ArrayTexels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format16UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        (Size - 1u) >> 2u,
        0xfacu | (Type1DArray << 28u),
        Layers - 1u, 0u, 0u, 0u,
    };
}

std::array<std::uint32_t, 8> PlaneDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(PlaneTexels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format16UNorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void FillLine() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(LineDescriptor()));
    Require(geometry.guestBytes <= Line.size(), "1D texture does not fit texel storage");
    std::fill(Line.begin(), Line.end(), 0xeeu);
    for (std::uint32_t x = 0; x < Size; ++x) {
        const auto value = static_cast<std::uint16_t>((x + 1u) * Step1D);
        auto* texel = &Line[geometry.GuestLayerOffset(0) + geometry.mips.at(0).tiledOffset + x * 2u];
        texel[0] = static_cast<std::uint8_t>(value);
        texel[1] = static_cast<std::uint8_t>(value >> 8u);
    }
}

void FillArray() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(ArrayDescriptor()));
    Require(geometry.guestBytes <= ArrayTexels.size(), "1D array texture does not fit texel storage");
    std::fill(ArrayTexels.begin(), ArrayTexels.end(), 0xeeu);
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto value = static_cast<std::uint16_t>((layer * Size + x + 1u) * StepArray);
            auto* texel = &ArrayTexels[geometry.GuestLayerOffset(layer) + geometry.mips.at(0).tiledOffset + x * 2u];
            texel[0] = static_cast<std::uint8_t>(value);
            texel[1] = static_cast<std::uint8_t>(value >> 8u);
        }
    }
}

void FillPlane() {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(PlaneDescriptor()));
    Require(geometry.guestBytes <= PlaneTexels.size(), "2D texture does not fit texel storage");
    std::fill(PlaneTexels.begin(), PlaneTexels.end(), 0xeeu);
    for (std::uint32_t y = 0; y < Size; ++y) {
        for (std::uint32_t x = 0; x < Size; ++x) {
            const auto value = static_cast<std::uint16_t>((y * Size + x + 1u) * Step2D);
            auto* texel = &PlaneTexels[geometry.GuestLayerOffset(0) + geometry.mips.at(0).tiledOffset + static_cast<std::uint64_t>(y) * geometry.mips.at(0).pitchBytes + x * 2u];
            texel[0] = static_cast<std::uint8_t>(value);
            texel[1] = static_cast<std::uint8_t>(value >> 8u);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 4> SamplerDescriptor() {
    return {CompareLess << 12u, 0xfffu << 12u, 1u << 26u, 0u};
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& textureDesc) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(textureDesc.begin(), textureDesc.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
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

void Check(const std::string& op) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float value = std::bit_cast<float>(Buffer[tid * Words + Result]);
        const float expected = ReferenceBelow(tid) ? 1.0f : 0.0f;
        Require(value == expected, op + ": thread " + std::to_string(tid) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
    }
}

void ExpectGatherReject(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& textureDesc) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(sizeof(Buffer)));
    const auto sampler = SamplerDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(textureDesc.begin(), textureDesc.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    try {
        (void)ShaderRecompiler::Recompile(request);
        Require(false, "remapped 1D depth gather was accepted unexpectedly");
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find("has an unsupported 1D gather variant") != std::string::npos, "unexpected error for 1D depth gather: " + std::string(error.what()));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillLine();
        FillArray();
        FillPlane();

        FillInput1D();
        Run(*device, Code1D_Lz, LineDescriptor());
        Check("1D image_sample_c_lz");

        FillInput1D();
        Run(*device, Code1D_ImplicitLod, LineDescriptor());
        Check("1D image_sample_c");

        FillInput1D();
        Run(*device, Code1D_ExplicitLod, LineDescriptor());
        Check("1D image_sample_c_l");

        FillInput1D();
        Run(*device, Code1D_Lz_Offset, LineDescriptor());
        Check("1D image_sample_c_lz_o");

        FillInput1DArray();
        Run(*device, Code1DArray_Lz, ArrayDescriptor());
        Check("1D array image_sample_c_lz");

        FillInput1DArray();
        Run(*device, Code1DArray_Lz_Offset, ArrayDescriptor());
        Check("1D array image_sample_c_lz_o");

        FillInput1DArray();
        Run(*device, Code1DArray_ImplicitLod, ArrayDescriptor());
        Check("1D array image_sample_c");

        FillInputArrayFirstLayer();
        Run(*device, Code1D_Lz, ArrayDescriptor());
        Check("1D image_sample_c_lz on 1D array first layer");

        FillInputArrayFirstLayer();
        Run(*device, Code1D_ImplicitLod, ArrayDescriptor());
        Check("1D image_sample_c on 1D array first layer");

        FillInput2D();
        Run(*device, Code2D_Lz, PlaneDescriptor());
        Check("2D image_sample_c_lz");

        FillInput2D();
        Run(*device, Code2D_ImplicitLod, PlaneDescriptor());
        Check("2D image_sample_c");

        ExpectGatherReject(*device, Code1D_Gather_C_Lz, LineDescriptor());
        ExpectGatherReject(*device, Code1DArray_Gather_C_Lz, ArrayDescriptor());

        std::puts("1D, 1D-array, and 2D comparison tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
