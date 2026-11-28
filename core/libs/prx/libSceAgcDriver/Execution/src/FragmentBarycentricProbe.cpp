#include "prx/libSceAgcDriver/Execution/include/FragmentBarycentricProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/BarycentricProbeLinearFragCoord_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/BarycentricProbeLinear_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/BarycentricProbeSmoothFragCoord_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/BarycentricProbeSmooth_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/BarycentricProbeVertex_spv.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <span>

namespace AgcDriver {
namespace {

using Graphics::Check;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 48;
constexpr VkFormat Format = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr std::size_t Bytes = Width * Height * 4u * sizeof(float);
constexpr double Tolerance = 1e-3;

struct Corner {
    double x;
    double y;
    double w;
};

constexpr std::array<Corner, 3> Corners{{{-0.8, -0.8, 1.0}, {0.9, -0.6, 2.5}, {-0.5, 0.85, 0.5}}};

struct Weights {
    bool inside = false;
    std::array<double, 3> linear{};
    std::array<double, 3> perspective{};
};

Weights Expected(std::uint32_t x, std::uint32_t y, bool snapped) {
    std::array<std::array<double, 2>, 3> window{};
    for (std::size_t i = 0; i < Corners.size(); ++i) window[i] = {static_cast<float>(Corners[i].x) * Width / 2.0 + Width / 2.0, static_cast<float>(Corners[i].y) * Height / 2.0 + Height / 2.0};
    if (snapped) {
        for (auto& corner : window) {
            for (auto& coordinate : corner) coordinate = std::round(coordinate * 256.0) / 256.0;
        }
    }
    const double px = x + 0.5;
    const double py = y + 0.5;
    const auto edge = [](const std::array<double, 2>& a, const std::array<double, 2>& b, double cx, double cy) { return (b[0] - a[0]) * (cy - a[1]) - (b[1] - a[1]) * (cx - a[0]); };
    const double area = edge(window[0], window[1], window[2][0], window[2][1]);
    Weights weights;
    weights.linear = {edge(window[1], window[2], px, py) / area, edge(window[2], window[0], px, py) / area, edge(window[0], window[1], px, py) / area};
    weights.inside = std::min({weights.linear[0], weights.linear[1], weights.linear[2]}) > 0.02;
    double sum = 0.0;
    for (std::size_t i = 0; i < 3; ++i) sum += weights.linear[i] / Corners[i].w;
    for (std::size_t i = 0; i < 3; ++i) weights.perspective[i] = weights.linear[i] / Corners[i].w / sum;
    return weights;
}

template<typename Handle, typename Destroy>
class Owned {
public:
    Owned(VkDevice device, Destroy destroy) : device(device), destroy(destroy) {}
    ~Owned() {
        if (handle != VK_NULL_HANDLE) destroy(device, handle, nullptr);
    }
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    Handle handle = VK_NULL_HANDLE;

private:
    VkDevice device;
    Destroy destroy;
};

FragmentBarycentricProbeMode Run(const Graphics::Context& context, std::span<const std::uint32_t> fragmentCode, bool perspective) {
    const Graphics::RenderTarget target(context, Graphics::ColorTarget{0u, {Width, Height}, Format, Bytes, 0xe4u}, false);
    Graphics::Buffer readback(context, Bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto device = context.device;

    VkAttachmentDescription attachment{};
    attachment.format = Format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkAttachmentReference reference{0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = 0;
    dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &attachment;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 1;
    passInfo.pDependencies = &dependency;
    Owned<VkRenderPass, PFN_vkDestroyRenderPass> pass(device, context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass"));
    Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(device, &passInfo, nullptr, &pass.handle), "vkCreateRenderPass barycentric probe");

    const auto view = target.View();
    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = pass.handle;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &view;
    framebufferInfo.width = Width;
    framebufferInfo.height = Height;
    framebufferInfo.layers = 1;
    Owned<VkFramebuffer, PFN_vkDestroyFramebuffer> framebuffer(device, context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer"));
    Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(device, &framebufferInfo, nullptr, &framebuffer.handle), "vkCreateFramebuffer barycentric probe");

    const auto createModule = context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule");
    const auto destroyModule = context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
    const auto module = [&](Owned<VkShaderModule, PFN_vkDestroyShaderModule>& owned, std::span<const std::uint32_t> code) {
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size_bytes();
        info.pCode = code.data();
        Check(createModule(device, &info, nullptr, &owned.handle), "vkCreateShaderModule barycentric probe");
    };
    Owned<VkShaderModule, PFN_vkDestroyShaderModule> vertexModule(device, destroyModule);
    Owned<VkShaderModule, PFN_vkDestroyShaderModule> fragmentModule(device, destroyModule);
    module(vertexModule, BARYCENTRIC_PROBE_VERTEX_SPV);
    module(fragmentModule, fragmentCode);

    const VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    Owned<VkPipelineLayout, PFN_vkDestroyPipelineLayout> layout(device, context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout"));
    Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(device, &layoutInfo, nullptr, &layout.handle), "vkCreatePipelineLayout barycentric probe");

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertexModule.handle, "main", nullptr};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule.handle, "main", nullptr};
    const VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f};
    const VkRect2D scissor{{0, 0}, {Width, Height}};
    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo rasterization{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &assembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.layout = layout.handle;
    pipelineInfo.renderPass = pass.handle;
    Owned<VkPipeline, PFN_vkDestroyPipeline> pipeline(device, context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline"));
    Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.handle), "vkCreateGraphicsPipelines barycentric probe");

