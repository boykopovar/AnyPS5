#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

void LogPipelineStatistics_nid_no_patch(const Context& context, VkPipeline pipeline) {
    if (!context.pipelineExecutableInfo) return;
    const auto getProperties = context.Function<PFN_vkGetPipelineExecutablePropertiesKHR>("vkGetPipelineExecutablePropertiesKHR");
    const auto getStatistics = context.Function<PFN_vkGetPipelineExecutableStatisticsKHR>("vkGetPipelineExecutableStatisticsKHR");
    VkPipelineInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
    pipelineInfo.pipeline = pipeline;
    std::uint32_t count = 0;
    Check(getProperties(context.device, &pipelineInfo, &count, nullptr), "vkGetPipelineExecutablePropertiesKHR count");
    std::vector<VkPipelineExecutablePropertiesKHR> properties(count, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR});
    Check(getProperties(context.device, &pipelineInfo, &count, properties.data()), "vkGetPipelineExecutablePropertiesKHR");
    properties.resize(count);
    for (std::uint32_t index = 0; index < properties.size(); ++index) {
        VkPipelineExecutableInfoKHR executable{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
        executable.pipeline = pipeline;
        executable.executableIndex = index;
        count = 0;
        Check(getStatistics(context.device, &executable, &count, nullptr), "vkGetPipelineExecutableStatisticsKHR count");
        std::vector<VkPipelineExecutableStatisticKHR> statistics(count, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR});
        Check(getStatistics(context.device, &executable, &count, statistics.data()), "vkGetPipelineExecutableStatisticsKHR");
        statistics.resize(count);
        const auto& property = properties[index];
        std::fprintf(stderr, "[pipeline-stats] executable=%u stages=0x%x subgroup=%u name=%s\n", index, property.stages, property.subgroupSize, property.name);
        for (const auto& statistic : statistics) {
            std::fprintf(stderr, "[pipeline-stats] executable=%u %s=", index, statistic.name);
            switch (statistic.format) {
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
                std::fprintf(stderr, "%s", statistic.value.b32 ? "true" : "false");
                break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
                std::fprintf(stderr, "%lld", static_cast<long long>(statistic.value.i64));
                break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
                std::fprintf(stderr, "%llu", static_cast<unsigned long long>(statistic.value.u64));
                break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:
                std::fprintf(stderr, "%.17g", statistic.value.f64);
                break;
            default:
                throw std::runtime_error("unsupported pipeline statistic format");
            }
            std::fprintf(stderr, " (%s)\n", statistic.description);
        }
    }
}

Framebuffer::Framebuffer(const Context& context, VkRenderPass renderPass, std::span<const VkImageView> targets, VkExtent2D extent) : context(context) {
    // Cached objects outlive their device's teardown; they must not keep its buffer pool alive past it.
    this->context.bufferPool.reset();
    Require(extent.width != 0 && extent.height != 0 && extent.width <= context.limits.maxFramebufferWidth && extent.height <= context.limits.maxFramebufferHeight, "framebuffer extent exceeds device limits");
    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<std::uint32_t>(targets.size());
    framebufferInfo.pAttachments = targets.empty() ? nullptr : targets.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;
    Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
}

Framebuffer::~Framebuffer() {
    if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
}

void ValidateDepthBounds(const Context& context, const State& state) {
    Require(!state.depthBoundsTest || context.depthRangeUnrestricted || (state.minDepthBounds >= 0.0f && state.minDepthBounds <= 1.0f && state.maxDepthBounds >= 0.0f && state.maxDepthBounds <= 1.0f), "depth bounds outside [0, 1] require VK_EXT_depth_range_unrestricted");
}

void ValidateProvokingVertex(const Context& context, const State& state, std::span<const CompiledShader> shaders) {
    Require(state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT || state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT, "invalid provoking vertex mode");
    if (state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT) return;
    Require(context.provokingVertexLast, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
    Require(!state.rectList && state.stages.path == ShaderPath::Vertex && !state.stages.mesh && !state.stages.tessellation, "last provoking vertex is unsupported for generated primitive pipelines");
    Require(state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, "last provoking vertex is unsupported for this primitive topology");
    for (const auto& shader : shaders) {
        if (shader.stage != ShaderRecompiler::ShaderStage::Fragment) continue;
        Require(shader.program != nullptr, "missing compiled fragment shader");
        Require(std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.flat && parameter.perVertex; }), "last provoking vertex with explicit per-vertex flat interpolation is unsupported");
        Require(state.topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.perVertex; }), "last provoking vertex with explicit per-vertex triangle-strip interpolation is unsupported");
    }
}

