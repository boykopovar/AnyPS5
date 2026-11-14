#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#endif
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
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

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Samples = 4;

alignas(4096) std::array<std::byte, Width * Height * 4 * Samples> Multisampled{};

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("draw recipe skips: " + what);
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes) : bytes((bytes + BlockBytes - 1u) & ~(BlockBytes - 1u)) {
#ifdef _WIN32
        block = static_cast<std::byte*>(VirtualAlloc(nullptr, this->bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::byte*>(std::aligned_alloc(BlockBytes, this->bytes));
#endif
        Expect(block != nullptr, "cannot allocate a guest block");
        std::memset(block, 0, this->bytes);
        GuestAllocations::Mutation().Add(block, this->bytes, true, true, true);
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }
    std::byte* Bytes() const { return block; }

private:
    std::size_t bytes;
    std::byte* block = nullptr;
};

State TargetState(const ColorTarget& color, std::uint32_t samples) {
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = color;
    state.colors = {color};
    state.hasColorTarget = true;
    state.samples = samples;
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
    return state;
}

ColorTarget ResidentTarget(const GuestBlock& block) {
    ColorTarget color{};
    color.address = block.Address();
    color.surfaceAddress = color.address;
    color.extent = {Width, Height};
    color.surfaceExtent = color.extent;
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.componentMapping = 0xe4u;
    color.tileMode = ColorTileMode::RenderTarget;
    color.elementBytes = 4u;
    color.bytes = ColorTargetLayout(Width, Height, color.tileMode, color.elementBytes).Bytes();
    return color;
}

ColorTarget MultisampledTarget() {
    ColorTarget color{reinterpret_cast<std::uintptr_t>(Multisampled.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Multisampled.size(), 0xe4u};
    color.surfaceAddress = color.address;
    color.samples = Samples;
    return color;
}

std::shared_ptr<const AgcDriver::DrawRecipe> Draw(const Context& context, AgcDriver::VulkanDevice& device, const Shaders& compiled, const State& state, bool wantRecipe) {
    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0, 1, 0, false};
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    AgcDriver::Graphics::Draw(context, state, draw, shaders, {}, wantRecipe ? &recipe : nullptr);
    return recipe;
}

std::string Describe(const std::array<std::uint64_t, DrawRecipeSkipCount>& before, const std::array<std::uint64_t, DrawRecipeSkipCount>& after) {
    std::string text;
    for (std::size_t i = 0; i < DrawRecipeSkipCount; ++i) {
        if (after[i] != before[i]) text += " " + std::string(DrawRecipeSkipName(static_cast<DrawRecipeSkip>(i))) + " " + std::to_string(after[i] - before[i]);
    }
    return text.empty() ? " nothing counted" : text;
}

void ExpectCounted(const std::array<std::uint64_t, DrawRecipeSkipCount>& before, const std::array<std::uint64_t, DrawRecipeSkipCount>& after, DrawRecipeSkip reason, const std::string& what) {
    for (std::size_t i = 0; i < DrawRecipeSkipCount; ++i) {
        const std::uint64_t expected = i == static_cast<std::size_t>(reason) ? 1u : 0u;
        Expect(after[i] - before[i] == expected, what + " was not counted once as '" + DrawRecipeSkipName(reason) + "':" + Describe(before, after));
    }
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
        if (context.detiler == nullptr) {
            std::puts("skipped, the device has no detiler for resident targets");
            return VulkanTestSkipped;
        }
        if ((context.limits.framebufferColorSampleCounts & Samples) == 0) {
            std::puts("skipped, the device cannot render four-sample color");
            return VulkanTestSkipped;
        }
        if (!DrawRecipes()) {
            std::puts("skipped, draw recipes are off");
            return VulkanTestSkipped;
        }
        GuestBlock vertices(BlockBytes);
        std::memcpy(vertices.Bytes(), Triangles().data(), Triangles().size() * sizeof(Triangles().front()));
        const auto shaders = Compile(*device, vertices.Bytes());
        device->WaitIdle();
        GuestBlock block(ColorTargetLayout(Width, Height, ColorTileMode::RenderTarget, 4u).Bytes());
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Recorder recorder(context);
        recorder.Activate();
        const auto resident = TargetState(ResidentTarget(block), 1u);
        auto before = DrawRecipeSkips();
        auto recipe = Draw(context, *device, shaders, resident, true);
        auto after = DrawRecipeSkips();
        Expect(recipe != nullptr, "a recorded draw into a resident target built no recipe:" + Describe(before, after));
        ExpectCounted(before, after, DrawRecipeSkip::Built, "a recorded draw into a resident target");
        before = after;
        recipe = Draw(context, *device, shaders, resident, false);
        after = DrawRecipeSkips();
        Expect(recipe == nullptr, "a draw without a recipe output returned a recipe");
        ExpectCounted(before, after, DrawRecipeSkip::NoOutput, "a draw without a recipe output");
        before = after;
        recipe = Draw(context, *device, shaders, TargetState(MultisampledTarget(), Samples), true);
        after = DrawRecipeSkips();
        Expect(recipe == nullptr, "a multisampled draw built a recipe");
        ExpectCounted(before, after, DrawRecipeSkip::Multisampled, "a multisampled draw");
        recorder.Submit();
        recorder.Sync();
        std::puts("draw recipe skips tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
