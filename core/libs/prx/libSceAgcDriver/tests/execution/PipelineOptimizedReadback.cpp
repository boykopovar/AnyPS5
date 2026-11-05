#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
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
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<float, Width * Height> DepthMemory{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 7> MaskedPixelCode{
    0x7e040f00, 0x36060481, 0x7da40680, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 4> FullPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};
constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};
constexpr std::array<std::array<float, 4>, 12> DepthTriangles{{
    {-1.0f, -1.0f, 0.25f, 1.0f}, {0.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 1.0f, 0.25f, 1.0f},
    {-1.0f, 1.0f, 0.25f, 1.0f}, {0.0f, -1.0f, 0.25f, 1.0f}, {0.0f, 1.0f, 0.25f, 1.0f},
    {0.0f, -1.0f, 0.75f, 1.0f}, {1.0f, -1.0f, 0.75f, 1.0f}, {0.0f, 1.0f, 0.75f, 1.0f},
    {0.0f, 1.0f, 0.75f, 1.0f}, {1.0f, -1.0f, 0.75f, 1.0f}, {1.0f, 1.0f, 0.75f, 1.0f}
}};

struct PipelineTrace {
    PFN_vkGetDeviceProcAddr resolve = nullptr;
    PFN_vkCreateGraphicsPipelines create = nullptr;
    PFN_vkCmdBindPipeline bind = nullptr;
    PFN_vkCmdDraw draw = nullptr;
    DeviceFunctions functions{};
    std::mutex mutex;
    std::condition_variable released;
    bool allowOptimization = false;
    std::uint32_t libraries = 0;
    std::uint32_t retaining = 0;
    std::vector<VkPipeline> fast;
    std::vector<VkPipeline> optimized;
    VkCommandBuffer boundCommands = VK_NULL_HANDLE;
    VkPipeline bound = VK_NULL_HANDLE;
    std::vector<VkPipeline> drawn;

    void ReleaseOptimization() {
        {
            std::lock_guard lock(mutex);
            allowOptimization = true;
        }
        released.notify_all();
    }
};

PipelineTrace* Trace = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL CreatePipelines(VkDevice device, VkPipelineCache cache, std::uint32_t count, const VkGraphicsPipelineCreateInfo* infos, const VkAllocationCallbacks* allocator, VkPipeline* pipelines) {
    const bool optimizing = std::any_of(infos, infos + count, [](const auto& info) { return (info.flags & VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT) != 0; });
    if (optimizing) {
        std::unique_lock lock(Trace->mutex);
        Trace->released.wait(lock, [] { return Trace->allowOptimization; });
    }
    const auto result = Trace->create(device, cache, count, infos, allocator, pipelines);
    std::lock_guard lock(Trace->mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (pipelines[index] == VK_NULL_HANDLE) continue;
        if ((infos[index].flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) != 0) {
            ++Trace->libraries;
            if ((infos[index].flags & VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT) != 0) ++Trace->retaining;
            continue;
        }
        auto& handles = (infos[index].flags & VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT) != 0 ? Trace->optimized : Trace->fast;
        handles.push_back(pipelines[index]);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL BindPipeline(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipeline pipeline) {
    if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) {
        Trace->boundCommands = commands;
        Trace->bound = pipeline;
    }
    Trace->bind(commands, point, pipeline);
}

VKAPI_ATTR void VKAPI_CALL DrawCommands(VkCommandBuffer commands, std::uint32_t vertices, std::uint32_t instances, std::uint32_t firstVertex, std::uint32_t firstInstance) {
    Trace->drawn.push_back(Trace->boundCommands == commands ? Trace->bound : VK_NULL_HANDLE);
    Trace->draw(commands, vertices, instances, firstVertex, firstInstance);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkCreateGraphicsPipelines") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreatePipelines);
    if (std::strcmp(name, "vkCmdBindPipeline") == 0) return reinterpret_cast<PFN_vkVoidFunction>(BindPipeline);
    if (std::strcmp(name, "vkCmdDraw") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DrawCommands);
    return Trace->resolve(device, name);
}

struct OptimizationDrain {
    VkDevice device;
    ~OptimizationDrain() {
        Trace->ReleaseOptimization();
        WaitForOptimizedPipelines(device);
    }
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void CheckPixels(bool masked, std::string_view phase) {
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const bool drawn = masked ? (pixel % Width) % 2u == 0u : pixel % Width < Width / 2u;
        const auto expected = drawn ? std::byte{255} : Kept;
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            const auto actual = Pixels[pixel * 4u + channel];
            Require(actual == expected, std::string(phase) + ": pixel " + std::to_string(pixel) + " channel " + std::to_string(channel) + " is " + std::to_string(static_cast<unsigned>(actual)) + ", expected " + std::to_string(static_cast<unsigned>(expected)));
        }
    }
}

void CheckDrawn(PipelineTrace& trace, bool optimized) {
    std::lock_guard lock(trace.mutex);
    const auto& expected = optimized ? trace.optimized : trace.fast;
    Require(trace.drawn.size() == 1, "pipeline readback did not observe exactly one real draw");
    Require(std::find(expected.begin(), expected.end(), trace.drawn.front()) != expected.end(), optimized ? "readback draw did not bind an optimized pipeline" : "readback draw did not bind a fast-linked pipeline");
}