void ValidateViewport(const Context& context, const VkViewport& viewport) {
    Require(std::isfinite(viewport.minDepth) && std::isfinite(viewport.maxDepth), "non-finite viewport depth range");
    Require(context.depthRangeUnrestricted || (viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1), "viewport depth outside [0, 1] requires VK_EXT_depth_range_unrestricted");
    Require(std::isfinite(viewport.x) && std::isfinite(viewport.y) && std::isfinite(viewport.width) && std::isfinite(viewport.height), "viewport arithmetic overflow");
    Require(viewport.width <= context.limits.maxViewportDimensions[0] && std::abs(viewport.height) <= context.limits.maxViewportDimensions[1], "viewport dimensions exceed device limits");
    Require(viewport.x >= context.limits.viewportBoundsRange[0] && viewport.x + viewport.width <= context.limits.viewportBoundsRange[1], "viewport X exceeds device bounds");
    Require(std::min(viewport.y, viewport.y + viewport.height) >= context.limits.viewportBoundsRange[0] && std::max(viewport.y, viewport.y + viewport.height) <= context.limits.viewportBoundsRange[1], "viewport Y exceeds device bounds");
}

namespace {

template<typename TValue>
void appendKey(std::vector<std::byte>& key, const TValue& value) {
    static_assert(std::is_trivially_copyable_v<TValue>);
    const auto bytes = std::as_bytes(std::span(&value, 1));
    key.insert(key.end(), bytes.begin(), bytes.end());
}

bool libraryKeys(PipelineLibraryKeys& keys, const Context& context, const State& state, const VertexInputLayout& input, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    using Stage = ShaderRecompiler::ShaderStage;
    if (!context.graphicsPipelineLibrary || context.pipelineExecutableInfo || state.depthBiasPerFace || state.rectList) return false;
    keys = {};
    for (const auto& shader : shaders) {
        if (shader.stage == Stage::Geometry || shader.program->PipelineVariantId() == 0) return false;
        auto& key = shader.stage == Stage::Fragment ? keys.fragmentShader : keys.preRasterization;
        appendKey(key, shader.stage);
        appendKey(key, shader.program->PipelineVariantId());
        appendKey(key, shader.pushConstantOffset);
    }
    for (const auto& binding : input.bindings) {
        appendKey(keys.vertexInput, binding.binding);
        appendKey(keys.vertexInput, binding.stride);
        appendKey(keys.vertexInput, binding.inputRate);
    }
    appendKey(keys.vertexInput, input.attributes.size());
    for (const auto& attribute : input.attributes) {
        appendKey(keys.vertexInput, attribute.location);
        appendKey(keys.vertexInput, attribute.binding);
        appendKey(keys.vertexInput, attribute.format);
        appendKey(keys.vertexInput, attribute.offset);
    }
    appendKey(keys.vertexInput, state.topology);
    appendKey(keys.vertexInput, state.primitiveRestart);
    appendKey(keys.preRasterization, state.provokingVertexMode);
    appendKey(keys.preRasterization, state.negativeOneToOne);
    appendKey(keys.preRasterization, state.depthClamp && context.depthClamp);
    appendKey(keys.preRasterization, state.conservativeRasterization);
    appendKey(keys.preRasterization, state.depth.has_value() && state.depthBias);
    appendKey(keys.preRasterization, state.stages.tessellation.has_value());
    if (state.stages.tessellation) appendKey(keys.preRasterization, state.stages.tessellation->inputControlPoints);
    appendKey(keys.fragmentShader, state.depth.has_value());
    for (const auto& blend : state.blends) appendKey(keys.fragmentOutput, blend);
    for (const auto value : state.blendConstants) appendKey(keys.fragmentOutput, value);
    appendKey(keys.renderPass, attachmentLayout);
    appendKey(keys.renderPass, state.blends.size());
    for (const auto& color : state.colors) {
        appendKey(keys.renderPass, color.format);
        appendKey(keys.renderPass, color.exportIndex);
    }
    appendKey(keys.renderPass, state.depth.has_value());
    if (state.depth) appendKey(keys.renderPass, state.depth->format);
    appendKey(keys.layout, PushConstantStages(shaders));
    for (const auto word : resources.LayoutKey()) appendKey(keys.layout, word);
    return true;
}

}

