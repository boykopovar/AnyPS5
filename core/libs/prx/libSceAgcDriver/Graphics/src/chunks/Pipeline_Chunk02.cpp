#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PatchVertexLayer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <mutex>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
Pipeline::Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) : context(context), _modules(shaders.size()), attachments(state.colors.size() + (state.depth ? 1u : 0u)), colorAttachments(state.colors.size()), depthBounds(state.depth.has_value() && state.depthBoundsTest), depthBias(state.depth.has_value() && state.depthBias), dynamicColorWriteMask(context.dynamicColorWriteMask && !FragmentInterlock(shaders)), dynamicRendering(context.graphicsPipelineLibrary) {
    PerformanceTimer timing("Vulkan.GraphicsPipeline");
    for (const auto& color : state.colors) boundAttachments.push_back(color.partialWriteMask == 0u);
    if (state.depth) boundAttachments.push_back(true);
    sampleLocationsEnabled = ValidateSampleLocations(context, state);
    const auto host = HostSampleLocationsFor(context, state);
    sampleLocationGrid = host.grid;
    sampleOrder = host.order;
    // A cached pipeline may outlive its device's teardown (see ClearCachedPipelines); it must not keep
    // the buffer pool, which is reset with the device, alive past it.
    this->context.bufferPool.reset();
    Require(state.blends.size() == (state.colors.empty() ? 0u : state.colors.back().exportIndex + 1u) && state.colors.size() <= state.blends.size(), "blend states do not match decoded color state");
    Require(state.blends.size() <= context.limits.maxColorAttachments, "color targets exceed device attachment limits");
    const bool boundColor = std::ranges::any_of(state.colors, [](const ColorTarget& color) { return color.partialWriteMask == 0u; });
    Require((state.hasColorTarget && (boundColor || state.colors.empty() || state.depth.has_value())) || (context.limits.framebufferNoAttachmentsSampleCounts & static_cast<VkSampleCountFlagBits>(state.samples)) != 0, "device does not support rendering without attachments at the draw's sample count");
    Require(state.samples == 1 || !state.depth.has_value() || (context.limits.framebufferDepthSampleCounts & static_cast<VkSampleCountFlagBits>(state.samples)) != 0, "device does not support depth targets with the draw's sample count");
    Require(!depthBounds || context.depthBounds, "device does not support the depth bounds test");
    Require(!depthBias || state.depthBiasClamp == 0.0f || context.depthBiasClamp, "device does not support depth bias clamping");
    Require(!state.dualSourceBlend || (context.dualSrcBlend && state.blends.size() <= context.limits.maxFragmentDualSrcAttachments), "device does not support dual-source blending into this many attachments");
    Require(state.reversedBlend == 0 || (context.dualSrcBlend && state.blends.size() <= context.limits.maxFragmentDualSrcAttachments), "blending into a color target with a reversed component order through a second source color needs dual-source blending, which the device does not support");
    Require(LogicOpFromRop(state.colorRop) == VK_LOGIC_OP_COPY || context.logicOp, "device does not support color logic operations");
    Require(LogicOpFromRop(state.colorRop) == VK_LOGIC_OP_COPY || std::ranges::all_of(state.colors, [](const ColorTarget& color) { return LogicOpFormat(color.format); }), "color logic operations on a color target that is not a normalized or integer format are not implemented");
    Require(!state.negativeOneToOne || context.depthClipControl, "negative-one-to-one depth clipping requires VK_EXT_depth_clip_control with depthClipControl enabled");
    Require(!state.perSampleShading || context.sampleRateShading, "per-sample pixel shading requires the sampleRateShading device feature");
    Require(!state.mergedCoverage || (context.mergedCoverageSampleCounts & state.samples) != 0, "an overrasterized draw (a multisampled rasterizer into one-sample color targets) requires VK_NV_framebuffer_mixed_samples with merged coverage reduction");
    Require(!(state.vertexPoints || (state.stages.mesh.has_value() && state.stages.mesh->outputPrimitiveSize == 1u)) || context.largePoints || (state.pointSize == 1.0f && ((state.paClVsOutCntl & (1u << 16u)) == 0 || (state.pointSizeMin == 1.0f && state.pointSizeMax == 1.0f))), "points of a size other than one pixel require the largePoints device feature");
    Require(!state.truncatedCoverage || (context.truncatedCoverageSampleCounts & state.samples) != 0, "one-sample color targets beside a multisampled depth target require VK_AMD_mixed_attachment_samples or VK_NV_framebuffer_mixed_samples with truncated coverage reduction");
    Require(state.conservativeRasterization == VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT || context.conservativeRasterization, "conservative rasterization requires VK_EXT_conservative_rasterization with at most 1/256 pixel of overestimation and degenerate triangles rasterized");
    ValidateProvokingVertex(context, state, shaders);
    if (state.rectList) Require(context.tessellationShader && context.limits.maxTessellationPatchSize >= 4, "rect-list requires tessellation with four output control points");
    const bool barycentricPatches = BarycentricPatches(state, shaders);
    if (barycentricPatches) Require(context.tessellationShader && ((state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && (!state.primitiveRestart || !context.primitiveListRestart)) || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP), "emulated barycentrics through tessellation require a triangle list or strip, with primitive restart in a list only on a device without VK_EXT_primitive_topology_list_restart");
    if (std::any_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.stage == ShaderRecompiler::ShaderStage::Geometry; })) Require(context.geometryShader, "device does not support geometry shaders");
    if (state.stages.tessellation) {
        Require(context.tessellationShader, "device does not support tessellation shaders");
        Require(state.stages.tessellation->inputControlPoints <= context.limits.maxTessellationPatchSize && state.stages.tessellation->outputControlPoints <= context.limits.maxTessellationPatchSize, "tessellation patch exceeds device limits");
    }
    if (state.stages.mesh) {
        Require(context.meshShader, "device does not support VK_EXT_mesh_shader");
        const auto& mesh = *state.stages.mesh;
        const auto invocations = state.stages.vertexWaveSize == 64u && context.subgroup.subgroupSize == 32u ? mesh.threadsPerGroup / 2u : mesh.threadsPerGroup;
        ValidateMeshLimits(context, mesh, invocations);
    }
    const auto pushStages = PushConstantStages(shaders);
    Require(pushStages == 0 || context.limits.maxPushConstantsSize >= PushBlockBytes(dynamicRendering), "graphics push constant range exceeds device limit");
    if (dynamicRendering) {
        for (const auto& shader : shaders) Require(shader.program->pushConstants.empty() || (shader.pushConstantOffset >= PipelinePushSlotBytes) == (shader.stage == ShaderRecompiler::ShaderStage::Fragment), "a stage's push constants lie outside its push constant slot");
    }
    try {
        std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
        std::vector<PipelineSpecialization> specializations;
        specializations.reserve(shaders.size());
        const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT meshSubgroup{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT, nullptr, 32u};
        for (std::uint32_t i = 0; i < shaders.size(); ++i) {
            const auto& shader = *shaders[i].program;
            const auto demoted = (barycentricPatches || state.rectList) && shaders[i].stage == ShaderRecompiler::ShaderStage::Vertex ? DemoteVertexLayerOutput(shader.spirv.Words()) : std::vector<std::uint32_t>{};
            const auto words = demoted.empty() ? shader.spirv.Words() : std::span<const std::uint32_t>(demoted);
            _modules[i] = CreateShaderModule(context, words.data(), words.size() * sizeof(std::uint32_t), "vkCreateShaderModule graphics");
            const auto stage = VulkanStage(shaders[i].stage);
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = stage;
            stages[i].module = _modules[i];
            stages[i].pName = "main";
            if (shaders[i].stage == ShaderRecompiler::ShaderStage::Mesh && context.meshWave32 && shader.hostSubgroupSize == 32u) stages[i].pNext = &meshSubgroup;
            specializations.emplace_back(shader, context.atomicLockTable, state.meshLayerBase, state.meshLayerCount);
            stages[i].pSpecializationInfo = specializations.back().Info();
        }
        if (std::ranges::any_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && !shader.program->stencilReferences.empty(); })) {
            for (std::size_t i = 0; i < shaders.size(); ++i) {
                if (shaders[i].stage != ShaderRecompiler::ShaderStage::Vertex) continue;
                Require(stages[i].flags == 0u && stages[i].pNext == nullptr, "stencil replay vertex stage allows varying subgroup size");
            }
        }
        // A descriptor set layout with the same bindings as this one is compatible with the pipeline
        // layout, so later draws bind their own ShaderResources' set under it.
        const bool pixelLds = std::ranges::any_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->functionLdsBuffer; });
        const bool meshWindow = UsesMeshWindow(shaders);
        const auto pixelLdsLayouts = pixelLds || meshWindow ? PixelFunctionLdsSetLayouts(context) : PixelFunctionLdsLayouts{};
        const std::array<VkDescriptorSetLayout, 4> setLayouts{resources.main, (pixelLds || meshWindow) && !resources.split ? pixelLdsLayouts.empty : resources.fragment, pixelLds ? pixelLdsLayouts.buffer : pixelLdsLayouts.empty, meshWindow ? MeshWindowSetLayout(context) : VK_NULL_HANDLE};
        const VkPushConstantRange push{pushStages, 0, PipelinePushSlotBytes};
        const std::array<VkPushConstantRange, 2> slots{{{pushStagesOf(context), 0, PipelinePushSlotBytes}, {VK_SHADER_STAGE_FRAGMENT_BIT, PipelinePushSlotBytes, PipelinePushSlotBytes}}};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.flags = pixelLds || meshWindow || !resources.split ? 0 : VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
        layoutInfo.setLayoutCount = meshWindow ? 4u : pixelLds ? 3u : resources.split ? 2u : 1u;
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.pushConstantRangeCount = dynamicRendering ? static_cast<std::uint32_t>(slots.size()) : pushStages != 0 ? 1 : 0;
        layoutInfo.pPushConstantRanges = dynamicRendering ? slots.data() : pushStages != 0 ? &push : nullptr;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout graphics");
        std::vector<VkAttachmentDescription> colors;
        std::vector<VkAttachmentReference> references(state.blends.size(), VkAttachmentReference{VK_ATTACHMENT_UNUSED, attachmentLayout});
        for (std::uint32_t index = 0; index < state.colors.size(); ++index) {
            if (state.colors[index].partialWriteMask != 0u) continue;
            VkAttachmentDescription color{};
            color.format = state.colors[index].format;
            color.samples = state.mergedCoverage || state.truncatedCoverage ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(state.samples);
            color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = attachmentLayout;
            color.finalLayout = attachmentLayout;
            references.at(state.colors[index].exportIndex) = {static_cast<std::uint32_t>(colors.size()), attachmentLayout};
            colors.push_back(color);
        }
        boundColorAttachments = static_cast<std::uint32_t>(colors.size());
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<std::uint32_t>(references.size());
        subpass.pColorAttachments = references.empty() ? nullptr : references.data();
        const VkAttachmentReference depthReference{static_cast<std::uint32_t>(colors.size()), VK_IMAGE_LAYOUT_GENERAL};
        if (state.depth) {
            VkAttachmentDescription depth{};
            depth.format = HostDepthFormat(context, state.depth->format);
            depth.samples = static_cast<VkSampleCountFlagBits>(state.samples);
            depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
            depth.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
            colors.push_back(depth);
            subpass.pDepthStencilAttachment = &depthReference;
        }
        std::vector<VkFormat> renderingColors(state.blends.size(), VK_FORMAT_UNDEFINED);
        for (const auto& color : state.colors) {
            if (color.partialWriteMask == 0u) renderingColors.at(color.exportIndex) = color.format;
        }
        VkPipelineRenderingCreateInfoKHR rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
        rendering.colorAttachmentCount = static_cast<std::uint32_t>(renderingColors.size());
        rendering.pColorAttachmentFormats = renderingColors.empty() ? nullptr : renderingColors.data();
        const std::vector<VkSampleCountFlagBits> mergedColorSamples(renderingColors.size(), VK_SAMPLE_COUNT_1_BIT);
        VkAttachmentSampleCountInfoNV attachmentSamples{VK_STRUCTURE_TYPE_ATTACHMENT_SAMPLE_COUNT_INFO_NV};
        attachmentSamples.colorAttachmentCount = static_cast<std::uint32_t>(mergedColorSamples.size());
        attachmentSamples.pColorAttachmentSamples = mergedColorSamples.empty() ? nullptr : mergedColorSamples.data();
        if (state.truncatedCoverage) attachmentSamples.depthStencilAttachmentSamples = static_cast<VkSampleCountFlagBits>(state.samples);
        if (state.mergedCoverage || state.truncatedCoverage) rendering.pNext = &attachmentSamples;
        if (state.depth) {
            rendering.depthAttachmentFormat = HostDepthFormat(context, state.depth->format);
            if (HasStencil(state.depth->format)) rendering.stencilAttachmentFormat = rendering.depthAttachmentFormat;
        }
        if (dynamicRendering) {
            depthFormat = state.depth ? HostDepthFormat(context, state.depth->format) : VK_FORMAT_UNDEFINED;
            colorSlots.assign(state.blends.size(), ~0u);
            for (std::uint32_t index = 0; index < state.colors.size(); ++index) {
                if (state.colors[index].partialWriteMask == 0u) colorSlots.at(state.colors[index].exportIndex) = index;
            }
            this->attachmentLayout = attachmentLayout;
        }
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = static_cast<std::uint32_t>(colors.size());
        passInfo.pAttachments = colors.empty() ? nullptr : colors.data();
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        if (!dynamicRendering) Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &renderPass), "vkCreateRenderPass");
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexInput.bindings.size());
        input.pVertexBindingDescriptions = vertexInput.bindings.data();
        input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexInput.attributes.size());
        input.pVertexAttributeDescriptions = vertexInput.attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        const bool patches = BarycentricPatches(state, shaders);
        assembly.topology = patches ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : state.topology;
        const bool listTopology = state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        assembly.primitiveRestartEnable = !patches && state.primitiveRestart && (!listTopology || context.primitiveListRestart) ? VK_TRUE : VK_FALSE;
        // Viewport and scissor are set per draw (Begin), so they do not multiply pipelines; the depth
        // clip control stays baked in.
        VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
        depthClip.negativeOneToOne = state.negativeOneToOne;
        if (state.negativeOneToOne) viewports.pNext = &depthClip;
        Require(!patches || state.extraViewports.empty(), "a viewport index through the barycentric patch stages is unsupported");
        viewports.viewportCount = ViewportCount(state);
        viewports.scissorCount = ViewportCount(state);
        PipelineLibraryKeys keys;
        narrowLibraryKeys = narrowLibraryKeysNow();
        libraries = libraryKeys(keys, context, state, vertexInput, resources, shaders, narrowLibraryKeys);
        std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        if (state.stencilReplayReference) dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        if (state.stencilReplayReference && state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && state.cullMode != VK_CULL_MODE_NONE && context.graphicsPipelineLibrary) dynamicStates.push_back(VK_DYNAMIC_STATE_FRONT_FACE_EXT);
        if (depthBounds) dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        if (depthBias) dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
        if (libraries) dynamicStates.assign(PipelineLibraryDynamicStates().begin(), PipelineLibraryDynamicStates().end());
        dynamicDepthBiasEnable = libraries && narrowLibraryKeys && context.dynamicDepthBiasEnable;
        if (dynamicDepthBiasEnable) dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
        if (dynamicColorWriteMask) dynamicStates.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
        dynamicSlots = CommandState::DynamicSlots(dynamicStates);
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        Require(state.polygonMode == VK_POLYGON_MODE_FILL || context.fillModeNonSolid, "a point or line polygon mode needs the fillModeNonSolid feature");
        Require(state.polygonMode != VK_POLYGON_MODE_POINT || context.maintenance5, "a point polygon mode needs VK_KHR_maintenance5 for its one-pixel point size");
        ValidateLineWidth(context, state.lineWidth);
        raster.polygonMode = state.polygonMode;
        raster.rasterizerDiscardEnable = state.rasterizerDiscard ? VK_TRUE : VK_FALSE;
        raster.depthClampEnable = state.depthClamp && context.depthClamp ? VK_TRUE : VK_FALSE;
        Require(!state.depthBiasPerFace || (depthBias && state.cullMode == VK_CULL_MODE_NONE), "per-face depth bias needs both faces rasterized");
        raster.cullMode = state.depthBiasPerFace ? VK_CULL_MODE_BACK_BIT : state.cullMode;
        raster.frontFace = state.frontFace;
        raster.depthBiasEnable = depthBias;
        raster.lineWidth = state.lineWidth;
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
        samples.rasterizationSamples = static_cast<VkSampleCountFlagBits>(state.samples);
        samples.sampleShadingEnable = state.perSampleShading ? VK_TRUE : VK_FALSE;
        samples.minSampleShading = state.perSampleShading ? 1.0f : 0.0f;
        const auto locations = SampleLocations(state);
        VkPipelineSampleLocationsStateCreateInfoEXT sampleLocations{VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT};
        sampleLocations.sampleLocationsEnable = VK_TRUE;
        const auto grid = sampleLocationGrid;
        sampleLocations.sampleLocationsInfo = {VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, samples.rasterizationSamples, grid, state.samples * grid.width * grid.height, locations.data()};
        if (sampleLocationsEnabled) samples.pNext = &sampleLocations;
        VkPipelineCoverageReductionStateCreateInfoNV coverageReduction{VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV};
        coverageReduction.coverageReductionMode = state.truncatedCoverage ? VK_COVERAGE_REDUCTION_MODE_TRUNCATE_NV : VK_COVERAGE_REDUCTION_MODE_MERGE_NV;
        if (state.mergedCoverage || (state.truncatedCoverage && !context.mixedAttachmentSamples)) {
            coverageReduction.pNext = samples.pNext;
            samples.pNext = &coverageReduction;
        }
        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = state.depthTest;
        depthStencil.depthWriteEnable = state.depthWrite;
        depthStencil.depthCompareOp = state.depthCompare;
        depthStencil.depthBoundsTestEnable = depthBounds;
        depthStencil.stencilTestEnable = state.stencilTest;
        depthStencil.front = state.stencilFront;
        depthStencil.back = state.stencilBack;
        const VkSampleMask sampleMask = HostSampleMask(state);
        if (PartialSampleMask(state) && UniformSampleMask(state) && !SampleMaskFollowsOrder(state, sampleOrder)) samples.pSampleMask = &sampleMask;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        const bool alternateAlpha = SplitsAlternateAlpha(state);
        std::vector<VkPipelineColorBlendAttachmentState> firstChannels(state.blends.begin(), state.blends.end());
        std::vector<VkPipelineColorBlendAttachmentState> secondChannels(state.blends.begin(), state.blends.end());
        for (const auto& merged : state.colors) {
            if (merged.partialWriteMask != UnclampedBlendMask) continue;
            for (auto* channels : {&firstChannels, &secondChannels}) channels->at(merged.exportIndex) = {};
        }
        if (alternateAlpha) {
            const auto keepsStencil = [](const VkStencilOpState& face) { return face.writeMask == 0 || (face.failOp == VK_STENCIL_OP_KEEP && face.passOp == VK_STENCIL_OP_KEEP && face.depthFailOp == VK_STENCIL_OP_KEEP); };
            Require(!(state.depth && (state.depthWrite || (state.stencilTest && (!keepsStencil(state.stencilFront) || !keepsStencil(state.stencilBack))))) && !state.depthBiasPerFace, "blending into a two-channel color target with the alternate component order and a separate alpha blend from a draw that writes depth or stencil is not implemented");
            Require(!ShadersWriteMemory(shaders), "blending into a two-channel color target with the alternate component order and a separate alpha blend from a draw whose shaders write memory is not implemented");
            for (std::size_t index = 0; index < secondChannels.size(); ++index) {
                const bool split = index < state.colors.size() && state.colors[index].componentMapping == 0xecu && state.blends[index].blendEnable;
                if (!split) continue;
                secondChannels[index].srcColorBlendFactor = state.blends[index].srcAlphaBlendFactor;
                secondChannels[index].dstColorBlendFactor = state.blends[index].dstAlphaBlendFactor;
                secondChannels[index].colorBlendOp = state.blends[index].alphaBlendOp;
            }
        }
        const auto firstMasks = channelWriteMasks(state, state.blends, false);
        const auto secondMasks = channelWriteMasks(state, state.blends, true);
        for (std::size_t index = 0; index < firstChannels.size(); ++index) {
            firstChannels[index].colorWriteMask = dynamicColorWriteMask ? 0u : firstMasks[index];
            secondChannels[index].colorWriteMask = dynamicColorWriteMask ? 0u : secondMasks[index];
        }
        if (const auto logicOp = LogicOpFromRop(state.colorRop); logicOp != VK_LOGIC_OP_COPY) {
            blend.logicOpEnable = VK_TRUE;
            blend.logicOp = logicOp;
        }
        blend.attachmentCount = static_cast<std::uint32_t>(firstChannels.size());
        blend.pAttachments = firstChannels.empty() ? nullptr : firstChannels.data();
        std::copy(state.blendConstants.begin(), state.blendConstants.end(), blend.blendConstants);
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.flags = context.pipelineExecutableInfo ? VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR : 0;
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        VkPipelineTessellationStateCreateInfo tessellation{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};
        if (state.rectList || state.stages.tessellation || barycentricPatches) {
            tessellation.patchControlPoints = state.stages.tessellation ? state.stages.tessellation->inputControlPoints : 3u;
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
        if (dynamicRendering) pipelineInfo.pNext = &rendering;
        timing.Mark("modules_and_state");
        if (prebuilding && !libraries) {
            for (const auto module : _modules) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            _modules.clear();
            return;
        }
        if (prebuilding) {
            static_cast<void>(LinkPipelineFromLibraries(context, pipelineInfo, rendering, layoutInfo, keys, &optimized, true, _modules));
            for (const auto module : _modules) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
            _modules.clear();
            prebuilt = true;
            return;
        }
        const VkPipelineCache pipelineCache = (state.stages.tessellation || barycentricPatches || state.rectList) && !context.tessellationPipelineCache ? VK_NULL_HANDLE : context.pipelineCache;
        if (libraries) pipeline = LinkPipelineFromLibraries(context, pipelineInfo, rendering, layoutInfo, keys, &optimized, false, _modules);
        else {
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines");
        }
        if (alternateAlpha) {
            blend.pAttachments = secondChannels.data();
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, pipelineCache, 1, &pipelineInfo, nullptr, &secondChannel), "vkCreateGraphicsPipelines");
        }
        if (state.depthBiasPerFace) {
            raster.cullMode = VK_CULL_MODE_FRONT_BIT;
            auto pipelineCacheLock = context.LockPipelineCacheForUse();
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, pipelineCache, 1, &pipelineInfo, nullptr, &backFaces), "vkCreateGraphicsPipelines");
        }
        for (const auto module : _modules) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        _modules.clear();
        timing.Mark("create");
        PerformanceCounters::CountPipeline();
        LogPipelineStatistics_nid_no_patch(context, pipeline, shaders);
    } catch (...) {
        release();
        throw;
    }
}

