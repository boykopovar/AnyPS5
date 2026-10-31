#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourcePlan.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
constexpr std::uint32_t Width = 8;
constexpr std::uint32_t MaxGuestLayers = 8192;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type1DArray = 12;
constexpr std::uint32_t Wrap = 0;
constexpr std::uint32_t ClampEdge = 2;
constexpr std::uint32_t Bilinear = (1u << 20u) | (1u << 22u);

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 8> Output{};
constexpr std::size_t TexelBytes = 4u << 20u;
constexpr std::size_t TexelAlignment = 65536;
std::array<std::uint8_t, TexelBytes + TexelAlignment - 1> TexelStorage{};
const std::span<std::uint8_t, TexelBytes> Texels{reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(TexelStorage.data()) + TexelAlignment - 1) & ~(TexelAlignment - 1)), TexelBytes};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleLevelZero{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf09c0f20, 0x00820602, 0x34020085, 0xbf8c3f70,
    0xe0781000, 0x80010601, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> SampleLevel{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf0900f20, 0x00820602, 0x34020085, 0xbf8c3f70,
    0xe0781000, 0x80010601, 0xbf810000,
};

std::uint32_t Layers = 0;

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint32_t baseArray) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (Type1DArray << 28u),
        (Layers - 1u) | (baseArray << 16u), 0u, 0u, 0u,
    };
}

std::array<std::uint8_t, 4> Texel(std::uint32_t x, std::uint32_t layer) {
    return {static_cast<std::uint8_t>(x * 30u + layer % 5u), static_cast<std::uint8_t>((layer & 1u) != 0u ? 255u : 0u), static_cast<std::uint8_t>(layer), 0xff};
}

void FillTexels() {
    std::ranges::fill(Texels, std::uint8_t{0});
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(0)));
    const auto& mip = surface.mips.at(0);
    Require(surface.layers == Layers && surface.guestBytes <= Texels.size(), "the test array does not fit its storage");
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto texel = Texel(x, layer);
            std::copy(texel.begin(), texel.end(), Texels.begin() + static_cast<std::ptrdiff_t>(surface.GuestLayerOffset(layer) + mip.tiledOffset + x * 4u));
        }
    }
}

void FillInput(std::uint32_t limit) {
    const auto last = static_cast<float>(Layers - 1u);
    const std::array<float, 16> layers{0.0f, 1.0f, 2.4f, 1.6f, -1.0f, -0.4f, 0.4f, last, last + 0.4f, last + 1.0f, last + 7.0f, 1.0e6f, -1.0e6f, static_cast<float>(limit - 1u), static_cast<float>(limit), static_cast<float>(limit) + 0.6f};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = std::bit_cast<std::uint32_t>(-0.2f + 0.045f * static_cast<float>(tid));
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(layers[tid % layers.size()]);
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(static_cast<float>(tid % 3u) * 1.7f);
        Input[tid * 4u + 3u] = 0u;
    }
}

float Unorm(std::uint8_t value) {
    return static_cast<float>(value);
}

void Expect(std::uint32_t tid, const std::string& name, std::uint32_t component, float scaled, float expected, float tolerance) {
    Require(std::fabs(scaled - expected) <= tolerance, name + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected) + "/255");
}

struct Compiled {
    std::size_t bindings = 0;
    std::uint32_t elements = 0;
    ShaderRecompiler::SpirvContentHash spirv;
    std::uint64_t registeredVariant = 0;
    std::uint32_t registeredElements = 0;
    std::uint32_t preparedModes = 0;
    std::size_t preparedEntries = 0;
    bool promotedAtCapture = false;
};