Pipeline::Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) : context(context), _modules(shaders.size()), attachments(state.colors.size() + (state.depth ? 1u : 0u)), colorAttachments(state.colors.size()), depthBounds(state.depth.has_value() && state.depthBoundsTest), depthBias(state.depth.has_value() && state.depthBias) {
    PerformanceTimer timing("Vulkan.GraphicsPipeline");
    // A cached pipeline may outlive its device's teardown (see ClearCachedPipelines); it must not keep
    // the buffer pool, which is reset with the device, alive past it.
    this->context.bufferPool.reset();
    Require(state.blends.size() == (state.colors.empty() ? 0u : state.colors.back().exportIndex + 1u) && state.colors.size() <= state.blends.size(), "blend states do not match decoded color state");
    Require(state.blends.size() <= context.limits.maxColorAttachments, "color targets exceed device attachment limits");
    Require(state.hasColorTarget || (context.limits.framebufferNoAttachmentsSampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0, "device does not support single-sample rendering without attachments");
    Require(!depthBounds || context.depthBounds, "device does not support the depth bounds test");
    Require(!depthBias || state.depthBiasClamp == 0.0f || context.depthBiasClamp, "device does not support depth bias clamping");
    Require(!state.dualSourceBlend || (context.dualSrcBlend && state.blends.size() <= context.limits.maxFragmentDualSrcAttachments), "device does not support dual-source blending into this many attachments");
    Require(!state.negativeOneToOne || context.depthClipControl, "negative-one-to-one depth clipping requires VK_EXT_depth_clip_control with depthClipControl enabled");
    Require(state.conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT || context.conservativeRasterization, "conservative rasterization requires VK_EXT_conservative_rasterization with at most 1/256 pixel of overestimation and degenerate triangles rasterized");
    ValidateProvokingVertex(context, state, shaders);
    if (state.rectList) Require(context.tessellationShader && context.limits.maxTessellationPatchSize >= 4, "rect-list requires tessellation with four output control points");
    if (std::any_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.stage == ShaderRecompiler::ShaderStage::Geometry; })) Require(context.geometryShader, "device does not support geometry shaders");
    if (state.stages.tessellation) {
        Require(context.tessellationShader, "device does not support tessellation shaders");
        Require(state.stages.tessellation->inputControlPoints <= context.limits.maxTessellationPatchSize && state.stages.tessellation->outputControlPoints <= context.limits.maxTessellationPatchSize, "tessellation patch exceeds device limits");
    }
    if (state.stages.mesh) {
        Require(context.meshShader, "device does not support VK_EXT_mesh_shader");
        const auto& mesh = *state.stages.mesh;
        const auto invocations = state.stages.vertexWaveSize == 64u && context.subgroup.subgroupSize == 32u ? mesh.threadsPerGroup / 2u : mesh.threadsPerGroup;
        Require(invocations <= context.meshLimits.maxMeshWorkGroupInvocations && invocations <= context.meshLimits.maxMeshWorkGroupSize[0], "mesh workgroup exceeds device limits");
        Require(mesh.maxVertices <= context.meshLimits.maxMeshOutputVertices && mesh.maxPrimitives <= context.meshLimits.maxMeshOutputPrimitives && static_cast<std::uint64_t>(mesh.ldsSizeDwords) * 4 <= context.meshLimits.maxMeshSharedMemorySize, "mesh output or LDS exceeds device limits");
    }
    const auto pushStages = PushConstantStages(shaders);
    Require(pushStages == 0 || context.limits.maxPushConstantsSize >= PipelinePushConstantBytes, "graphics push constant range exceeds device limit");
    try {
        std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
        std::vector<PipelineSpecialization> specializations;
        specializations.reserve(shaders.size());
        for (std::uint32_t i = 0; i < shaders.size(); ++i) {
            const auto& shader = *shaders[i].program;
            VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            module.codeSize = shader.spirv.size() * sizeof(std::uint32_t);
            module.pCode = shader.spirv.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &module, nullptr, &_modules[i]), "vkCreateShaderModule graphics");
            const auto stage = VulkanStage(shaders[i].stage);
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = stage;
            stages[i].module = _modules[i];
            stages[i].pName = "main";
            specializations.emplace_back(shader);
            stages[i].pSpecializationInfo = specializations.back().Info();
        }
        // A descriptor set layout with the same bindings as this one is compatible with the pipeline
        // layout, so later draws bind their own ShaderResources' set under it.
        const auto setLayout = resources.Layout();
        const VkPushConstantRange push{pushStages, 0, PipelinePushConstantBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
        layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout graphics");
        std::vector<VkAttachmentDescription> colors;
        std::vector<VkAttachmentReference> references(state.blends.size(), VkAttachmentReference{VK_ATTACHMENT_UNUSED, attachmentLayout});
        for (std::uint32_t index = 0; index < state.colors.size(); ++index) {
            VkAttachmentDescription color{};
            color.format = state.colors[index].format;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = attachmentLayout;
            color.finalLayout = attachmentLayout;
            colors.push_back(color);
            references.at(state.colors[index].exportIndex) = {index, attachmentLayout};
        }
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<std::uint32_t>(references.size());
        subpass.pColorAttachments = references.empty() ? nullptr : references.data();
        const VkAttachmentReference depthReference{static_cast<std::uint32_t>(colors.size()), VK_IMAGE_LAYOUT_GENERAL};
        if (state.depth) {
            VkAttachmentDescription depth{};
            depth.format = state.depth->format;
            depth.samples = VK_SAMPLE_COUNT_1_BIT;
            depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
            depth.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
            colors.push_back(depth);
            subpass.pDepthStencilAttachment = &depthReference;
        }
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = static_cast<std::uint32_t>(colors.size());
        passInfo.pAttachments = colors.empty() ? nullptr : colors.data();
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass");
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexInput.bindings.size());
        input.pVertexBindingDescriptions = vertexInput.bindings.data();
        input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexInput.attributes.size());
        input.pVertexAttributeDescriptions = vertexInput.attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = state.topology;
        const bool listTopology = state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        assembly.primitiveRestartEnable = state.primitiveRestart && (!listTopology || context.primitiveListRestart) ? VK_TRUE : VK_FALSE;
        // Viewport and scissor are set per draw (Begin), so they do not multiply pipelines; the depth
        // clip control stays baked in.
        VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
        depthClip.negativeOneToOne = state.negativeOneToOne;
        if (state.negativeOneToOne) viewports.pNext = &depthClip;
        viewports.viewportCount = 1;
        viewports.scissorCount = 1;
        PipelineLibraryKeys keys;
        libraries = libraryKeys(keys, context, state, vertexInput, resources, shaders, attachmentLayout);
        std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        if (depthBounds) dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        if (depthBias) dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        if (libraries) dynamicStates.assign(PipelineLibraryDynamicStates().begin(), PipelineLibraryDynamicStates().end());
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.depthClampEnable = state.depthClamp && context.depthClamp ? VK_TRUE : VK_FALSE;
        Require(!state.depthBiasPerFace || (depthBias && state.cullMode == VK_CULL_MODE_NONE), "per-face depth bias needs both faces rasterized");
        raster.cullMode = state.depthBiasPerFace ? VK_CULL_MODE_BACK_BIT : state.cullMode;
        raster.frontFace = state.frontFace;
        raster.depthBiasEnable = depthBias;
        raster.lineWidth = 1;
        VkPipelineRasterizationConservativeStateCreateInfoEXT conservative{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT};
        conservative.conservativeRasterizationMode = state.conservativeRasterization;
        if (state.conservativeRasterization != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT) raster.pNext = &conservative;
        VkPipelineRasterizationProvokingVertexStateCreateInfoEXT provoking{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT};
        provoking.provokingVertexMode = state.provokingVertexMode;
        if (context.provokingVertexLast) {
            provoking.pNext = raster.pNext;
            raster.pNext = &provoking;
        }
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = state.depthTest;
        depthStencil.depthWriteEnable = state.depthWrite;
        depthStencil.depthCompareOp = state.depthCompare;
        depthStencil.depthBoundsTestEnable = depthBounds;
        depthStencil.stencilTestEnable = state.stencilTest;
        depthStencil.front = state.stencilFront;
        depthStencil.back = state.stencilBack;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = static_cast<std::uint32_t>(state.blends.size());
        blend.pAttachments = state.blends.empty() ? nullptr : state.blends.data();
        std::copy(state.blendConstants.begin(), state.blendConstants.end(), blend.blendConstants);
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.flags = context.pipelineExecutableInfo ? VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR : 0;
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        VkPipelineTessellationStateCreateInfo tessellation{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        if (state.rectList || state.stages.tessellation) {
            tessellation.patchControlPoints = state.rectList ? 3u : state.stages.tessellation->inputControlPoints;
            pipelineInfo.pTessellationState = &tessellation;
        }
        pipelineInfo.pVertexInputState = state.stages.mesh ? nullptr : &input;
        pipelineInfo.pInputAssemblyState = state.stages.mesh ? nullptr : &assembly;
        pipelineInfo.pViewportState = &viewports;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &samples;
        pipelineInfo.pDepthStencilState = state.depth ? &depthStencil : nullptr;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = renderPass;
        timing.Mark("modules_and_state");
        if (libraries) pipeline = LinkPipelineFromLibraries(context, pipelineInfo, passInfo, layoutInfo, keys);
        else Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines");
        if (state.depthBiasPerFace) {
            raster.cullMode = VK_CULL_MODE_FRONT_BIT;
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &backFaces), "vkCreateGraphicsPipelines");
        }
        for (const auto module : _modules) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        _modules.clear();
        timing.Mark("create");
        LogPipelineStatistics_nid_no_patch(context, pipeline);
    } catch (...) {
        release();
        throw;
    }
}

