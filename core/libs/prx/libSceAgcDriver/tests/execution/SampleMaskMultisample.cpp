#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::PixelInput;
using ShaderRecompiler::PixelInputBit;
using ShaderRecompiler::ShaderStage;
using Masks = std::array<std::uint16_t, 4>;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Samples = 4;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t EarlyZThenLateZ = 0x10u;
constexpr Masks Full{0xf, 0xf, 0xf, 0xf};
alignas(256) std::array<std::uint32_t, Width * Height * Samples> Invocations{};
alignas(256) std::array<std::byte, 256 * 1024> DepthMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> SampleStoreCode{
    0x7e100f04, 0x7e120f05, 0x34121286, 0x4a101308, 0x34101082, 0xd548000a, 0x02111106, 0x4a101508,
    0x34101082, 0x7e160281, 0xe0705000, 0x80000b08, 0xbf8c3f70, 0xf8001890, 0x00000000, 0xbf810000,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> NullExportCode{0xf8001890, 0x00000000, 0xbf810000};

using Triangle = std::array<std::array<float, 4>, 3>;
alignas(256) constexpr Triangle Far{{{-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f}}};
alignas(256) constexpr Triangle Probe{{{-1.0f, -1.0f, 0.375f, 1.0f}, {3.0f, -1.0f, 0.375f, 1.0f}, {-1.0f, 3.0f, 0.375f, 1.0f}}};
alignas(256) constexpr Triangle Middle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, stride == 0u ? 0x31016facu : 0x01016facu};
}

struct Pass {
    Masks quadSampleMask = Full;
    bool perSample = true;
    std::uint32_t shaderControl = 0u;
    const Triangle* triangle = &Middle;
    bool depth = false;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_ALWAYS;
};

void Draw(AgcDriver::VulkanDevice& device, const Pass& pass) {
    Invocations.fill(0);
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(pass.triangle->data(), 16u, static_cast<std::uint32_t>(pass.triangle->size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.hasColorTarget = false;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blendConstants = {};
    state.samples = Samples;
    state.perSampleShading = pass.perSample;
    state.quadSampleMask = pass.quadSampleMask;
    state.pixelEarlyZ = AgcDriver::Graphics::PixelEarlyZ(pass.shaderControl);
    if (pass.depth) {
        state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), 0u, {Width, Height}, VK_FORMAT_D32_SFLOAT, 0.0f, 0};
        state.depth->samples = Samples;
        state.depthTest = true;
        state.depthWrite = pass.depthWrite;
        state.depthCompare = pass.depthCompare;
    }

    const std::uint32_t inputs = pass.perSample ? PixelInputBit(PixelInput::PerspectiveSample) | PixelInputBit(PixelInput::LinearSample) | PixelInputBit(PixelInput::PositionX) | PixelInputBit(PixelInput::PositionY) | PixelInputBit(PixelInput::Ancillary) : PixelInputBit(PixelInput::PerspectiveCenter);
    const AgcDriver::Registers context{{0x1b3, inputs}, {0x1b4, inputs}, {0x1b6, 0u}, {0x1c5, 0u}, {0x203, pass.shaderControl}};
    auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    Require(pixel.sampleShading == pass.perSample, "the pixel inputs did not select the requested shading rate");
    pixel.quadPixelMask = AgcDriver::Graphics::QuadPixelMask(state);
    pixel.quadSampleMask = AgcDriver::Graphics::QuadSampleMasks(state);
    pixel.iteratesSamples = state.perSampleShading;
    pixel.sampleLocations = AgcDriver::Graphics::PixelSampleLocations(state, pixel);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const auto invocations = BufferDescriptor(Invocations.data(), 0u, static_cast<std::uint32_t>(sizeof(Invocations)));
    std::copy(invocations.begin(), invocations.end(), pixelUserData.begin());
    const std::span<const std::uint32_t> code = pass.perSample ? std::span<const std::uint32_t>(SampleStoreCode) : std::span<const std::uint32_t>(NullExportCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(pass.triangle->size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const Masks& quadSampleMask, const std::string& what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto mask = quadSampleMask[(x & 1u) | ((y & 1u) << 1u)];
            for (std::uint32_t sample = 0; sample < Samples; ++sample) {
                const auto expected = (mask >> sample) & 1u;
                const auto ran = Invocations[(y * Width + x) * Samples + sample];
                Require(ran == expected, what + ": sample " + std::to_string(sample) + " of pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") " + (ran != 0u ? "stored" : "did not store") + " (" + std::to_string(ran) + "), the quad pixel's mask is " + std::to_string(mask));
            }
        }
    }
}

void DepthWritten(AgcDriver::VulkanDevice& device, const Masks& mask, const std::string& what) {
    Pass reset;
    reset.perSample = false;
    reset.triangle = &Far;
    reset.depth = true;
    reset.depthWrite = true;
    Draw(device, reset);
    Pass masked = reset;
    masked.quadSampleMask = mask;
    masked.triangle = &Middle;
    Draw(device, masked);
    Pass probe;
    probe.shaderControl = EarlyZThenLateZ;
    probe.triangle = &Probe;
    probe.depth = true;
    probe.depthCompare = VK_COMPARE_OP_LESS;
    Draw(device, probe);
    Check(mask, "the samples a pixel-rate draw with " + what + " wrote depth 0.5 into, probed by early-Z per-sample stores");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        struct Case {
            const char* name;
            Masks mask;
        };
        const std::array<Case, 9> cases{{
            {"no sample mask", Full},
            {"samples 0 and 2 of every pixel", {0x5, 0x5, 0x5, 0x5}},
            {"sample 3 of every pixel", {0x8, 0x8, 0x8, 0x8}},
            {"no sample of any pixel", {0x0, 0x0, 0x0, 0x0}},
            {"every sample of the even-x, even-y pixel of each quad", {0xf, 0x0, 0x0, 0x0}},
            {"every sample of the odd-x pixels of each quad", {0x0, 0xf, 0x0, 0xf}},
            {"sample q of quad pixel q", {0x1, 0x2, 0x4, 0x8}},
            {"all, none, none and samples 0 and 1", {0xf, 0x0, 0x0, 0x3}},
            {"samples 1 and 3, 0 and 2, 2 and 3, and 0", {0xa, 0x5, 0xc, 0x1}},
        }};
        for (const auto& test : cases) {
            Pass pass;
            pass.quadSampleMask = test.mask;
            Draw(*device, pass);
            Check(test.mask, std::string("4x per-sample shading with ") + test.name);
        }
        for (const auto& test : cases) DepthWritten(*device, test.mask, test.name);
        Pass early;
        early.quadSampleMask = {0x6, 0x6, 0x6, 0x6};
        early.shaderControl = EarlyZThenLateZ;
        try {
            Draw(*device, early);
        } catch (const std::exception& error) {
            Require(std::string(error.what()).find("earlyFragmentSampleMaskTestBeforeSampleCounting") != std::string::npos, std::string("a sample mask with early fragment tests failed for another reason: ") + error.what());
            std::puts("4x sample mask tests passed (early fragment tests refused: the device does not order the sample mask test before sample counting)");
            return 0;
        }
        Check(early.quadSampleMask, "4x per-sample shading with early fragment tests and samples 1 and 2 of every pixel");
        std::puts("4x sample mask tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