    {
        Graphics::CommandBatch batch(context);
        const auto commands = batch.Handle();
        VkClearValue clear{};
        clear.color = {{-7.0f, -7.0f, -7.0f, -7.0f}};
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = pass.handle;
        begin.framebuffer = framebuffer.handle;
        begin.renderArea = scissor;
        begin.clearValueCount = 1;
        begin.pClearValues = &clear;
        context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        Graphics::CommandState state;
        state.Begin(commands, false);
        state.BindPipeline(context, pipeline.handle, 0);
        context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3u, 1u, 0u, 0u);
        context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        copy.imageExtent = {Width, Height, 1u};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, target.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.Handle(), 1u, &copy);
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
    }
    readback.Invalidate();
    std::array<float, Width * Height * 4u> pixels{};
    std::memcpy(pixels.data(), readback.Bytes().data(), Bytes);

    FragmentBarycentricProbeMode mode;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto unsnapped = Expected(x, y, false);
            if (!unsnapped.inside) continue;
            const auto snapped = Expected(x, y, true);
            const auto& reference = perspective ? unsnapped.perspective : unsnapped.linear;
            const auto& snappedReference = perspective ? snapped.perspective : snapped.linear;
            for (std::uint32_t component = 0; component < 3; ++component) {
                const double value = pixels[(y * Width + x) * 4u + component];
                ++mode.compared;
                if (!(std::fabs(value - reference[component]) <= Tolerance || std::fabs(value - snappedReference[component]) <= Tolerance)) ++mode.mismatches;
            }
        }
    }
    return mode;
}

}

FragmentBarycentricProbeResult ProbeFragmentBarycentricBuiltins(const Graphics::Context& context, bool readFragCoord) {
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    FragmentBarycentricProbeResult result;
    result.perspective = Run(context, readFragCoord ? std::span<const std::uint32_t>(BARYCENTRIC_PROBE_SMOOTHFRAGCOORD_SPV) : std::span<const std::uint32_t>(BARYCENTRIC_PROBE_SMOOTH_SPV), true);
    result.linear = Run(context, readFragCoord ? std::span<const std::uint32_t>(BARYCENTRIC_PROBE_LINEARFRAGCOORD_SPV) : std::span<const std::uint32_t>(BARYCENTRIC_PROBE_LINEAR_SPV), false);
    return result;
}

}