Pipeline::~Pipeline() {
    release();
}

void Pipeline::release() noexcept {
    framebuffers.clear();
    if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    if (backFaces) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, backFaces, nullptr);
    if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
    for (auto module : _modules) {
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    }
    pipeline = VK_NULL_HANDLE;
    backFaces = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    _modules.clear();
}

void Pipeline::Abandon() noexcept {
    for (auto& entry : framebuffers) entry.framebuffer->Abandon();
    framebuffers.clear();
    pipeline = VK_NULL_HANDLE;
    backFaces = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    _modules.clear();
}

void Framebuffer::Abandon() noexcept {
    framebuffer = VK_NULL_HANDLE;
}

VkPipelineLayout Pipeline::Layout() const {
    return layout;
}

std::shared_ptr<Framebuffer> Pipeline::AcquireFramebuffer(std::span<const VkImageView> targets, std::span<const std::shared_ptr<StorageTexture>> owners, VkExtent2D extent) {
    Require(targets.size() == attachments && owners.size() == colorAttachments, "render targets do not match the pipeline's attachments");
    const bool resident = std::all_of(owners.begin(), owners.end(), [](const auto& owner) { return owner != nullptr; });
    if (resident) {
        // Entries whose views are gone can never match again and go as soon as no recorded draw holds
        // them (Kept keeps its framebuffer until the batch completes).
        std::erase_if(framebuffers, [](const CachedFramebuffer& entry) {
            return entry.framebuffer.use_count() == 1 && std::any_of(entry.owners.begin(), entry.owners.end(), [](const auto& owner) { return owner.expired(); });
        });
        for (auto it = framebuffers.begin(); it != framebuffers.end(); ++it) {
            if (it->extent.width != extent.width || it->extent.height != extent.height || !std::equal(it->views.begin(), it->views.end(), targets.begin(), targets.end())) continue;
            // View handles are recycled once a StorageTexture is destroyed, so the owners must be the
            // very objects the views were made for.
            bool same = true;
            for (std::size_t i = 0; i < owners.size() && same; ++i) same = it->owners[i].lock().get() == owners[i].get();
            if (!same) continue;
            std::rotate(it, std::next(it), framebuffers.end());
            return framebuffers.back().framebuffer;
        }
    }
    auto framebuffer = std::make_shared<Framebuffer>(context, renderPass, targets, extent);
    if (!resident) return framebuffer;
    // Beyond the bound the least recently used unreferenced entry goes.
    constexpr std::size_t bound = 8;
    while (framebuffers.size() >= bound) {
        const auto victim = std::find_if(framebuffers.begin(), framebuffers.end(), [](const CachedFramebuffer& entry) { return entry.framebuffer.use_count() == 1; });
        if (victim == framebuffers.end()) break;
        framebuffers.erase(victim);
    }
    CachedFramebuffer entry;
    entry.views.assign(targets.begin(), targets.end());
    entry.owners.assign(owners.begin(), owners.end());
    entry.extent = extent;
    entry.framebuffer = framebuffer;
    framebuffers.push_back(std::move(entry));
    return framebuffer;
}

