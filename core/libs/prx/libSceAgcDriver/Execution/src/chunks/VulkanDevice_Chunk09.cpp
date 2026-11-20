#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameDumpIo.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/CompileBudget.hpp"
#include "BdaAbi.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float16ConversionProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float16NanProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/FloatFlushProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostReportSources.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostReportVulkan.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaAtomics.hpp"
#include "prx/libSceAgcDriver/Execution/include/RasterOrderGroup.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libSceAgcDriver/Execution/include/FragmentBarycentric.hpp"
#include "prx/libSceAgcDriver/Execution/include/Float32FlushControl.hpp"
#include "prx/libSceAgcDriver/Execution/include/PipelineLinkOptimization.hpp"
#include "prx/libSceAgcDriver/Execution/include/DrawInputsInPlace.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubgroupBarrier.hpp"
#include "prx/libSceAgcDriver/Execution/include/TessellationPipelineCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/IndexedIndirectPatches.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubgroupClock.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderDeviceProfile.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameLimit.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentMode.hpp"
#include "prx/libSceAgcDriver/Execution/include/SwapchainImages.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/KeyboardOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PadStatusOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationPass.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/TraceRevision.hpp"
#include "prx/libSceAgcDriver/Execution/include/UpscaleMode.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/RunReportComponent.hpp"
#include "RdnaDecoder/DiagnosticSink.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
extern char** environ;
#endif
#include <atomic>
#include <chrono>
#include <charconv>
#include <condition_variable>
#include <future>
#include <fstream>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <SDL_loadso.h>
#include <SDL_error.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <optional>
#include <algorithm>
#include <bit>
#include <cstring>
#include <deque>

