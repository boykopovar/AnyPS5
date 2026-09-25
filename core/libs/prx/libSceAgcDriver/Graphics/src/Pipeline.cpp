#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

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

void ValidateViewport(const Context& context, const VkViewport& viewport) {
    Require(std::isfinite(viewport.minDepth) && std::isfinite(viewport.maxDepth), "non-finite viewport depth range");
    Require(context.depthRangeUnrestricted || (viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1), "viewport depth outside [0, 1] requires VK_EXT_depth_range_unrestricted");
    Require(std::isfinite(viewport.x) && std::isfinite(viewport.y) && std::isfinite(viewport.width) && std::isfinite(viewport.height), "viewport arithmetic overflow");
    Require(viewport.width <= context.limits.maxViewportDimensions[0] && std::abs(viewport.height) <= context.limits.maxViewportDimensions[1], "viewport dimensions exceed device limits");
    Require(viewport.x >= context.limits.viewportBoundsRange[0] && viewport.x + viewport.width <= context.limits.viewportBoundsRange[1], "viewport X exceeds device bounds");
    Require(std::min(viewport.y, viewport.y + viewport.height) >= context.limits.viewportBoundsRange[0] && std::max(viewport.y, viewport.y + viewport.height) <= context.limits.viewportBoundsRange[1], "viewport Y exceeds device bounds");
}