Compiled Run(AgcDriver::VulkanDevice& device, std::uint32_t baseArray, std::uint32_t addressX, std::uint32_t filter, const char* name, const std::array<std::uint32_t, 11>& code = SampleLevelZero) {
    Output.fill(-1.0f);
    std::vector<std::uint32_t> userData(20, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = TextureDescriptor(baseArray);
    const std::array<std::uint32_t, 4> sampler{addressX | (ClampEdge << 3u) | (ClampEdge << 6u), 0u, filter, 0u};
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    static AgcDriver::DriverDetail::ShaderSnapshot zeroSnapshot{reinterpret_cast<std::uintptr_t>(SampleLevelZero.data()), 0, 0, {SampleLevelZero.begin(), SampleLevelZero.end()}, {}};
    static AgcDriver::DriverDetail::ShaderSnapshot levelSnapshot{reinterpret_cast<std::uintptr_t>(SampleLevel.data()), 0, 0, {SampleLevel.begin(), SampleLevel.end()}, {}};
    auto& snapshot = &code == &SampleLevel ? levelSnapshot : zeroSnapshot;
    request.shader.code = snapshot.code;
    auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
    const auto beforeCapture = snapshot.prepared->entries.size();
    AgcDriver::ShaderMemory shaderMemory(request.context.memory);
    const auto capture = AgcDriver::DriverDetail::CaptureInvocation(shaderMemory, snapshot, 0, request, invocation);
    const auto registered = invocation.Materialize(*capture);
    device.Dispatch(*registered, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto viewed = Layers - baseArray;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const float u = std::bit_cast<float>(Input[tid * 4u + 0u]);
        const auto layer = baseArray + static_cast<std::uint32_t>(std::clamp(std::nearbyint(std::bit_cast<float>(Input[tid * 4u + 1u])), 0.0f, static_cast<float>(viewed - 1u)));
        const float* actual = &Output[tid * 8u];
        std::array<float, 4> expected{};
        std::array<float, 4> tolerance{0.01f, 0.01f, 0.01f, 0.01f};
        if (filter == 0u) {
            const auto scaledU = static_cast<int>(std::floor(u * static_cast<float>(Width)));
            const auto x = addressX == Wrap ? static_cast<std::uint32_t>(((scaledU % static_cast<int>(Width)) + static_cast<int>(Width)) % static_cast<int>(Width)) : static_cast<std::uint32_t>(std::clamp(scaledU, 0, static_cast<int>(Width) - 1));
            const auto texel = Texel(x, layer);
            for (std::uint32_t component = 0; component < 4u; ++component) expected[component] = Unorm(texel[component]);
        } else {
            const float position = u * static_cast<float>(Width) - 0.5f;
            const auto left = static_cast<int>(std::floor(position));
            const float weight = position - std::floor(position);
            const auto clampX = [](int x) { return static_cast<std::uint32_t>(std::clamp(x, 0, static_cast<int>(Width) - 1)); };
            const auto a = Texel(clampX(left), layer);
            const auto b = Texel(clampX(left + 1), layer);
            for (std::uint32_t component = 0; component < 4u; ++component) expected[component] = Unorm(a[component]) * (1.0f - weight) + Unorm(b[component]) * weight;
            tolerance[0] = 1.0f;
        }
        for (std::uint32_t component = 0; component < 4u; ++component) Expect(tid, name, component, actual[component] * 255.0f, expected[component], tolerance[component]);
    }
    Compiled compiled{result.bindings.size(), 0u, result.spirv.ContentHash()};
    for (const auto& binding : result.bindings) compiled.elements += binding.count;
    compiled.registeredVariant = registered->variantId;
    for (const auto& binding : registered->bindings) compiled.registeredElements += binding.count;
    for (const auto& image : capture->plan->info.images) compiled.preparedModes += static_cast<std::uint32_t>(ShaderRecompiler::ResourceMaterializer::RuntimeImageModes(image).size());
    compiled.preparedEntries = snapshot.prepared->entries.size();
    compiled.promotedAtCapture = compiled.preparedEntries > beforeCapture;
    std::printf("%s: %zu bindings, %u descriptor elements, SPIR-V %016llx%016llx\n", name, compiled.bindings, compiled.elements, static_cast<unsigned long long>(compiled.spirv.high), static_cast<unsigned long long>(compiled.spirv.low));
    std::printf("registered: %u descriptor elements, prepared: %u image modes, variant %llu, capture promotion %u\n", compiled.registeredElements, compiled.preparedModes, static_cast<unsigned long long>(compiled.registeredVariant), compiled.promotedAtCapture ? 1u : 0u);
    return compiled;
}

}