namespace AgcDriver {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::shared_ptr<ComputePipelineObjects> VulkanDevice::createComputePipeline(const Graphics::Context& context, const ShaderRecompiler::RecompileResult& shader, VkDescriptorSetLayout setLayout, std::span<const std::uint32_t> layoutKey, VkShaderStageFlags pushStages, std::uint64_t programAddress) {
    PerformanceTimer timing("Vulkan.ComputePipeline");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = std::chrono::steady_clock::now();
    auto objects = std::make_shared<ComputePipelineObjects>();
    objects->device = state->device;
    objects->destroyModule = state->DeviceFunction<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
    objects->destroyLayout = state->DeviceFunction<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
    objects->destroyPipeline = state->DeviceFunction<PFN_vkDestroyPipeline>("vkDestroyPipeline");
    objects->module = Graphics::CreateShaderModule(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule"), state->device, state->graphicsPipelineLibrary, shader.spirv.data(), shader.spirv.size() * sizeof(std::uint32_t), "vkCreateShaderModule");
    objects->resourcesLayout = setLayout;
    const bool scratchPool = shader.scratchPoolSlotDwords != 0;
    const std::array<VkDescriptorSetLayout, 3> setLayouts{setLayout, shader.workgroupMemoryDwords != 0 || scratchPool ? state->workgroupMemoryLayout : VK_NULL_HANDLE, scratchPool ? state->workgroupMemoryLayout : VK_NULL_HANDLE};
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::ComputePushConstantBytes};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = scratchPool ? ShaderRecompiler::ScratchPoolDescriptorSet + 1 : shader.workgroupMemoryDwords != 0 ? ShaderRecompiler::WorkgroupMemoryDescriptorSet + 1 : 1;
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
    layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
    check(state->DeviceFunction<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(state->device, &layoutInfo, nullptr, &objects->layout), "vkCreatePipelineLayout");
    const VkPipelineCreateFlags pipelineFlags = computePipelineFlags();
    const Graphics::PipelineSpecialization specialization(shader, context.atomicLockTable);
    const auto subgroupSize = computeSubgroupSize(shader);
    const auto stageFlags = computeStageFlags(shader);
    if (const auto* directory = ComputePipelineDumpDirectory(); directory != nullptr && *directory != 0 && !state->probingFloatControls && ComputePipelineCapture::OptionalEnabled() && CaptureComputePipeline(shader.spirv.size())) {
        ComputePipelineCapture capture;
        capture.spirv = shader.spirv.Copy();
        ComputeDescriptorSetLayout resourcesLayout;
        const bool updateAfterBind = layoutKey.size() % 4u == 1u;
        resourcesLayout.flags = updateAfterBind ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
        for (std::size_t index = 0; index + 4u <= layoutKey.size(); index += 4u) {
            const auto type = static_cast<VkDescriptorType>(layoutKey[index + 1u]);
            resourcesLayout.bindings.push_back({layoutKey[index], type, layoutKey[index + 2u], layoutKey[index + 3u], updateAfterBind && type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0u});
        }
        capture.sets.push_back(std::move(resourcesLayout));
        if (shader.workgroupMemoryDwords != 0 || scratchPool) capture.sets.push_back({0, {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}}});
        if (scratchPool) capture.sets.push_back({0, {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}}});
        if (pushStages != 0) capture.pushConstants.push_back(push);
        for (const auto& value : shader.specialization) capture.specialization.push_back({value.id, value.value});
        capture.requiredSubgroupSize = subgroupSize;
        capture.stageFlags = stageFlags;
        capture.flags = pipelineFlags;
        capture.deviceName = state->properties.deviceName;
        capture.vendorId = state->properties.vendorID;
        capture.deviceId = state->properties.deviceID;
        capture.driverVersion = state->properties.driverVersion;
        capture.apiVersion = state->properties.apiVersion;
        std::copy_n(state->properties.pipelineCacheUUID, VK_UUID_SIZE, capture.pipelineCacheUuid.begin());
        const auto path = capture.WriteOptional(directory);
        if (!path.empty()) std::fprintf(stderr, "[compute-capture] %s\n", path.string().c_str());
    }
    if (profile && shader.spirv.size() > 100000) std::fprintf(stderr, "[dispatch] creating a pipeline for %zu SPIR-V words (program 0x%llx)\n", shader.spirv.size(), static_cast<unsigned long long>(programAddress));
    CreateComputePipeline(state->device, state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines"), context.pipelineCache, objects->module, objects->layout, pipelineFlags, stageFlags, specialization.Info(), subgroupSize, &objects->pipeline, context.pipelineCacheSynchronization);
    objects->destroyModule(objects->device, objects->module, nullptr);
    objects->module = VK_NULL_HANDLE;
    if (!state->probingFloatControls) {
        computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
        state->computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
        if (GuestMemory::GpuMutex().HeldByThisThread()) computePipelinesCreatedLocked.fetch_add(1, std::memory_order_relaxed);
    }
    Graphics::LogPipelineStatistics_nid_no_patch(context, objects->pipeline);
    if (profile && shader.spirv.size() > 100000) std::fprintf(stderr, "[dispatch] created a pipeline for %zu SPIR-V words in %.0f ms (program 0x%llx)\n", shader.spirv.size(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(), static_cast<unsigned long long>(programAddress));
    return objects;
}

VkPipeline VulkanDevice::splitComputePipeline(const Graphics::Context& context, const ShaderRecompiler::RecompileResult& shader, ComputePipelineObjects& objects, const std::array<std::uint32_t, 3>& base) {
    std::lock_guard lock(objects.splitMutex);
    if (const auto found = objects.splitPipelines.find(base); found != objects.splitPipelines.end()) return found->second;
    std::array<ShaderRecompiler::PipelineSpecializationConstant, ShaderRecompiler::PipelineSpecialization::WorkgroupOffsetWords> offsets{};
    for (std::uint32_t component = 0; component < offsets.size(); ++component) offsets[component] = {ShaderRecompiler::PipelineSpecialization::WorkgroupOffsetBase + component, base[component]};
    const Graphics::PipelineSpecialization specialization(shader, context.atomicLockTable, offsets);
    VkShaderModule module = Graphics::CreateShaderModule(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule"), state->device, state->graphicsPipelineLibrary, shader.spirv.data(), shader.spirv.size() * sizeof(std::uint32_t), "vkCreateShaderModule");
    VkPipeline pipeline = VK_NULL_HANDLE;
    try {
        CreateComputePipeline(state->device, state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines"), context.pipelineCache, module, objects.layout, computePipelineFlags(), computeStageFlags(shader), specialization.Info(), computeSubgroupSize(shader), &pipeline, context.pipelineCacheSynchronization);
    } catch (...) {
        objects.destroyModule(objects.device, module, nullptr);
        throw;
    }
    objects.destroyModule(objects.device, module, nullptr);
    computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
    state->computePipelinesCreated.fetch_add(1, std::memory_order_relaxed);
    if (GuestMemory::GpuMutex().HeldByThisThread()) computePipelinesCreatedLocked.fetch_add(1, std::memory_order_relaxed);
    objects.splitPipelines.emplace(base, pipeline);
    return pipeline;
}

std::shared_ptr<ComputePipelineObjects> VulkanDevice::computePipeline(const Graphics::Context& context, const ShaderRecompiler::RecompileResult& shader, VkDescriptorSetLayout setLayout, std::span<const std::uint32_t> layoutKey, VkShaderStageFlags pushStages, std::uint64_t programAddress) {
    const auto pipelineKey = ComputePipelineKey::Of(shader, pushStages != 0);
    std::promise<std::shared_ptr<ComputePipelineObjects>> building;
    if (!pipelineKey.identity.Empty()) {
        std::shared_future<std::shared_ptr<ComputePipelineObjects>> other;
        {
            std::lock_guard pipelines(state->computePipelinesMutex);
            if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) return found->second;
            if (const auto pending = state->computePipelinesBuilding.find(pipelineKey); pending != state->computePipelinesBuilding.end()) other = pending->second;
            else state->computePipelinesBuilding.emplace(pipelineKey, building.get_future().share());
        }
        if (other.valid()) return other.get();
    }
    const auto finish = [&](std::shared_ptr<ComputePipelineObjects> built, std::exception_ptr error) {
        if (pipelineKey.identity.Empty()) return built;
        {
            std::lock_guard pipelines(state->computePipelinesMutex);
            if (built != nullptr) built = state->computePipelines.try_emplace(pipelineKey, built).first->second;
            state->computePipelinesBuilding.erase(pipelineKey);
        }
        if (error != nullptr) building.set_exception(error);
        else building.set_value(built);
        return built;
    };
    std::shared_ptr<ComputePipelineObjects> objects;
    try {
        objects = createComputePipeline(context, shader, setLayout, layoutKey, pushStages, programAddress);
        if (Graphics::PipelineJournalEnabled() && !state->probingFloatControls) Graphics::RecordComputePipeline(shader, pushStages != 0);
    } catch (...) {
        finish(nullptr, std::current_exception());
        throw;
    }
    return finish(std::move(objects), nullptr);
}

VulkanDevice::IndirectOutcome VulkanDevice::dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared, std::shared_ptr<const Recipe>* recipeOut, const std::array<std::uint32_t, 3>& threadGroup, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear, const std::vector<ShaderRecompiler::RegisteredImageView>* registeredImageViews) {
    startJournalReplay();
    for (const auto& binding : shader.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        for (std::size_t at = 0; at + 4u <= binding.guestDescriptor.size(); at += 4u) {
            const auto* words = binding.guestDescriptor.data() + at;
            const auto base = static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1] & 0xffffu) << 32u);
            const auto stride = (words[1] >> 16u) & 0x3fffu;
            const auto records = static_cast<std::uint64_t>(words[2]);
            const auto bytes = stride != 0 ? records * stride : records;
            if (base != 0 && bytes != 0) static_cast<void>(Graphics::FlushDepthPlanes(base, static_cast<std::size_t>(bytes)));
        }
    }
    state->recorder->BoundKeptBytes();
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    PerformanceTimer timing("Vulkan.Dispatch");
    if (recipeOut != nullptr) *recipeOut = nullptr;
    // Group counts for the trace lines; an indirect dispatch does not know them.
    char groupsText[40];
    if (arguments != 0) std::snprintf(groupsText, sizeof(groupsText), "indirect");
    else std::snprintf(groupsText, sizeof(groupsText), "%ux%ux%u", x, y, z);
    APS5_LOG_OUT_DEBUG("Dispatch groups=%s spirvWords=%zu bindings=%zu pushConstants=%zu", groupsText, shader.spirv.size(), shader.bindings.size(), shader.pushConstants.size());
    if (shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) {
        throw std::runtime_error("Vulkan dispatch: invalid SPIR-V");
    }
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0, registeredImageViews}}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::ComputePushConstantBytes) {
        throw std::runtime_error("Vulkan dispatch: compute push constant range exceeds device limit");
    }
    auto pushBytes = Graphics::AssemblePushConstants(shaders);
    const auto context = graphicsContext();
    const auto pipelineKey = ComputePipelineKey::Of(shader, pushStages != 0);
    std::shared_ptr<ComputePipelineObjects> objects = prepared != nullptr ? prepared->objects : nullptr;
    if (objects == nullptr && !pipelineKey.identity.Empty()) {
        std::lock_guard pipelines(state->computePipelinesMutex);
        if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) objects = found->second;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& d = Dispatches();
    // This call's phases (indexed by DispatchPhase), formatted only for a slow call or a new
    // [indirect] maximum; the totals per phase across dispatches are reported every 1000 dispatches
    // under APS5_PROFILE_DRAW.
    DispatchTimer timer(profile, arguments != 0, shader.spirv.size(), programAddress);
    DispatchTimer::countDispatch(profile);
    const auto lookupsBefore = Graphics::DeviceProcLookups();
    std::shared_ptr<Graphics::ShaderResources> resources;
    ResourceCache::Key contentKey;
    const bool cacheable = !htileClear.has_value() && ResourceCacheEnabled() && shader.PipelineVariantId() != 0;
    // The object came from the resource cache: a compute template whose data buffers take this
    // dispatch's words at the record (ShaderResources::RefreshData).
    bool fromCache = false;
    bool refusedMove = false;
    double revalidateMs = 0;
    std::shared_ptr<Graphics::ShaderResources::DrawBindings> overlay;
    // The batches the pre-sync (PrepareDispatch) waited for are reaped first: their completions
    // (CPU write-backs) land before stage B or a cached object's Revalidate reads, and the flush
    // hook then finds nothing pending over their surfaces.
    if (prepared != nullptr && prepared->presyncSerial != 0) {
        // Timed alone: the phase entry exists only when a reap ran, and measures just the reap.
        timer.restart();
        ReapRecorded(prepared->presyncSerial);
        timer.phase(PhaseReap);
    }
    // The 'resources' phase below, split (APS5_PROFILE_DRAW) into what runs under the mutex: the
    // sub-phases are extra rows named "resources: ..." in the [dispatch] totals and the [indirect]
    // line, so the hold's biggest part (stage B's image lookups, the whole build of an address-based
    // shader) is named, and "resources: hook waits" says how much of the build was the nested flush
    // hook waiting for recorded work (a locked wait: it overlaps the other rows).
    const auto subPhaseStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto waitedBefore = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    // The image lookups of this call by outcome (stage B, a Revalidate or a locked build), for the
    // [indirect] line: reset here, read after the resources phase.
    if (profile && arguments != 0) Graphics::ThreadLookupOutcomes() = {};
    const auto noteRefusalBuild = [](bool inserted) {
        auto& moves = DispatchMoves();
        moves.rebuilt.fetch_add(1, std::memory_order_relaxed);
        if (inserted) moves.kept.fetch_add(1, std::memory_order_relaxed);
    };
    // The resource cache insert evicts the oldest entry, destroying its ShaderResources here when the
    // cache was the last owner: timed apart from the build.
    const auto insert = [&] {
        const auto insertStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        // The displaced entries go to the open batch: their destruction (descriptor sets, pooled
        // buffers, texture references) then runs on the release thread once the batch completed,
        // not here under the mutex. APS5_NO_DEFERRED_EVICTION=1 destroys them here as before.
        static const bool deferEviction = std::getenv("APS5_NO_DEFERRED_EVICTION") == nullptr;
        std::vector<std::shared_ptr<Graphics::ShaderResources>> evicted;
        state->resourceCache.Insert(contentKey, resources, deferEviction ? &evicted : nullptr);
        for (auto& object : evicted) {
            object->KeepStorageImageBytes(*state->recorder);
            state->recorder->Keep(std::move(object));
        }
        if (profile) {
            timer.add(PhaseResourcesInsert, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - insertStart).count());
            if (arguments != 0) d.indirectHold.phaseCounts[PhaseResourcesInsert] += evicted.size();
        }
    };
    if (prepared != nullptr && prepared->resources != nullptr) {
        // Stage A ran without the mutex (PrepareDispatch); stage B completes the build here.
        resources = std::move(prepared->resources);
        contentKey = std::move(prepared->key);
        // The build's own sub-phase totals before and after: the differences are stage B's parts
        // (an address-based build also runs its stage A here, under the mutex: 'A locked').
        const auto before = profile ? resources->Timing() : Graphics::ShaderResources::BuildTiming{};
        const auto completeStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        resources->Complete();
        require(prepared->describedLayoutKey.empty() || std::ranges::equal(resources->SetLayouts().mainKey, prepared->describedLayoutKey), "the set layout described for the dispatch's pipeline differs from its build");
        if (profile) {
            const auto completeWall = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - completeStart).count();
            timer.add(PhaseResourcesComplete, completeWall);
            const auto& after = resources->Timing();
            const auto images = after.bindingsMs - before.bindingsMs;
            const auto upload = after.uploadMs - before.uploadMs;
            const auto descriptors = after.descriptorsMs - before.descriptorsMs;
            const auto stageA = after.prepareMs - before.prepareMs;
            const auto stageB = after.completeMs - before.completeMs;
            timer.add(PhaseResourcesImages, images);
            timer.add(PhaseResourcesUpload, upload);
            timer.add(PhaseResourcesDescriptors, descriptors);
            timer.add(PhaseResourcesOther, std::max(0.0, stageB - images - upload - descriptors));
            timer.add(PhaseResourcesALocked, stageA);
            // What Complete() spends outside both stages' totals: an address-based build's lease
            // and mirror acquisition (prepareAddressBindings) before its locked stage A.
            timer.add(PhaseResourcesAddress, std::max(0.0, completeWall - stageA - stageB));
            timer.callMs[PhaseResourcesAUnlocked] += prepared->prepareMs;
            d.phaseTotals[PhaseResourcesAUnlocked] += prepared->prepareMs;
        }
        if (cacheable) {
            ++d.cacheMisses;
            if (prepared->exact) noteRefusalBuild(resources->Reusable());
            if (resources->Reusable()) insert();
        }
    } else if (cacheable) {
        // PrepareDispatch made the key already when it found the cached object, and carries the
        // object it found: revalidated as a map hit would be (an entry replaced meanwhile is left
        // alone by the pointer-conditional Remove). APS5_NO_PREPARED_FIND=1 looks the key up again.
        static const bool preparedFind = std::getenv("APS5_NO_PREPARED_FIND") == nullptr;
        const bool exactEntry = prepared != nullptr && prepared->exact;
        contentKey = prepared != nullptr && !prepared->key.empty() ? std::move(prepared->key) : DispatchContentKey(shaders[0], context.device);
        auto cached = preparedFind && prepared != nullptr && prepared->cached != nullptr ? std::move(prepared->cached) : state->resourceCache.Find(contentKey);
        if (cached != nullptr) {
            if (cached->Revalidate(shaders[0])) {
                bool usable = true;
                if (DispatchMove() && !exactEntry) {
                    auto& moves = DispatchMoves();
                    Graphics::ShaderResources::MoveProof proof;
                    auto moved = cached->MovedDispatchBuffers(shaders[0], state->recorder.get(), DispatchMoveBytes(), proof);
                    moves.checked.fetch_add(1, std::memory_order_relaxed);
                    if (moved.has_value() && threadGroup != std::array<std::uint32_t, 3>{}) {
                        const auto data = cached->shaderDataAllocation(shaders[0]);
                        if (data.has_value() && std::ranges::any_of(*moved, [&](const Graphics::ShaderResources::MovedBuffer& entry) { return entry.allocation == *data; })) moved.reset();
                    }
                    if (moved.has_value() && !moved->empty()) {
                        overlay = cached->PrepareDrawBindings(*state->recorder, *moved, true);
                        if (overlay != nullptr && overlay->allocation.set == VK_NULL_HANDLE) overlay.reset();
                        if (overlay == nullptr) {
                            proof.refusal = Graphics::ShaderResources::MoveRefusal::Bindings;
                            moved.reset();
                        }
                    }
                    if (!moved.has_value()) {
                        usable = false;
                        moves.refused[static_cast<std::size_t>(proof.refusal)].fetch_add(1, std::memory_order_relaxed);
                    } else if (moved->empty()) {
                        moves.exact.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        moves.moved.fetch_add(1, std::memory_order_relaxed);
                        moves.buffers.fetch_add(proof.buffers, std::memory_order_relaxed);
                        moves.bytes.fetch_add(proof.bytes, std::memory_order_relaxed);
                        noteDispatchMoveSize(proof.bytes);
                    }
                }
                if (!usable) {
                    contentKey = DispatchContentKey(shaders[0], context.device, false);
                    refusedMove = true;
                    cached = state->resourceCache.Find(contentKey);
                    if (cached != nullptr && cached->Revalidate(shaders[0])) usable = true;
                    else if (cached != nullptr) {
                        state->resourceCache.Remove(contentKey, cached.get());
                        ++d.cacheInvalidated;
                        state->recorder->Keep(std::move(cached));
                    }
                }
                if (usable) {
                    if (refusedMove || exactEntry) DispatchMoves().served.fetch_add(1, std::memory_order_relaxed);
                    resources = std::move(cached);
                    fromCache = true;
                    ++d.cacheHits;
                }
            } else {
                state->resourceCache.Remove(contentKey, cached.get());
                ++d.cacheInvalidated;
                // Off the mutex with its batch, as an evicted entry (insert()).
                cached->KeepStorageImageBytes(*state->recorder);
                state->recorder->Keep(std::move(cached));
            }
            if (profile) {
                revalidateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - subPhaseStart).count();
                timer.add(PhaseResourcesRevalidate, revalidateMs);
            }
        }
    }
    if (resources == nullptr) {
        const auto buildStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        resources = std::make_shared<Graphics::ShaderResources>(context, shaders[0], snapshots, false, prepared != nullptr ? prepared->dispatchThreads : 0, htileClear);
        if (profile) timer.add(PhaseResourcesFullBuild, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildStart).count());
        if (cacheable) {
            ++d.cacheMisses;
            if (refusedMove) noteRefusalBuild(resources->Reusable());
            if (resources->Reusable()) insert();
        }
    }
    resources->PatchPushConstants(pushBytes);
    if (profile) timer.add(PhaseResourcesHookWaits, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    // The lookup outcomes of this call: rows with counts on the [indirect] line, and a summary for
    // the longest call.
    if (profile && arguments != 0) {
        const auto& outcomes = Graphics::ThreadLookupOutcomes();
        for (std::size_t kind = 0; kind < Graphics::LookupOutcomes::Count; ++kind) {
            if (outcomes.counts[kind] == 0) continue;
            const auto row = DispatchPhaseCount + kind;
            timer.callMs[row] += outcomes.ms[kind];
            d.indirectHold.phaseMs[row] += outcomes.ms[kind];
            d.indirectHold.phaseCounts[row] += outcomes.counts[kind];
        }
    }
    if (profile && std::chrono::steady_clock::now() - d.cacheReport > std::chrono::seconds(10)) {
        d.cacheReport = std::chrono::steady_clock::now();
        AgcDriver::ProfilePrint_nid_no_patch("[rescache] (cumulative): %llu hits, %llu misses, %llu invalidated, %zu entries (dispatch + draw); template hits: %llu refreshed the data buffers (revalidate %.1f ms), %llu had the same words, %llu paired their data bindings by number (no image table binding of their own); Find calls %llu, Touch calls %llu\n", static_cast<unsigned long long>(d.cacheHits), static_cast<unsigned long long>(d.cacheMisses), static_cast<unsigned long long>(d.cacheInvalidated), state->resourceCache.Size(), static_cast<unsigned long long>(d.templateRefreshed), d.templateRevalidateMs, static_cast<unsigned long long>(d.templateSameWords), static_cast<unsigned long long>(d.templateByNumber), static_cast<unsigned long long>(ResourceCache::Finds()), static_cast<unsigned long long>(ResourceCache::Touches()));
        AgcDriver::ProfilePrint_nid_no_patch("[vk] deviceProc lookups inside the device call (cumulative): %llu (%.2f per dispatch); pre-dispatch barriers recorded %llu, skipped %llu; compute pipelines created %llu (%llu under the GPU mutex)\n", static_cast<unsigned long long>(d.procLookups), d.profiledDispatches != 0 ? static_cast<double>(d.procLookups) / static_cast<double>(d.profiledDispatches) : 0.0, static_cast<unsigned long long>(d.preBarriersRecorded), static_cast<unsigned long long>(d.preBarriersSkipped), static_cast<unsigned long long>(computePipelinesCreated.load(std::memory_order_relaxed)), static_cast<unsigned long long>(computePipelinesCreatedLocked.load(std::memory_order_relaxed)));
        reportRecipes();
        reportDispatchMove();
    }
    timing.Mark("shader_resources");
    timer.phase(PhaseResources);
    static const bool verifySetLayouts = std::getenv("APS5_VERIFY_SET_LAYOUTS") != nullptr;
    if (verifySetLayouts && objects != nullptr) require(objects->resourcesLayout == resources->Layout(), "the resident compute pipeline's set layout differs from the dispatch's");
    if (objects == nullptr) {
        if (verifySetLayouts && context.descriptorCache != nullptr) {
            const auto described = Graphics::DescribeSetLayouts(context, shaders);
            require(described.layouts.main == resources->Layout() && !described.layouts.split, "the set layout described from the compute shader differs from the dispatch's");
        }
        const auto layouts = resources->SetLayouts();
        objects = computePipeline(context, shader, layouts.main, layouts.mainKey, pushStages, programAddress);
        timing.Mark("pipeline_create");
        timer.phase(PhasePipeline);
    }
    DispatchCompletion completion{context, shaders, pushStages, pushBytes, objects, resources, contentKey, cacheable, fromCache, revalidateMs, overlay, lookupsBefore, timer, timing, groupsText};
    return recordCompletedDispatch(shader, x, y, z, arguments, programAddress, recipeOut, threadGroup, htileClear, completion);
}

}
}
}
}
}
}
}
}
