#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;
using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Triangles;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Samples = 4;

constexpr std::size_t SurfaceBytes = 65536;

class Surface {
public:
    std::span<std::byte> Bytes() {
        void* pointer = storage.data();
        std::size_t space = storage.size();
        auto* aligned = static_cast<std::byte*>(std::align(SurfaceBytes, SurfaceBytes, pointer, space));
        Require(aligned != nullptr, "pass break reasons: cannot align the surface backing");
        return {aligned, SurfaceBytes};
    }

    std::uint64_t Address() { return reinterpret_cast<std::uintptr_t>(Bytes().data()); }

private:
    std::array<std::byte, SurfaceBytes * 2> storage{};
};

Surface ColorA{};
Surface ColorB{};
Surface DepthA{};
Surface DepthB{};
Surface DepthC{};

struct Counts {
    DrawPassCounts passes;
    std::array<std::uint64_t, PassBreakCount> breaks;
};

Counts Snapshot() {
    return {DrawPassTotals()[0], DrawPassBreaks()};
}

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pass break reasons: " + what);
}

std::string Describe(const Counts& before, const Counts& after) {
    std::string text = std::to_string(after.passes.passesBegun - before.passes.passesBegun) + " begun, " + std::to_string(after.passes.passesContinued - before.passes.passesContinued) + " continued;";
    for (std::size_t i = 0; i < PassBreakCount; ++i) {
        if (after.breaks[i] != before.breaks[i]) text += " " + std::string(PassBreakNames[i]) + " " + std::to_string(after.breaks[i] - before.breaks[i]);
    }
    return text;
}

void ExpectBreak(const Counts& before, const Counts& after, PassBreak reason, const std::string& what) {
    Expect(after.passes.passesBegun - before.passes.passesBegun == 1 && after.passes.passesContinued == before.passes.passesContinued, what + " did not begin exactly one pass: " + Describe(before, after));
    for (std::size_t i = 0; i < PassBreakCount; ++i) {
        const std::uint64_t expected = i == static_cast<std::size_t>(reason) ? 1u : 0u;
        Expect(after.breaks[i] - before.breaks[i] == expected, what + " was not counted once as '" + PassBreakNames[static_cast<std::size_t>(reason)] + "': " + Describe(before, after));
    }
}

void Draw(const Context& context, AgcDriver::VulkanDevice& device, const Shaders& compiled, Surface& color, std::uint64_t depth) {
    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {color.Address(), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, color.Bytes().size(), 0xe4u};
    state.color.surfaceAddress = state.color.address;
    state.color.surfaceExtent = state.color.extent;
    state.color.tileMode = ColorTileMode::RenderTarget;
    state.color.samples = Samples;
    Expect(MultisampledColorBytes(state.color) == color.Bytes().size(), "the color backing differs from its multisample footprint");
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = DepthTarget{depth, 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0, false, Samples};
    state.depthTest = true;
    state.depthWrite = true;
    state.depthCompare = VK_COMPARE_OP_LESS;
    state.samples = Samples;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0, 1, 0, false};
    AgcDriver::Graphics::Draw(context, state, draw, shaders);
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
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if ((context.limits.framebufferColorSampleCounts & Samples) == 0 || (context.limits.framebufferDepthSampleCounts & Samples) == 0) {
            std::puts("skipped, the device cannot render four-sample color and depth");
            return VulkanTestSkipped;
        }
        const auto shaders = Compile(*device);
        device->WaitIdle();
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(context);
        recorder.Activate();
        Expect(MultisampledDepthBytes_nid_no_patch({Width, Height}, sizeof(float), Samples) == DepthA.Bytes().size(), "the depth backing differs from its multisample footprint");
        const auto depthA = DepthA.Address();
        const auto depthB = DepthB.Address();
        const auto depthC = DepthC.Address();
        auto before = Snapshot();
        Draw(context, *device, shaders, ColorA, depthA);
        auto after = Snapshot();
        ExpectBreak(before, after, PassBreak::NewBatch, "the first draw of a batch");
        Draw(context, *device, shaders, ColorA, depthB);
        Draw(context, *device, shaders, ColorB, depthC);
        recorder.Submit();
        recorder.Sync();
        before = Snapshot();
        Draw(context, *device, shaders, ColorA, depthA);
        after = Snapshot();
        ExpectBreak(before, after, PassBreak::NewBatch, "the first draw after a submit");
        before = after;
        Draw(context, *device, shaders, ColorA, depthA);
        after = Snapshot();
        Expect(after.passes.passesContinued - before.passes.passesContinued == 1 && after.breaks == before.breaks, "a draw with the open pass's key and no hazard did not continue it: " + Describe(before, after));
        recorder.Commands();
        before = Snapshot();
        Draw(context, *device, shaders, ColorA, depthA);
        after = Snapshot();
        ExpectBreak(before, after, PassBreak::Command, "a draw after a non-draw command");
        before = after;
        Draw(context, *device, shaders, ColorA, depthB);
        after = Snapshot();
        ExpectBreak(before, after, PassBreak::Image, "a draw switching to a depth target the open pass does not touch while sharing its color target");
        before = after;
        Draw(context, *device, shaders, ColorB, depthC);
        after = Snapshot();
        ExpectBreak(before, after, PassBreak::Key, "a draw into targets the open pass does not touch");
        recorder.Submit();
        recorder.Sync();
        std::puts("pass break reasons tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