Pipeline::Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders) : context(context), _modules(shaders.size()), attachments(state.colors.size()) {
    // A cached pipeline may outlive its device's teardown (see ClearCachedPipelines); it must not keep
    // the buffer pool, which is reset with the device, alive past it.
    this->context.bufferPool.reset();
    Require(state.blends.size() == state.colors.size(), "blend states do not match decoded color state");
    Require(state.colors.size() <= context.limits.maxColorAttachments, "color targets exceed device attachment limits");
    Require(state.hasColorTarget || (context.limits.framebufferNoAttachmentsSampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0, "device does not support single-sample rendering without attachments");
    Require(!state.negativeOneToOne || context.depthClipControl, "negative-one-to-one depth clipping requires VK_EXT_depth_clip_control with depthClipControl enabled");
    if (state.rectList) Require(context.tessellationShader && context.limits.maxTessellationPatchSize >= 4, "rect-list requires tessellation with four output control points");
    if (state.stages.tessellation) {
        Require(context.tessellationShader, "device does not support tessellation shaders");
        Require(state.stages.tessellation->inputControlPoints <= context.limits.maxTessellationPatchSize && state.stages.tessellation->outputControlPoints <= context.limits.maxTessellationPatchSize, "tessellation patch exceeds device limits");
    }
    if (state.stages.mesh) {
        Require(context.meshShader, "device does not support VK_EXT_mesh_shader");
        const auto& mesh = *state.stages.mesh;
        Require(mesh.threadsPerGroup <= context.meshLimits.maxMeshWorkGroupInvocations && mesh.threadsPerGroup <= context.meshLimits.maxMeshWorkGroupSize[0], "mesh workgroup exceeds device limits");
        Require(mesh.maxVertices <= context.meshLimits.maxMeshOutputVertices && mesh.maxPrimitives <= context.meshLimits.maxMeshOutputPrimitives && static_cast<std::uint64_t>(mesh.ldsSizeDwords) * 4 <= context.meshLimits.maxMeshSharedMemorySize, "mesh output or LDS exceeds device limits");
    }
    const auto pushStages = PushConstantStages(shaders);
    Require(pushStages == 0 || context.limits.maxPushConstantsSize >= PipelinePushConstantBytes, "graphics push constant range exceeds device limit");
    try {
        std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
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
        std::vector<VkAttachmentReference> references;
        for (std::uint32_t index = 0; index < state.colors.size(); ++index) {
            VkAttachmentDescription color{};
            color.format = state.colors[index].format;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colors.push_back(color);
            references.push_back({index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        }
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<std::uint32_t>(references.size());
        subpass.pColorAttachments = references.empty() ? nullptr : references.data();
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
        // Viewport and scissor are set per draw (Begin), so they do not multiply pipelines; the depth
        // clip control stays baked in.
        VkPipelineViewportStateCreateInfo viewports{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        VkPipelineViewportDepthClipControlCreateInfoEXT depthClip{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
        depthClip.negativeOneToOne = state.negativeOneToOne;
        if (state.negativeOneToOne) viewports.pNext = &depthClip;
        viewports.viewportCount = 1;
        viewports.scissorCount = 1;
        const std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.depthClampEnable = state.depthClamp && context.depthClamp ? VK_TRUE : VK_FALSE;
        raster.cullMode = state.cullMode;
        raster.frontFace = state.frontFace;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = static_cast<std::uint32_t>(state.blends.size());
        blend.pAttachments = state.blends.empty() ? nullptr : state.blends.data();
        std::copy(state.blendConstants.begin(), state.blendConstants.end(), blend.blendConstants);
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
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
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = renderPass;
        Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateGraphicsPipelines");
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
    if (renderPass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, renderPass, nullptr);
    if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
    for (auto module : _modules) {
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    }
    pipeline = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
    _modules.clear();
}

void Pipeline::Abandon() noexcept {
    for (auto& entry : framebuffers) entry.framebuffer->Abandon();
    framebuffers.clear();
    pipeline = VK_NULL_HANDLE;
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
    Require(targets.size() == attachments && targets.size() == owners.size(), "render targets do not match the pipeline's color attachments");
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

void Pipeline::Begin(VkCommandBuffer commands, const Framebuffer& framebuffer, VkExtent2D extent, const VkViewport& viewport, const VkRect2D& scissor) const {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = renderPass;
    begin.framebuffer = framebuffer.Handle();
    begin.renderArea = {{0, 0}, extent};
    context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
    context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    context.Function<PFN_vkCmdSetViewport>("vkCmdSetViewport")(commands, 0, 1, &viewport);
    context.Function<PFN_vkCmdSetScissor>("vkCmdSetScissor")(commands, 0, 1, &scissor);
}

void Pipeline::PushConstants(VkCommandBuffer commands, std::span<const CompiledShader> shaders) const {
    const auto stages = PushConstantStages(shaders);
    if (stages == 0) return;
    const auto bytes = AssemblePushConstants(shaders);
    context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, layout, stages, 0, PipelinePushConstantBytes, bytes.data());
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
std::vector<std::byte> pipelineKey(const Context& context, const State& state, const VertexInputLayout& input, const ShaderResources& resources, std::span<const CompiledShader> shaders) {
    using Stage = ShaderRecompiler::ShaderStage;
    std::vector<std::byte> key;
    append(key, context.device);
    append(key, shaders.size());
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        const bool generated = state.rectList && (shader.stage == Stage::TessellationControl || shader.stage == Stage::TessellationEvaluation);
        if (!generated && shader.program->variantId == 0) return {};
        append(key, shader.stage);
        append(key, generated ? std::uint64_t{0} : shader.program->variantId);
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
    append(key, state.cullMode);
    append(key, state.frontFace);
    append(key, state.negativeOneToOne);
    append(key, state.depthClamp && context.depthClamp);
    append(key, state.blends.size());
    for (const auto& blend : state.blends) append(key, blend);
    for (const auto value : state.blendConstants) append(key, value);
    append(key, state.colors.size());
    for (const auto& color : state.colors) append(key, color.format);
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
    return key;
}

std::uint64_t hashKey(const std::vector<std::byte>& key) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : key) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
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
    std::fprintf(stderr, "[pipecache] %llu lookups over 10 s: %llu hits (%.0f%%), %llu misses, %llu private (no variant id), %llu evicted, %zu cached\n", static_cast<unsigned long long>(lookups), static_cast<unsigned long long>(store.hits), lookups != 0 ? 100.0 * static_cast<double>(store.hits) / static_cast<double>(lookups) : 0.0, static_cast<unsigned long long>(store.misses), static_cast<unsigned long long>(store.uncached), static_cast<unsigned long long>(store.evicted), store.entries.size());
    store.hits = store.misses = store.uncached = store.evicted = 0;
}

}

std::shared_ptr<Pipeline> CachedPipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders) {
    static const bool disabled = std::getenv("APS5_NO_PIPELINE_CACHE") != nullptr;
    if (disabled) return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders);
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    reportPipelines(store);
    const auto key = pipelineKey(context, state, vertexInput, resources, shaders);
    if (key.empty()) {
        ++store.uncached;
        return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders);
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
            return std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders);
        }
    }
    ++store.misses;
    // Entries of another device belong to one the driver replaced (it does so under the GpuMutex
    // before any draw reaches the new device), whose objects went with it: forget them.
    for (auto it = store.entries.begin(); it != store.entries.end();) {
        it = alive(*it, context) ? std::next(it) : abandon(store, it);
    }
    auto pipeline = std::make_shared<Pipeline>(context, state, vertexInput, resources, shaders);
    store.entries.push_back({context.device, context.bufferPool, hash, key, pipeline});
    store.index[hash] = std::prev(store.entries.end());
    constexpr std::size_t bound = 256;
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
}

}