void Pipeline::Begin(VkCommandBuffer commands, const Framebuffer& framebuffer, VkExtent2D extent, const State& state) const {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = renderPass;
    begin.framebuffer = framebuffer.Handle();
    begin.renderArea = {{0, 0}, extent};
    context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    Continue(commands, state);
}

void Pipeline::Continue(VkCommandBuffer commands, const State& state) const {
    context.Resolved(&DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    context.Resolved(&DeviceFunctions::cmdSetViewport, "vkCmdSetViewport")(commands, 0, 1, &state.viewport);
    context.Resolved(&DeviceFunctions::cmdSetScissor, "vkCmdSetScissor")(commands, 0, 1, &state.scissor);
    if (libraries) {
        setLibraryState(commands, state);
        return;
    }
    if (depthBias) context.Resolved(&DeviceFunctions::cmdSetDepthBias, "vkCmdSetDepthBias")(commands, state.depthBiasConstant, state.depthBiasClamp, state.depthBiasSlope);
    if (!depthBounds) return;
    context.Resolved(&DeviceFunctions::cmdSetDepthBounds, "vkCmdSetDepthBounds")(commands, state.minDepthBounds, state.maxDepthBounds);
}

void Pipeline::setLibraryState(VkCommandBuffer commands, const State& state) const {
    const bool depth = state.depth.has_value();
    context.Resolved(&DeviceFunctions::cmdSetCullMode, "vkCmdSetCullModeEXT")(commands, state.cullMode);
    context.Resolved(&DeviceFunctions::cmdSetFrontFace, "vkCmdSetFrontFaceEXT")(commands, state.frontFace);
    context.Resolved(&DeviceFunctions::cmdSetDepthBias, "vkCmdSetDepthBias")(commands, depthBias ? state.depthBiasConstant : 0.0f, depthBias ? state.depthBiasClamp : 0.0f, depthBias ? state.depthBiasSlope : 0.0f);
    context.Resolved(&DeviceFunctions::cmdSetDepthBounds, "vkCmdSetDepthBounds")(commands, depthBounds ? state.minDepthBounds : 0.0f, depthBounds ? state.maxDepthBounds : 1.0f);
    context.Resolved(&DeviceFunctions::cmdSetDepthTestEnable, "vkCmdSetDepthTestEnableEXT")(commands, depth && state.depthTest);
    context.Resolved(&DeviceFunctions::cmdSetDepthWriteEnable, "vkCmdSetDepthWriteEnableEXT")(commands, depth && state.depthWrite);
    context.Resolved(&DeviceFunctions::cmdSetDepthCompareOp, "vkCmdSetDepthCompareOpEXT")(commands, depth ? state.depthCompare : VK_COMPARE_OP_ALWAYS);
    context.Resolved(&DeviceFunctions::cmdSetDepthBoundsTestEnable, "vkCmdSetDepthBoundsTestEnableEXT")(commands, depthBounds);
    context.Resolved(&DeviceFunctions::cmdSetStencilTestEnable, "vkCmdSetStencilTestEnableEXT")(commands, depth && state.stencilTest);
    const auto stencil = [&](VkStencilFaceFlags face, const VkStencilOpState& op) {
        context.Resolved(&DeviceFunctions::cmdSetStencilOp, "vkCmdSetStencilOpEXT")(commands, face, op.failOp, op.passOp, op.depthFailOp, op.compareOp);
        context.Resolved(&DeviceFunctions::cmdSetStencilCompareMask, "vkCmdSetStencilCompareMask")(commands, face, op.compareMask);
        context.Resolved(&DeviceFunctions::cmdSetStencilWriteMask, "vkCmdSetStencilWriteMask")(commands, face, op.writeMask);
        context.Resolved(&DeviceFunctions::cmdSetStencilReference, "vkCmdSetStencilReference")(commands, face, op.reference);
    };
    stencil(VK_STENCIL_FACE_FRONT_BIT, depth ? state.stencilFront : VkStencilOpState{});
    stencil(VK_STENCIL_FACE_BACK_BIT, depth ? state.stencilBack : VkStencilOpState{});
}

void Pipeline::ContinueBackFaces(VkCommandBuffer commands, const State& state) const {
    Require(backFaces != VK_NULL_HANDLE, "the pipeline draws both faces in one pass");
    context.Resolved(&DeviceFunctions::cmdBindPipeline, "vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, backFaces);
    context.Resolved(&DeviceFunctions::cmdSetViewport, "vkCmdSetViewport")(commands, 0, 1, &state.viewport);
    context.Resolved(&DeviceFunctions::cmdSetScissor, "vkCmdSetScissor")(commands, 0, 1, &state.scissor);
    context.Resolved(&DeviceFunctions::cmdSetDepthBias, "vkCmdSetDepthBias")(commands, state.backDepthBiasConstant, state.depthBiasClamp, state.backDepthBiasSlope);
    if (depthBounds) context.Resolved(&DeviceFunctions::cmdSetDepthBounds, "vkCmdSetDepthBounds")(commands, state.minDepthBounds, state.maxDepthBounds);
}

void Pipeline::PushConstants(VkCommandBuffer commands, VkShaderStageFlags stages, std::span<const std::byte, PipelinePushConstantBytes> bytes) const {
    if (stages == 0) return;
    context.Resolved(&DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, layout, stages, 0, PipelinePushConstantBytes, bytes.data());
}

namespace {

template<typename TValue>
void append(std::vector<std::byte>& key, const TValue& value) {
    static_assert(std::is_trivially_copyable_v<TValue>);
    const auto bytes = std::as_bytes(std::span(&value, 1));
    key.insert(key.end(), bytes.begin(), bytes.end());
}

// Everything the Pipeline objects are built from, or empty when a stage's result has no variant id
// (the recompiler could not identify it, so nothing else may share its pipeline). The rect-list
// control and evaluation stages are generated from the vertex and fragment results, which the key
// already names, so they carry no id of their own.
void pipelineKey(std::vector<std::byte>& key, const Context& context, const State& state, const VertexInputLayout& input, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    using Stage = ShaderRecompiler::ShaderStage;
    key.clear();
    append(key, context.device);
    append(key, attachmentLayout);
    append(key, shaders.size());
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        const bool generated = (state.rectList && (shader.stage == Stage::TessellationControl || shader.stage == Stage::TessellationEvaluation)) || shader.stage == Stage::Geometry;
        if (!generated && shader.program->PipelineVariantId() == 0) {
            key.clear();
            return;
        }
        append(key, shader.stage);
        append(key, generated ? std::uint64_t{0} : shader.program->PipelineVariantId());
        // Where the stage's push constants sit in the block (AssemblePushConstants).
        append(key, shader.pushConstantOffset);
    }
    append(key, PushConstantStages(shaders));
    append(key, input.bindings.size());
    for (const auto& binding : input.bindings) {
        append(key, binding.binding);
        append(key, binding.stride);
        append(key, binding.inputRate);
    }
    append(key, input.attributes.size());
    for (const auto& attribute : input.attributes) {
        append(key, attribute.location);
        append(key, attribute.binding);
        append(key, attribute.format);
        append(key, attribute.offset);
    }
    append(key, resources.LayoutKey().size());
    for (const auto word : resources.LayoutKey()) append(key, word);
    append(key, state.hasColorTarget);
    append(key, state.rectList);
    append(key, state.topology);
    append(key, state.primitiveRestart);
    append(key, state.cullMode);
    append(key, state.frontFace);
    append(key, state.provokingVertexMode);
    append(key, state.negativeOneToOne);
    append(key, state.depthClamp && context.depthClamp);
    append(key, state.conservativeRasterization);
    append(key, state.blends.size());
    for (const auto& blend : state.blends) append(key, blend);
    for (const auto value : state.blendConstants) append(key, value);
    append(key, state.colors.size());
    for (const auto& color : state.colors) append(key, color.format);
    if (state.blends.size() != state.colors.size()) {
        for (const auto& color : state.colors) append(key, color.exportIndex);
    }
    append(key, state.depth.has_value());
    if (state.depth) {
        append(key, state.depth->format);
        append(key, state.depthTest);
        append(key, state.depthWrite);
        append(key, state.depthCompare);
        append(key, state.depthBoundsTest);
        append(key, state.depthBias);
        append(key, state.depthBiasPerFace);
        append(key, state.stencilTest);
        append(key, state.stencilFront);
        append(key, state.stencilBack);
    }
    append(key, state.stages.mesh.has_value());
    if (state.stages.mesh) {
        const auto& mesh = *state.stages.mesh;
        append(key, mesh.inputPrimitive);
        append(key, mesh.primitivesPerGroup);
        append(key, mesh.verticesPerGroup);
        append(key, mesh.maxVertices);
        append(key, mesh.maxPrimitives);
        append(key, mesh.threadsPerGroup);
        append(key, mesh.ldsSizeDwords);
        append(key, mesh.provokingVertex);
    }
    append(key, state.stages.tessellation.has_value());
    if (state.stages.tessellation) {
        const auto& tessellation = *state.stages.tessellation;
        append(key, tessellation.inputControlPoints);
        append(key, tessellation.outputControlPoints);
        append(key, tessellation.domain);
        append(key, tessellation.partitioning);
        append(key, tessellation.outputTopology);
    }
}

std::uint64_t hashKey(const std::vector<std::byte>& key) {
    return std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(key.data()), key.size()));
}