int main() {
    try {
        Require(reinterpret_cast<std::uintptr_t>(Texels.data()) % TexelAlignment == 0, "the test array is not 64 KiB aligned");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto limit = device->graphicsContext().limits.maxImageArrayLayers;
        Require(limit != 0u, "the device reports no maxImageArrayLayers");
        if (limit >= MaxGuestLayers) {
            std::puts("skipped: the device allows every array a T# can describe");
            return VulkanTestSkipped;
        }
        Layers = std::min(limit + 3u, MaxGuestLayers);
        const auto baseWithin = Layers - limit;
        const auto basePast = baseWithin - 1u;
        FillTexels();
        FillInput(limit);
        const auto atLimit = Run(*device, baseWithin, ClampEdge, 0, "point image_sample_lz on a view of exactly maxImageArrayLayers slices");
        const auto atLimitLevel = Run(*device, baseWithin, ClampEdge, Bilinear, "bilinear image_sample_l at levels 0 to 3.4 on a view of exactly maxImageArrayLayers slices", SampleLevel);
        const auto pastLimit = Run(*device, basePast, ClampEdge, 0, "point image_sample_lz on a view of maxImageArrayLayers + 1 slices");
        const auto pastLimitLevel = Run(*device, basePast, ClampEdge, Bilinear, "bilinear image_sample_l at levels 0 to 3.4 on a view of maxImageArrayLayers + 1 slices", SampleLevel);
        Require(atLimit.preparedModes < pastLimit.preparedModes && atLimitLevel.preparedModes < pastLimitLevel.preparedModes, "a shader that never saw a view past maxImageArrayLayers was given the layer-row mode");
        Require(!(atLimit.spirv == pastLimit.spirv) && !(atLimitLevel.spirv == pastLimitLevel.spirv), "a view past maxImageArrayLayers was sampled through the shader prepared for views within the limit");
        Require(pastLimit.promotedAtCapture && pastLimitLevel.promotedAtCapture, "capturing the first oversized view did not promote its registered shader");
        Require(atLimit.registeredVariant != pastLimit.registeredVariant && atLimitLevel.registeredVariant != pastLimitLevel.registeredVariant, "registered shaders used the same artifact before and after layer-row promotion");
        const auto repeated = Run(*device, basePast, ClampEdge, 0, "repeated oversized view through its promoted registered shader");
        const auto returned = Run(*device, baseWithin, ClampEdge, 0, "within-limit view through its promoted registered shader");
        const auto repeatedLevel = Run(*device, basePast, ClampEdge, Bilinear, "repeated explicit-LOD oversized view through its promoted registered shader", SampleLevel);
        const auto returnedLevel = Run(*device, baseWithin, ClampEdge, Bilinear, "within-limit explicit-LOD view through its promoted registered shader", SampleLevel);
        for (const auto* reused : {&repeated, &returned}) {
            Require(reused->registeredVariant == pastLimit.registeredVariant && reused->preparedEntries == pastLimit.preparedEntries && !reused->promotedAtCapture, "registered layer-row shader was prepared again instead of reused");
        }
        for (const auto* reused : {&repeatedLevel, &returnedLevel}) {
            Require(reused->registeredVariant == pastLimitLevel.registeredVariant && reused->preparedEntries == pastLimitLevel.preparedEntries && !reused->promotedAtCapture, "registered explicit-LOD layer-row shader was prepared again instead of reused");
        }
        Run(*device, 0, ClampEdge, 0, "point image_sample_lz on a 1D array past maxImageArrayLayers");
        Run(*device, 0, Wrap, 0, "wrapped point image_sample_lz on a 1D array past maxImageArrayLayers");
        Run(*device, 0, ClampEdge, Bilinear, "bilinear image_sample_lz on a 1D array past maxImageArrayLayers");
        Run(*device, 0, ClampEdge, Bilinear, "bilinear image_sample_l at levels 0 to 3.4 on a single-level 1D array past maxImageArrayLayers", SampleLevel);
        std::puts("a 1D array past maxImageArrayLayers was sampled as rows of a 2D image");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
