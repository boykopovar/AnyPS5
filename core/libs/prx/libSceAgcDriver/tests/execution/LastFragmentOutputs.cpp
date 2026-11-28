#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>

namespace {

using AgcDriver::Graphics::FragmentOutputLookups;
using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Triangles;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("last fragment outputs: " + what);
}

std::string Describe(const FragmentOutputLookups& before, const FragmentOutputLookups& after) {
    return std::to_string(after.last - before.last) + " from the last key, " + std::to_string(after.memo - before.memo) + " from the memo, " + std::to_string(after.validated - before.validated) + " validated";
}

std::uint32_t Draw(AgcDriver::VulkanDevice& device, const Shaders& compiled, VkCullModeFlags cullMode) {
    std::fill(Pixels.begin(), Pixels.end(), std::byte{0});
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = cullMode;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    std::uint32_t covered = 0;
    for (std::size_t pixel = 0; pixel < Pixels.size() / 4u; ++pixel) {
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) == 255u) ++covered;
    }
    return covered;
}

void ExpectLookup(const FragmentOutputLookups& before, const FragmentOutputLookups& after, std::uint64_t last, std::uint64_t memo, std::uint64_t validated, const std::string& what) {
    Expect(after.last - before.last == last && after.memo - before.memo == memo && after.validated - before.validated == validated, what + ": " + Describe(before, after));
}

}

int main() {
    try {
        const bool remembers = std::getenv("APS5_NO_LAST_FRAGMENT_OUTPUTS") == nullptr;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = Compile(*device);
        device->WaitIdle();
        auto before = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        Expect(Draw(*device, shaders, VK_CULL_MODE_NONE) == Covered, "the first draw did not cover its target");
        auto after = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        ExpectLookup(before, after, 0, 0, 1, "the first draw of a key");
        before = after;
        Expect(Draw(*device, shaders, VK_CULL_MODE_NONE) == Covered, "a repeated draw did not cover its target");
        after = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        ExpectLookup(before, after, remembers ? 1 : 0, remembers ? 0 : 1, 0, "a repeat of the thread's last key");
        before = after;
        Expect(Draw(*device, shaders, VK_CULL_MODE_FRONT_AND_BACK) == 0, "a draw culling both faces wrote pixels");
        after = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        ExpectLookup(before, after, 0, 0, 1, "a draw of another key");
        before = after;
        Expect(Draw(*device, shaders, VK_CULL_MODE_NONE) == Covered, "the first key after another key did not cover its target");
        after = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        ExpectLookup(before, after, 0, 1, 0, "a key that is not the thread's last");
        before = after;
        Expect(Draw(*device, shaders, VK_CULL_MODE_NONE) == Covered, "the key repeated after the memo served it did not cover its target");
        after = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        ExpectLookup(before, after, remembers ? 1 : 0, remembers ? 0 : 1, 0, "a repeat of the key the memo served last");
        before = after;
        Expect(Draw(*device, shaders, VK_CULL_MODE_FRONT_AND_BACK) == 0, "the other key repeated after a switch wrote pixels");
        after = AgcDriver::Graphics::ThreadFragmentOutputLookups();
        ExpectLookup(before, after, 0, 1, 0, "the other key after a switch back");
        std::printf("last fragment outputs tests passed (%s)\n", remembers ? "on" : "off");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
