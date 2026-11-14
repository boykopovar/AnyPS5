#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
constexpr std::uint32_t Samples = 4;
alignas(4096) std::array<std::byte, 65536> RecordedPixels{};
alignas(4096) std::array<std::byte, 65536> RecordedSecondPixels{};
alignas(4096) std::array<std::byte, 65536> RecordedPairPixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelCode{
    0x7e0802ff, 0x3e4ccccd, 0x7e0a02ff, 0x3ecccccd, 0x7e0c02ff, 0x3f19999a, 0x7e0e02ff, 0x3f4ccccd,
    0xf800180f, 0x07060504, 0xbf810000,
};

constexpr std::array<float, 4> Source{0.2f, 0.4f, 0.6f, 0.8f};

constexpr std::array<std::array<float, 4>, 3> Vertices{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};
alignas(256) std::array<std::array<float, 4>, 3> VertexStorage = Vertices;

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

struct Shaders {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t vertexPush = 0;
};

Shaders Compile(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(VertexStorage.data(), 16u, static_cast<std::uint32_t>(VertexStorage.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    Shaders compiled;
    compiled.vertex = ShaderRecompiler::Recompile(vertex);
    compiled.vertexPush = static_cast<std::uint32_t>(compiled.vertex.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(compiled.vertexPush, target)
    };
    fragment.useCache = false;
    compiled.pixel = ShaderRecompiler::Recompile(fragment);
    return compiled;
}

std::array<AgcDriver::Graphics::CompiledShader, 2> Stages(AgcDriver::VulkanDevice& device, const Shaders& compiled) {
    return {{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
}

AgcDriver::Graphics::State StateFor(VkColorComponentFlags mask, bool recorded, bool unwrittenTarget) {
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.blendEnable = VK_FALSE;
    state.blend.colorWriteMask = mask;
    state.blends = {state.blend};
    if (recorded) {
        state.color = {reinterpret_cast<std::uintptr_t>(RecordedPixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, RecordedPixels.size(), 0xe4u};
        state.color.surfaceAddress = state.color.address;
        state.color.samples = Samples;
        state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
        state.colors = {state.color};
        state.samples = Samples;
    }
    if (unwrittenTarget) {
        auto second = state.color;
        second.address = second.surfaceAddress = reinterpret_cast<std::uintptr_t>(RecordedSecondPixels.data());
        second.exportIndex = 1;
        state.colors.push_back(second);
        state.blends.push_back(state.blend);
    }
    return state;
}

AgcDriver::Graphics::State SplitAlphaStateFor(VkColorComponentFlags mask) {
    auto state = StateFor(mask, true, false);
    state.color = {reinterpret_cast<std::uintptr_t>(RecordedPairPixels.data()), {Width, Height}, VK_FORMAT_R8G8_UNORM, RecordedPairPixels.size(), 0xecu};
    state.color.elementBytes = 2;
    state.color.surfaceAddress = state.color.address;
    state.color.samples = Samples;
    state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
    state.colors = {state.color};
    state.blend.blendEnable = VK_TRUE;
    state.blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    state.blend.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    state.blend.colorBlendOp = VK_BLEND_OP_ADD;
    state.blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    state.blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    state.blend.alphaBlendOp = VK_BLEND_OP_ADD;
    state.blends = {state.blend};
    return state;
}

const AgcDriver::Pm4::DrawParameters FullScreen{0, static_cast<std::uint32_t>(VertexStorage.size()), 0, 1, 0, false};

void Draw(AgcDriver::VulkanDevice& device, const Shaders& compiled, VkColorComponentFlags mask) {
    Pixels.fill(std::byte{0x5a});
    device.Draw(StateFor(mask, false, false), FullScreen, Stages(device, compiled));
    device.WaitIdle();
}

void Check(VkColorComponentFlags mask, std::string_view what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const auto expected = ((mask >> channel) & 1u) != 0 ? static_cast<int>(std::lround(Source[channel] * 255.0f)) : 0x5a;
                const auto actual = static_cast<int>(Pixels[(y * Width + x) * 4u + channel]);
                Require(std::abs(actual - expected) <= 1, std::string(what) + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
            }
        }
    }
}

bool SameCounters(const AgcDriver::Graphics::PipelineLibraryCounters& a, const AgcDriver::Graphics::PipelineLibraryCounters& b) {
    return a.built == b.built && a.linked == b.linked;
}

PFN_vkCmdSetColorWriteMaskEXT NativeSetMasks = nullptr;
std::atomic<std::uint64_t> MaskSets{0u};
std::mutex LastMasksMutex;
std::vector<VkColorComponentFlags> LastMasks;

VKAPI_ATTR void VKAPI_CALL CountMasks(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkColorComponentFlags* masks) {
    {
        const std::lock_guard lock(LastMasksMutex);
        LastMasks.assign(masks, masks + count);
    }
    ++MaskSets;
    NativeSetMasks(commands, first, count, masks);
}

struct MaskTrace {
    AgcDriver::Graphics::DeviceFunctions* functions;
    explicit MaskTrace(const AgcDriver::Graphics::Context& context) {
        functions = const_cast<AgcDriver::Graphics::DeviceFunctions*>(context.functions);
        NativeSetMasks = context.dynamicColorWriteMask ? reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(context.deviceProc(context.device, "vkCmdSetColorWriteMaskEXT")) : nullptr;
        Require(functions != nullptr && NativeSetMasks != nullptr, "vkCmdSetColorWriteMaskEXT is unavailable");
        functions->cmdSetColorWriteMask = CountMasks;
    }
    ~MaskTrace() { functions->cmdSetColorWriteMask = NativeSetMasks; }
};

std::string Masks(const std::vector<VkColorComponentFlags>& masks) {
    std::string text = "{";
    for (std::size_t index = 0; index < masks.size(); ++index) text += (index != 0 ? ", " : "") + std::to_string(masks[index]);
    return text + "}";
}

void ExpectSets(std::uint64_t before, std::uint64_t expected, std::string_view what) {
    const auto sets = MaskSets.load() - before;
    Require(sets == expected, std::string(what) + ": set the color write masks " + std::to_string(sets) + " times, expected " + std::to_string(expected));
}

void ExpectMasks(const std::vector<VkColorComponentFlags>& expected, std::string_view what) {
    const std::lock_guard lock(LastMasksMutex);
    Require(LastMasks == expected, std::string(what) + ": color write masks " + Masks(LastMasks) + ", expected " + Masks(expected));
}

std::string BreaksSince(const std::array<std::uint64_t, AgcDriver::Graphics::PassBreakCount>& before) {
    using namespace AgcDriver::Graphics;
    std::string reasons;
    const auto after = DrawPassBreaks();
    for (std::size_t index = 0; index < PassBreakCount; ++index) if (after[index] != before[index]) reasons += " " + std::string(PassBreakNames[index]) + " " + std::to_string(after[index] - before[index]);
    const auto totals = DrawPassTotals();
    return reasons + "; lean " + std::to_string(totals[0].draws) + "/" + std::to_string(totals[0].passesBegun) + "/" + std::to_string(totals[0].passesContinued) + ", address " + std::to_string(totals[1].draws) + "/" + std::to_string(totals[1].passesBegun) + "/" + std::to_string(totals[1].passesContinued);
}

void RecordedMasks(AgcDriver::VulkanDevice& device, const Shaders& compiled, VkColorComponentFlags all, VkColorComponentFlags redBlue) {
    using namespace AgcDriver::Graphics;
    const auto context = device.graphicsContext();
    const auto shaders = Stages(device, compiled);
    const std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Recorder recorder(context, true);
    recorder.Activate();
    const MaskTrace trace(context);
    const auto continued = [] { return DrawPassTotals()[0].passesContinued; };
    auto before = MaskSets.load();
    AgcDriver::Graphics::Draw(context, StateFor(all, true, false), FullScreen, shaders);
    ExpectSets(before, 1u, "the recorded draw that begins the pass");
    ExpectMasks({all}, "the recorded draw that begins the pass");
    auto passes = continued();
    auto breaks = DrawPassBreaks();
    before = MaskSets.load();
    AgcDriver::Graphics::Draw(context, StateFor(redBlue, true, false), FullScreen, shaders);
    Require(continued() == passes + 1u, "the draw with another color write mask did not continue the open pass; breaks:" + BreaksSince(breaks));
    ExpectSets(before, 1u, "a continued draw with another color write mask");
    ExpectMasks({redBlue}, "a continued draw with another color write mask");
    passes = continued();
    breaks = DrawPassBreaks();
    before = MaskSets.load();
    AgcDriver::Graphics::Draw(context, StateFor(redBlue, true, false), FullScreen, shaders);
    Require(continued() == passes + 1u, "the draw with the same color write mask did not continue the open pass; breaks:" + BreaksSince(breaks));
    ExpectSets(before, std::getenv("APS5_NO_STATE_SUPPRESSION") == nullptr ? 0u : 1u, "a continued draw with the same color write mask");
    ExpectMasks({redBlue}, "a continued draw with the same color write mask");
    recorder.Commands();
    before = MaskSets.load();
    AgcDriver::Graphics::Draw(context, StateFor(all, true, true), FullScreen, shaders);
    ExpectSets(before, 1u, "the draw with a color target the pixel shader does not write");
    ExpectMasks({all, 0u}, "the draw with a color target the pixel shader does not write");
    recorder.Commands();
    before = MaskSets.load();
    AgcDriver::Graphics::Draw(context, SplitAlphaStateFor(all), FullScreen, shaders);
    ExpectSets(before, 2u, "the draw that blends alpha separately into an 8_8 target in two passes");
    ExpectMasks({VK_COLOR_COMPONENT_G_BIT}, "the second pass of the split alpha draw");
    passes = continued();
    breaks = DrawPassBreaks();
    before = MaskSets.load();
    AgcDriver::Graphics::Draw(context, SplitAlphaStateFor(all), FullScreen, shaders);
    Require(continued() == passes + 1u, "the draw after the split alpha draw did not continue the open pass; breaks:" + BreaksSince(breaks));
    ExpectSets(before, 2u, "a continued draw after the split alpha draw left the green mask set");
    recorder.Submit();
    recorder.Sync();
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto vkDevice = device->graphicsContext().device;
        const bool dynamic = device->graphicsContext().dynamicColorWriteMask;
        constexpr VkColorComponentFlags all = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        constexpr VkColorComponentFlags redBlue = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT;
        constexpr VkColorComponentFlags greenAlpha = VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT;
        const auto compiled = Compile(*device);
        Draw(*device, compiled, all);
        Check(all, "all channels");
        const auto counters = AgcDriver::Graphics::PipelineLibraryCountersOf(vkDevice);
        Draw(*device, compiled, redBlue);
        Check(redBlue, "red and blue only");
        Draw(*device, compiled, greenAlpha);
        Check(greenAlpha, "green and alpha only");
        Draw(*device, compiled, all);
        Check(all, "all channels again");
        const auto after = AgcDriver::Graphics::PipelineLibraryCountersOf(vkDevice);
        if (dynamic) Require(SameCounters(counters, after), "draws that differ only in their color write mask built or linked another pipeline: linked " + std::to_string(counters.linked) + " -> " + std::to_string(after.linked) + ", fragment output libraries " + std::to_string(counters.built[3]) + " -> " + std::to_string(after.built[3]));
        const bool multisampled = (device->graphicsContext().limits.framebufferColorSampleCounts & Samples) != 0;
        if (dynamic && multisampled) RecordedMasks(*device, compiled, all, redBlue);
        std::printf("color write mask tests passed (%s, %s, %s)\n", dynamic ? "dynamic write mask" : "write mask in the pipeline", counters.linked != 0 ? "pipeline libraries" : "whole pipelines", !dynamic ? "no recorded draws" : multisampled ? "recorded draws" : "recorded draws skipped: no four-sample color");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