void Run(AgcDriver::VulkanDevice& device, Context context, PipelineTrace& trace, std::uint32_t waveSize, VkFormat depthFormat, bool optimize) {
    const bool masked = depthFormat == VK_FORMAT_UNDEFINED;
    const auto target = device.Target();
    const auto vertices = masked ? std::span<const std::array<float, 4>>(Triangle) : std::span<const std::array<float, 4>>(DepthTriangles);
    const auto vertexBuffer = BufferDescriptor(vertices.data(), 16u, static_cast<std::uint32_t>(vertices.size()));
    const std::vector<std::uint32_t> vertexUserData(vertexBuffer.begin(), vertexBuffer.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const auto pixelCode = masked ? std::span<const std::uint32_t>(MaskedPixelCode) : std::span<const std::uint32_t>(FullPixelCode);
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
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
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    if (!masked) {
        state.depth = DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), 0, {Width, Height}, depthFormat, 0.5f, 0};
        state.depthTest = true;
        state.depthCompare = VK_COMPARE_OP_LESS;
    }
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    const auto render = [&](bool optimized, std::string_view phase) {
        Pixels.fill(Kept);
        trace.drawn.clear();
        trace.bound = VK_NULL_HANDLE;
        trace.boundCommands = VK_NULL_HANDLE;
        AgcDriver::Graphics::Draw(context, state, draw, shaders);
        device.WaitIdle();
        CheckDrawn(trace, optimized);
        CheckPixels(masked, phase);
    };
    render(false, "fast link");
    {
        std::lock_guard lock(trace.mutex);
        Require(trace.libraries != 0, "the readback pipeline's libraries were not created through the traced device");
        Require(trace.retaining == (optimize ? trace.libraries : 0u), std::to_string(trace.retaining) + " of the " + std::to_string(trace.libraries) + " libraries were created to retain link-time optimization info, expected " + (optimize ? "all of them" : "none, since no optimized link will be made"));
    }
    trace.ReleaseOptimization();
    WaitForOptimizedPipelines(context.device);
    Require(PipelineLibraryCountersOf(context.device).linked == 1, "the readback pipeline was not linked from graphics libraries");
    Require(trace.optimized.size() == (optimize ? 1u : 0u), "the optimized pipeline creation count differs from the requested mode");
    render(optimize, "completed link");
    std::printf("pipeline readback passed: fast draws %u, optimized draws %u, optimized creations %zu, libraries %u (%u retaining link-time optimization info)\n", optimize ? 1u : 2u, optimize ? 1u : 0u, trace.optimized.size(), trace.libraries, trace.retaining);
}

}

int main(int argc, char** argv) {
    try {
        std::string_view mode = "d32";
        bool force = false;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument == "--force-lto") force = true;
            else {
                Require(argument == "color32" || argument == "color64" || argument == "d32" || argument == "d16", "unknown pipeline readback case");
                mode = argument;
            }
        }
        PipelineTrace trace;
        Trace = &trace;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        if (!context.graphicsPipelineLibrary) {
            std::puts("skipped, graphics pipeline libraries are unavailable or disabled");
            return VulkanTestSkipped;
        }
        const bool masked = mode == "color32" || mode == "color64";
        if (masked && device->Target().subgroupSize < 32u) {
            std::puts("skipped, the masked pixel shader requires at least 32 subgroup lanes");
            return VulkanTestSkipped;
        }
        if (force) context.pipelineLinkTimeOptimization = true;
        const bool optimize = context.pipelineLinkTimeOptimization && std::getenv("APS5_NO_PIPELINE_LTO") == nullptr;
        std::printf("pipeline readback: %s, %.*s, LTO %s%s\n", device->DeviceName().c_str(), static_cast<int>(mode.size()), mode.data(), optimize ? "enabled" : "disabled", force ? " (explicit test override)" : " (production policy)");
        trace.resolve = context.deviceProc;
        trace.create = context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines");
        trace.bind = context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline");
        trace.draw = context.Function<PFN_vkCmdDraw>("vkCmdDraw");
        trace.functions = context.functions != nullptr ? *context.functions : DeviceFunctions{};
        trace.functions.cmdBindPipeline = BindPipeline;
        trace.functions.cmdDraw = DrawCommands;
        context.functions = &trace.functions;
        context.deviceProc = Resolve;
        OptimizationDrain drain{context.device};
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Run(*device, context, trace, mode == "color32" ? 32u : 64u, masked ? VK_FORMAT_UNDEFINED : (mode == "d16" ? VK_FORMAT_D16_UNORM : VK_FORMAT_D32_SFLOAT), optimize);
        if (optimize) Require(AgcDriver::Graphics::PipelineLibraryWorkers() == 1, "the optimized pipeline was not built on the pipeline library worker");
        AgcDriver::Graphics::StopPipelineLibraryWorker();
        Require(AgcDriver::Graphics::PipelineLibraryWorkers() == 0, "stopping the pipeline library left its worker running");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