struct PipelineStore {
    struct Entry {
        VkDevice device;
        // The device's buffer pool at insertion: it is made and reset with the device, so it tells
        // the device instance apart from a later one the loader gave the same handle value.
        std::weak_ptr<BufferPool> pool;
        std::uint64_t hash;
        std::vector<std::byte> key;
        std::shared_ptr<Pipeline> pipeline;
    };
    std::mutex mutex;
    std::vector<std::byte> scratchKey;
    // Least recently used first.
    std::list<Entry> entries;
    std::unordered_map<std::uint64_t, std::list<Entry>::iterator> index;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t uncached = 0;
    std::uint64_t evicted = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

// Never destroyed: the pipelines belong to a device that may already be gone when statics die, and
// the device's teardown (ClearCachedPipelines) is the place to destroy them.
PipelineStore& Pipelines() {
    static auto* store = new PipelineStore();
    return *store;
}

// Whether the entry's objects belong to the device the context names. A context without a buffer
// pool (tests) is identified by the handle alone.
bool alive(const PipelineStore::Entry& entry, const Context& context) {
    if (entry.device != context.device) return false;
    return context.bufferPool == nullptr || entry.pool.lock() == context.bufferPool;
}

// Drops an entry of a device that is gone: its objects went with the device, so they are forgotten,
// not destroyed.
std::list<PipelineStore::Entry>::iterator abandon(PipelineStore& store, std::list<PipelineStore::Entry>::iterator it) {
    it->pipeline->Abandon();
    store.index.erase(it->hash);
    return store.entries.erase(it);
}

void reportPipelines(PipelineStore& store) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - store.lastReport < std::chrono::seconds(10)) return;
    store.lastReport = now;
    const auto lookups = store.hits + store.misses + store.uncached;
    AgcDriver::ProfilePrint_nid_no_patch( "[pipecache] %llu lookups over 10 s: %llu hits (%.0f%%), %llu misses, %llu private (no variant id), %llu evicted, %zu cached\n", static_cast<unsigned long long>(lookups), static_cast<unsigned long long>(store.hits), lookups != 0 ? 100.0 * static_cast<double>(store.hits) / static_cast<double>(lookups) : 0.0, static_cast<unsigned long long>(store.misses), static_cast<unsigned long long>(store.uncached), static_cast<unsigned long long>(store.evicted), store.entries.size());
    store.hits = store.misses = store.uncached = store.evicted = 0;
}

}