bool Pipeline::Prebuild(const Context& context, const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders) {
    if (std::ranges::any_of(shaders, [](const CompiledShader& shader) { return !shader.program->stencilReferences.empty(); })) return false;
    struct Scope {
        Scope() { prebuilding = true; prebuilt = false; }
        ~Scope() { prebuilding = false; }
    } scope;
    const Pipeline pipeline(context, state, vertexInput, resources, shaders);
    return prebuilt;
}

Pipeline::~Pipeline() {
    release();
}

void Pipeline::release() noexcept {
    framebuffers.clear();
    if (optimized != nullptr) optimized->Release();
    optimized.reset();
    if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    if (backFaces) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, backFaces, nullptr);
    if (secondChannel) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, secondChannel, nullptr);
    if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
    for (auto module : _modules) {
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    }
    pipeline = VK_NULL_HANDLE;
    backFaces = VK_NULL_HANDLE;
    secondChannel = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    _modules.clear();
}

void Pipeline::Abandon() noexcept {
    if (optimized != nullptr) optimized->released.store(true);
    optimized.reset();
    for (auto& entry : framebuffers) entry.framebuffer->Abandon();
    framebuffers.clear();
    pipeline = VK_NULL_HANDLE;
    backFaces = VK_NULL_HANDLE;
    secondChannel = VK_NULL_HANDLE;
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

std::shared_ptr<Framebuffer> Pipeline::AcquireFramebuffer(std::span<const VkImageView> targets, std::span<const std::shared_ptr<void>> owners, VkExtent2D extent, std::uint32_t layers) {
    Require(targets.size() == attachments && owners.size() == colorAttachments, "render targets do not match the pipeline's attachments");
    bool resident = true;
    for (const auto& owner : owners) resident = resident && owner != nullptr;
    const auto depthReleases = attachments > colorAttachments ? DepthSurfacesReleased() : 0u;
    if (resident) {
        // Entries whose views are gone can never match again and go as soon as no recorded draw holds
        // them (Kept keeps its framebuffer until the batch completes).
        std::erase_if(framebuffers, [&](const CachedFramebuffer& entry) {
            if (entry.framebuffer.use_count() != 1) return false;
            if (entry.depthReleases != depthReleases) return true;
            for (std::size_t i = 0; i < entry.owners.size(); ++i) {
                if (entry.owned[i] && entry.owners[i].expired()) return true;
            }
            return false;
        });
        for (auto it = framebuffers.begin(); it != framebuffers.end(); ++it) {
            if (it->extent.width != extent.width || it->extent.height != extent.height || it->layers != layers || it->depthReleases != depthReleases || !std::equal(it->views.begin(), it->views.end(), targets.begin(), targets.end())) continue;
            // View handles are recycled once a StorageTexture is destroyed, so the owners must be the
            // very objects the views were made for.
            bool same = true;
            for (std::size_t i = 0; i < owners.size() && same; ++i) same = it->owned[i] == (owners[i] != nullptr) && it->owners[i].lock().get() == owners[i].get();
            if (!same) continue;
            std::rotate(it, std::next(it), framebuffers.end());
            return framebuffers.back().framebuffer;
        }
    }
    std::vector<VkImageView> boundViews;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (boundAttachments[i]) boundViews.push_back(targets[i]);
    }
    auto framebuffer = std::make_shared<Framebuffer>(context, renderPass, renderPass == VK_NULL_HANDLE ? targets : std::span<const VkImageView>(boundViews), extent, layers);
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
    for (const auto& owner : owners) entry.owned.push_back(owner != nullptr);
    entry.extent = extent;
    entry.layers = layers;
    entry.depthReleases = depthReleases;
    entry.framebuffer = framebuffer;
    framebuffers.push_back(std::move(entry));
    return framebuffer;
}

}
}
}
}
