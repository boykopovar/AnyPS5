#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

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

alignas(4096) std::array<std::byte, Width * Height * 4 * Samples> Multisampled{};
alignas(4096) std::array<float, Width * Height> DepthA{};
alignas(4096) std::array<float, Width * Height> DepthB{};

struct Barrier {
    VkPipelineStageFlags source;
    VkPipelineStageFlags destination;
    std::vector<VkMemoryBarrier> memory;
    std::vector<VkImageMemoryBarrier> images;
};

class Capture {
public:
    static Context Install(const Context& base) {
        Require(resolver == nullptr, "nested barrier capture");
        resolver = base.deviceProc;
        device = base.device;
        Context captured = base;
        captured.bufferPool.reset();
        captured.deviceProc = resolve;
        captured.functions = nullptr;
        return captured;
    }
    static std::vector<Barrier> Take() { return std::exchange(barriers, {}); }

private:
    static PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice handle, const char* name) {
        if (std::string_view(name) == "vkCmdPipelineBarrier") return reinterpret_cast<PFN_vkVoidFunction>(pipelineBarrier);
        return resolver(handle, name);
    }
    static VKAPI_ATTR void VKAPI_CALL pipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags dependencies, std::uint32_t memoryCount, const VkMemoryBarrier* memory, std::uint32_t bufferCount, const VkBufferMemoryBarrier* buffers, std::uint32_t imageCount, const VkImageMemoryBarrier* images) {
        barriers.push_back({source, destination, std::vector<VkMemoryBarrier>(memory, memory + memoryCount), std::vector<VkImageMemoryBarrier>(images, images + imageCount)});
        reinterpret_cast<PFN_vkCmdPipelineBarrier>(resolver(device, "vkCmdPipelineBarrier"))(commands, source, destination, dependencies, memoryCount, memory, bufferCount, buffers, imageCount, images);
    }

    static inline PFN_vkGetDeviceProcAddr resolver = nullptr;
    static inline VkDevice device = VK_NULL_HANDLE;
    static inline std::vector<Barrier> barriers;
};

bool OrdersColorWrites(const std::vector<Barrier>& barriers) {
    return std::any_of(barriers.begin(), barriers.end(), [](const Barrier& barrier) {
        return barrier.source == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT && barrier.destination == VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT && barrier.images.empty() && barrier.memory.size() == 1 && (barrier.memory[0].srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0 && (barrier.memory[0].dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0;
    });
}

void Draw(const Context& context, AgcDriver::VulkanDevice& device, const Shaders& compiled, std::uint64_t depth) {
    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Multisampled.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Multisampled.size(), 0xe4u};
    state.color.samples = Samples;
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
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto base = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if ((base.limits.framebufferColorSampleCounts & Samples) == 0 || (base.limits.framebufferDepthSampleCounts & Samples) == 0) {
            std::puts("skipped, the device cannot render four-sample color and depth");
            return VulkanTestSkipped;
        }
        if (!Recorder::MergeBarriers()) {
            std::puts("skipped, barrier elision is off");
            return VulkanTestSkipped;
        }
        const auto shaders = Compile(*device);
        device->WaitIdle();
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto captured = Capture::Install(base);
        Recorder recorder(captured);
        recorder.Activate();
        const auto depthA = reinterpret_cast<std::uintptr_t>(DepthA.data());
        const auto depthB = reinterpret_cast<std::uintptr_t>(DepthB.data());
        Draw(captured, *device, shaders, depthA);
        Draw(captured, *device, shaders, depthB);
        Capture::Take();
        Draw(captured, *device, shaders, depthA);
        const auto between = Capture::Take();
        Require(OrdersColorWrites(between), "multisampled pass order: two draws sharing a multisampled color target while switching depth targets recorded no barrier ordering the first draw's color writes before the second");
        recorder.Submit();
        recorder.Sync();
        std::puts("multisampled pass order tests passed (ordering asserted on the recorded barriers; the multisampled image is not resolved to guest memory)");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