std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    static const bool disabled = std::getenv("APS5_NO_PIPELINE_CACHE") != nullptr;
    if (disabled) return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    reportPipelines(store);
    auto& key = store.scratchKey;
    pipelineKey(key, context, state, vertexInput, resources, shaders, attachmentLayout);
    if (key.empty()) {
        ++store.uncached;
        return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
    }
    const auto hash = hashKey(key);
    if (const auto found = store.index.find(hash); found != store.index.end()) {
        const auto it = found->second;
        if (it->key == key) {
            // The key names the device handle, which the loader may reuse for a device created after
            // this one was destroyed without ClearCachedPipelines: such an entry is a miss.
            if (alive(*it, context)) {
                ++store.hits;
                store.entries.splice(store.entries.end(), store.entries, it);
                return it->pipeline;
            }
            abandon(store, it);
        } else {
            // A different configuration with the same hash keeps the resident entry; this one stays private.
            ++store.uncached;
            return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
        }
    }
    ++store.misses;
    // Entries of another device belong to one the driver replaced (it does so under the GpuMutex
    // before any draw reaches the new device), whose objects went with it: forget them.
    for (auto it = store.entries.begin(); it != store.entries.end();) {
        it = alive(*it, context) ? std::next(it) : abandon(store, it);
    }
    auto pipeline = std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders, attachmentLayout);
    store.entries.push_back({context.device, context.bufferPool, hash, key, pipeline});
    store.index[hash] = std::prev(store.entries.end());
    constexpr std::size_t bound = 1024;
    while (store.entries.size() > bound) {
        // Only an entry no recorded draw still holds may go (Kept keeps its shared_ptr until the fence).
        const auto victim = std::find_if(store.entries.begin(), store.entries.end(), [](const PipelineStore::Entry& entry) { return entry.pipeline.use_count() == 1; });
        if (victim == store.entries.end()) break;
        store.index.erase(victim->hash);
        store.entries.erase(victim);
        ++store.evicted;
    }
    return pipeline;
}

void ClearCachedPipelines(VkDevice device) {
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    for (auto it = store.entries.begin(); it != store.entries.end();) {
        if (it->device != device) {
            ++it;
            continue;
        }
        store.index.erase(it->hash);
        it = store.entries.erase(it);
    }
    ClearPipelineLibraries(device);
}

}
