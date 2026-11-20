#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexPlanScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void recordPreparedDraw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, PreparedDraw& prepared) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    static AgcDriver::Mutex dumpMutex;
    static std::map<std::uint64_t, int> dumped;
    auto& timing = *prepared.timing;
    auto& timer = prepared.timer;
    auto& outcome = prepared.outcome;
    const auto* built = prepared.built;
    const auto waitedBefore = prepared.waitedBefore;
    auto& ownWaitedMs = prepared.ownWaitedMs;
    const auto report = [&](const char* suffix) { reportDrawEnd(state, timer, built, outcome, waitedBefore, ownWaitedMs, suffix); };
    auto* recipeOut = prepared.recipeOut;
    const auto& stencilReferences = prepared.stencilReferences;
    auto& inputs = prepared.inputs;
    const auto* args = prepared.args;
    const int dumpLimit = prepared.dumpLimit;
    auto& targets = prepared.targets;
    const auto& targetViews = prepared.targetViews;
    auto* recorder = prepared.recorder;
    auto& resolved = prepared.resolved;
    auto& resources = resolved.resources;
    const auto& contentKey = resolved.contentKey;
    const bool cacheable = resolved.cacheable;
    auto& indirect = prepared.indirect;
    const bool completion = prepared.completion;
    const bool listed = prepared.listed;
    const bool recorded = prepared.recorded;
    const bool lean = prepared.lean;
    const bool multisampled = prepared.multisampled;
    const auto& masked = prepared.masked;
    const auto& stencilReplay = prepared.stencilReplay;
    const auto& pipeline = prepared.pipeline;
    const auto& owners = prepared.owners;
    const auto& framebuffer = prepared.framebuffer;
    const auto& reordered = prepared.reordered;
    const auto& reorderPass = prepared.reorderPass;
    const auto& depthLocations = prepared.depthLocations;
    if (lean) {
        RecordedDraw record;
        record.recorder = recorder;
        record.resources = resources;
        record.moved = resolved.moved;
        record.pipeline = pipeline;
        record.framebuffer = framebuffer;
        record.targetViews = targetViews;
        record.targets = owners;
        for (const auto& binding : targets) record.targetSlices.emplace_back(binding.color.arraySlice, binding.color.arrayLastSlice != 0 ? binding.color.layers : 1u);
        for (const auto& binding : targets) {
            if (binding.proxied) record.proxies.push_back(binding.resident);
            if (binding.multisampled) {
                record.multisampleImages.push_back(binding.multisampleImage);
                record.multisampleTargets.push_back(binding.multisampleOwner);
            }
            if (binding.stagedTemporary != nullptr) {
                record.multisampleImages.push_back(binding.stagedTemporary->Image());
                const auto& color = binding.color;
                const auto bounds = VkRect2D{{0, 0}, color.extent};
                auto area = state.scissor;
                area.offset.x = std::max(area.offset.x, bounds.offset.x);
                area.offset.y = std::max(area.offset.y, bounds.offset.y);
                const auto right = std::min<std::int64_t>(static_cast<std::int64_t>(state.scissor.offset.x) + state.scissor.extent.width, color.extent.width);
                const auto bottom = std::min<std::int64_t>(static_cast<std::int64_t>(state.scissor.offset.y) + state.scissor.extent.height, color.extent.height);
                area.extent = {static_cast<std::uint32_t>(std::max<std::int64_t>(right - area.offset.x, 0)), static_cast<std::uint32_t>(std::max<std::int64_t>(bottom - area.offset.y, 0))};
                record.staged.push_back({binding.stagedTemporary, binding.stagedTarget, color.stagedMask, color.samples, area});
            }
        }
        record.indirect = args != nullptr ? &indirect : nullptr;
        record.listed = listed;
        record.completion = completion;
        record.waited = outcome.waited;
        record.stencilReplay = stencilReplay;
        record.reordered = reordered;
        record.reorderPass = reorderPass;
        record.depthLocations = depthLocations;
        record.spaceKeep = &prepared.spaceKeep;
        record.masked = prepared.masked.has_value() ? &*prepared.masked : nullptr;
        recordDraw(context, state, draw, shaders, inputs, record, outcome, timer, ownWaitedMs, prepared.trace);
        if (prepared.passAttachments) rememberAttachments(context, state, prepared);
        // The recipe for the caller's draw-cache entry (design_cpu_final M8, rule R3 for draws):
        // only a template the resource cache serves under this content key (reusable: no lease,
        // no copied writes, every direct region import- or mirror-served) of a direct draw, so a
        // hit's proof is the template's ProveCurrent and nothing needs completion work.
        if (recipeOut != nullptr && DrawRecipes() && cacheable && !outcome.waited && args == nullptr && resources->Reusable() && !resources->SpaceReuse() && !multisampled && !pipeline->ReordersSamples()) {
            auto recipe = std::make_shared<DrawRecipe>();
            recipe->device = context.device;
            recipe->templateRef = resources;
            recipe->key = contentKey;
            recipe->pipeline = pipeline;
            recipe->framebuffer = framebuffer;
            for (const auto& owner : owners) recipe->targets.emplace_back(owner);
            recipe->targetViews = targetViews;
            recipe->passKey = DrawRenderPassKey(context, state, targetViews);
            recipe->vertexInput = inputs.VertexLayout();
            recipe->pushStages = PushConstantStages(shaders);
            if (recipe->pushStages != 0) {
                recipe->pushBytes = AssemblePushConstants(shaders);
                resources->PatchPushConstants(recipe->pushBytes);
            }
            recipe->masked = masked;
            recipe->fragmentOutputs = inputs.fragmentOutputs;
            recipe->shaderStages = inputs.shaderStages;
            recipe->hasDepth = state.depth.has_value();
            *recipeOut = std::move(recipe);
        }
        timing.Mark("draw_and_resource_release");
        report(outcome.waited ? (outcome.reason == SyncLease ? " recorded then waited (lease)" : " recorded then waited (copied writes)") : completion ? " recorded with completion" : " recorded");
        return;
    }
    if (prepared.passAttachments) attachmentMemo().valid = false;
    const bool gpuIndirect = args != nullptr && indirect.path == IndirectDrawPath::Gpu;
    const auto touches = [&](std::uint64_t begin, std::uint64_t end) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (resources->WritesOverlap(begin, bytes) || resources->ReadsOverlap(begin, bytes)) return true;
        if (!gpuIndirect) return false;
        return (begin < args->arguments + args->RangeBytes() && args->arguments < end) || (args->countIndirect && begin < args->countAddress + 4 && args->countAddress < end);
    };
    static_cast<void>(StorageTexture::StoreDrawnLinearTouching("draw access", [&](std::uint64_t begin, std::uint64_t end) { return touches(begin, end) || std::any_of(inputs.inPlaceReads.begin(), inputs.inPlaceReads.end(), [&](const auto& read) { return read.first < end && begin < read.second; }); }, resources->HoldsLease() || resources->UsesAddressTable() || resources->HasCopiedWrites()));
    if (UsesMeshWindow(shaders)) PrepareMeshWindow(context);
    APS5_LOG_CHARS_OUT_DEBUG("Creating CommandBatch");
    std::optional<CommandBatch> batch;
    if (!recorded) {
        // The color-target detiles below take their descriptor sets from a fresh batch.
        if (context.detiler != nullptr) context.detiler->BeginBatch();
        batch.emplace(context);
    }
    using CommandClass = Recorder::CommandClass;
    const auto countBarrier = [&](std::uint32_t count = 1) { if (recorded) Recorder::CountBarriers(CommandClass::Draw, count); };
    // A queued DCC key store over memory the draw writes or reads in place (unknown for an
    // address-based build), or over its GPU-side records, lands before it, as before a dispatch
    // (VulkanDevice::dispatch).
    if (recorded && recorder->HasQueuedKeyStores() && (resources->HoldsLease() || recorder->AnyQueuedKeyStore(touches))) recorder->FlushKeyStores();
    // A queued label store over such memory likewise (Recorder::RecordStore).
    if (recorded && recorder->HasQueuedStores() && (resources->HoldsLease() || recorder->AnyQueuedStore(touches))) recorder->FlushStores();
    const auto commands = recorded ? recorder->Commands() : batch->Handle();
    if (recorded) prepared.spaceKeep = reserveSpaceKeep(*recorder, *resources);
    std::optional<CommandState> batchRecording;
    if (!recorded) batchRecording.emplace().Begin(commands, false);
    auto& recording = recorded ? recorder->GraphicsState() : *batchRecording;
    if (recorded) recorder->PrepareSampleSlot();
    APS5_LOG_CHARS_OUT_DEBUG("CommandBatch created");
    // The draw's [gputime] class range: from its first barrier to the download barrier.
    const auto drawTiming = recorded ? recorder->BeginGpuTiming(CommandClass::Draw) : Recorder::NoTiming;
    if (recorded && Recorder::BarrierValidate()) {
        auto reads = resources->InPlaceReads();
        reads.insert(reads.end(), inputs.inPlaceReads.begin(), inputs.inPlaceReads.end());
        if (gpuIndirect) {
            reads.emplace_back(args->arguments, args->arguments + args->RangeBytes());
            if (args->countIndirect) reads.emplace_back(args->countAddress, args->countAddress + 4);
        }
        auto images = resources->StorageImages();
        for (const auto& binding : targets) {
            if (binding.resident != nullptr) images.emplace_back(binding.resident->Image(), true);
        }
        recorder->NoteAccess(CommandClass::Draw, Recorder::Access{reads, resources->GpuWrites(), images, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, resources->HoldsLease()});
    }
    std::unique_ptr<DeviceBuffer> scratch;
    std::shared_ptr<Buffer> argumentSnapshot;
    VkBuffer argumentBuffer = VK_NULL_HANDLE;
    VkDeviceSize argumentOffset = 0;
    bool rewritten = false;
    VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    upload.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    upload.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, 0, 1, &upload, 0, nullptr, 0, nullptr);
    countBarrier();
    if (state.depth) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        countBarrier();
    }
    APS5_LOG_CHARS_OUT_DEBUG("Upload barrier recorded");
    if (inputs.vertexInputBehindGpuWrite) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        countBarrier();
    }
    const auto pixelLdsSubgroup = PixelFunctionLdsSubgroup(shaders);
    if (pixelLdsSubgroup != 0u) PreparePixelFunctionLds(context, commands, pixelLdsSubgroup);
    std::shared_ptr<Buffer> meshArguments;
    if (state.stages.mesh && state.tessellationCapture != nullptr) {
        recordCaptureBarrier(context, commands);
        countBarrier();
        meshArguments = state.tessellationCapture->header;
        argumentBuffer = meshArguments->Handle();
    } else if (state.stages.mesh && args != nullptr) {
        meshArguments = recordMeshArguments(context, commands, recorder, recorded, state, draw, indirect, countBarrier, inputs.restartTable);
        argumentBuffer = meshArguments->Handle();
    } else if (!inputs.restartTable.empty()) {
        meshArguments = directMeshArguments(context, draw, inputs.restartTable);
    } else if (gpuIndirect) {
        rewritten = recordIndirectArguments(context, commands, recorder, recorded, indirect, scratch, argumentSnapshot, argumentBuffer, argumentOffset, countBarrier);
    }
    for (auto& binding : targets) {
        if (binding.proxied) {
            binding.resident->RecordAttachmentProxyLoad(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            countBarrier(2);
            continue;
        }
        if (binding.resident != nullptr) {
            imageBarrier(context, commands, binding.resident->Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, {VK_IMAGE_ASPECT_COLOR_BIT, binding.color.mip, 1, binding.resident->ImageDepth() == 1 ? binding.color.arraySlice : 0, binding.resident->ImageDepth() == 1 ? binding.color.layers : 1});
            countBarrier();
            continue;
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {binding.color.extent.width, binding.color.extent.height, 1};
        if (binding.banded) {
            copy.bufferRowLength = binding.bandPitch;
            copy.imageOffset = {binding.band.offset.x, binding.band.offset.y, 0};
            copy.imageExtent = {binding.band.extent.width, binding.band.extent.height, 1};
        }
        const VkBuffer linear = binding.gpuTiling ? binding.linearDevice->Handle() : binding.transfer->Handle();
        countBarrier(binding.gpuTiling ? 4 : 2);
        if (binding.gpuTiling) {
            CopyBuffer(context, commands, binding.tiled->Handle(), 0, binding.tiledDevice->Handle(), 0, binding.original.size());
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            context.detiler->Dispatch(commands, TextureTileMode::kR64KBX, binding.color.elementBytes, binding.tiledDevice->Handle(), 0, binding.linearDevice->Handle(), 0, binding.mip, false, 0, false, {.pipeBankXor = binding.color.pipeBankXor});
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        }
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Resolved(&DeviceFunctions::cmdCopyBufferToImage, "vkCmdCopyBufferToImage")(commands, linear, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        PerformanceTrace::Amount("Graphics.Copy", 1);
        PerformanceTrace::Amount("Graphics.Copy.bytes", binding.original.size());
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    std::vector<std::shared_ptr<void>> reorderKept;
    RecordSampleOrder(context, recording, reorderPass, reordered, true, reorderKept);
    APS5_LOG_OUT_DEBUG("Beginning pipeline renderExtent=%ux%u", state.renderExtent.width, state.renderExtent.height);
    pipeline->Begin(recording, *framebuffer, state.renderExtent, state, masked.has_value() ? &*masked : nullptr);
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline Begin OK");
    const auto drawUse = recorded ? resources->BeginDrawUse(*recorder) : nullptr;
    resources->BindDrawUse(commands, pipeline->Layout(), drawUse.get());
    if (pixelLdsSubgroup != 0u) BindPixelFunctionLds(context, commands, pipeline->Layout());
    if (UsesMeshWindow(shaders)) BindMeshWindow(context, commands, pipeline->Layout());
    APS5_LOG_CHARS_OUT_DEBUG("Resources bound");
    pushDrawConstants(*pipeline, commands, state, draw, shaders, *resources, nullptr, 0, meshArguments != nullptr ? meshArguments->DeviceAddress() : 0, !inputs.restartTable.empty(), inputs.meshLayerStride);
    APS5_LOG_CHARS_OUT_DEBUG("Push constants recorded");
    std::uint32_t subdraw = 0;
    if (recorded) noteDrawFaultToken(context, *recorder, commands, shaders, draw, static_cast<std::uint32_t>(state.stages.mesh.has_value() ? AgcDriver::FaultStage::mesh : AgcDriver::FaultStage::graphics), args);
    if (stencilReplay.empty()) recordDrawCommands(context, commands, state, draw, inputs, args != nullptr ? &indirect : nullptr, argumentBuffer, argumentOffset, prepared.trace, subdraw);
    else {
        recordStencilReplay(context, recording, state, draw, inputs, stencilReplay, shaders, *resources);
        if (recorded) for (const auto& [reference, selectedPipeline] : stencilReplay) recorder->Keep(selectedPipeline);
    }
    if (pipeline->SplitsFaces()) {
        pipeline->ContinueBackFaces(recording, state, masked.has_value() ? &*masked : nullptr);
        recordDrawCommands(context, commands, state, draw, inputs, args != nullptr ? &indirect : nullptr, argumentBuffer, argumentOffset, prepared.trace, subdraw);
    }
    if (pipeline->SplitsSecondChannel()) {
        pipeline->ContinueSecondChannel(recording, state, masked.has_value() ? &*masked : nullptr);
        recordDrawCommands(context, commands, state, draw, inputs, args != nullptr ? &indirect : nullptr, argumentBuffer, argumentOffset, prepared.trace, subdraw);
    }
    if (meshArguments != nullptr && recorded) recorder->Keep(meshArguments);
    if (argumentSnapshot != nullptr && recorded) recorder->Keep(argumentSnapshot);
    if (state.tessellationCapture != nullptr && recorded) {
        recorder->Keep(state.tessellationCapture->header);
        recorder->Keep(state.tessellationCapture->items);
    }
    if (args != nullptr) CountIndirectDraw(indirect.path, indirect.readMs, rewritten);
    auto checkRecords = indirectRecordCheck(args != nullptr ? &indirect : nullptr);
    APS5_LOG_CHARS_OUT_DEBUG("Draw recorded");
    if (recorded) recorder->EndPassSamples();
    EndRenderPass(context, commands);
    RecordSampleOrder(context, recording, reorderPass, reordered, false, reorderKept);
    if (depthLocations) {
        RecordDepthSampleLocations(context, commands, *depthLocations);
        countBarrier(2);
        reorderKept.push_back(depthLocations->keep);
    }
    if (recorded) {
        for (auto& object : reorderKept) recorder->Keep(std::move(object));
    }
    APS5_LOG_CHARS_OUT_DEBUG("Render pass ended");
    for (auto& binding : targets) {
        if (binding.proxied) {
            binding.resident->RecordAttachmentProxyStore(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            countBarrier(2);
            continue;
        }
        if (binding.resident != nullptr) {
            imageBarrier(context, commands, binding.resident->Image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, {VK_IMAGE_ASPECT_COLOR_BIT, binding.color.mip, 1, binding.resident->ImageDepth() == 1 ? binding.color.arraySlice : 0, binding.resident->ImageDepth() == 1 ? binding.color.layers : 1});
            countBarrier();
            continue;
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {binding.color.extent.width, binding.color.extent.height, 1};
        if (binding.banded) {
            copy.bufferRowLength = binding.bandPitch;
            copy.imageOffset = {binding.band.offset.x, binding.band.offset.y, 0};
            copy.imageExtent = {binding.band.extent.width, binding.band.extent.height, 1};
        }
        countBarrier(binding.gpuTiling ? 4 : 2);
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        const VkBuffer linear = binding.gpuTiling ? binding.linearDevice->Handle() : binding.transfer->Handle();
        VkBufferMemoryBarrier reuse{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        reuse.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        reuse.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        reuse.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reuse.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reuse.buffer = linear;
        reuse.size = VK_WHOLE_SIZE;
        context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &reuse, 0, nullptr);
        context.Resolved(&DeviceFunctions::cmdCopyImageToBuffer, "vkCmdCopyImageToBuffer")(commands, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear, 1, &copy);
        PerformanceTrace::Amount("Graphics.Copy", 1);
        PerformanceTrace::Amount("Graphics.Copy.bytes", binding.original.size());
        if (binding.gpuTiling) {
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
            if (dumpLimit > 0) {
                std::lock_guard lock(dumpMutex);
                if (dumped[binding.color.address] < dumpLimit) {
                    binding.dump = std::make_unique<Buffer>(context, binding.linearDevice->Size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                    CopyBuffer(context, commands, binding.linearDevice->Handle(), 0, binding.dump->Handle(), 0, binding.linearDevice->Size());
                }
            }
            context.detiler->Dispatch(commands, TextureTileMode::kR64KBX, binding.color.elementBytes, binding.linearDevice->Handle(), 0, binding.tiledDevice->Handle(), 0, binding.mip, true, 0, false, {.pipeBankXor = binding.color.pipeBankXor});
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, binding.tiledDevice->Handle(), 0, binding.tiled->Handle(), 0, binding.original.size());
        }
    }
    VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    download.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    download.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | inputs.shaderStages, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
    countBarrier();
    if (recorded) recorder->EndGpuTiming(drawTiming);
    APS5_LOG_CHARS_OUT_DEBUG("Download barrier recorded");
    APS5_LOG_CHARS_OUT_DEBUG("SubmitAndWait");
    timer.phase(PhaseRecord);
    if (recorded) {
        keepRecordedDraw(*recorder, resources, pipeline, framebuffer, inputs, owners, [&] { std::vector<std::pair<std::uint32_t, std::uint32_t>> slices; for (const auto& binding : targets) slices.emplace_back(binding.color.arraySlice, binding.color.arrayLastSlice != 0 ? binding.color.layers : 1u); return slices; }(), state.scissor, std::move(scratch), std::move(checkRecords), listed, completion, outcome, drawUse, prepared.spaceKeep != nullptr ? prepared.spaceKeep.get() : nullptr);
        timer.phase(PhaseKeep);
        if (outcome.waited) {
            // The wait the dispatch path makes for such work (source 3, "address-based"): the
            // write-back runs before the next packet, so the CPU never reads copied results or
            // touches a leased allocation before they landed.
            Recorder::CountSync(3);
            const auto ownBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
            recorder->Sync();
            if (profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
            timer.phase(PhaseSync);
        }
        report(outcome.waited ? (outcome.reason == SyncLease ? " recorded then waited (lease)" : " recorded then waited (copied writes)") : completion ? " recorded with completion" : " recorded");
        return;
    }
    {
        // The recorder sync SubmitAndWait makes is the draw's own wait, not a nested hook wait.
        const auto ownBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
        batch->SubmitAndWait();
        if (!stencilReferences.empty()) resources->CheckFaults();
        if (profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
    }
    if (checkRecords) checkRecords();
    timer.phase(PhaseSync);
    APS5_LOG_CHARS_OUT_DEBUG("SubmitAndWait OK");
    for (const auto& binding : targets) GuestMemory::CheckRange(reinterpret_cast<const void*>(binding.color.MemoryAddress()), binding.color.bytes, 256, true);
    for (auto& binding : targets) {
        if (!binding.dump) continue;
        std::lock_guard lock(dumpMutex);
        char name[64];
        std::snprintf(name, sizeof(name), "target_%llx_%d.raw", static_cast<unsigned long long>(binding.color.address), dumped[binding.color.address]++);
        if (std::FILE* file = std::fopen(name, "wb")) {
            const std::uint32_t header[3] = {binding.color.extent.width, binding.color.extent.height, static_cast<std::uint32_t>(binding.color.format)};
            std::fwrite(header, sizeof(header), 1, file);
            std::fwrite(binding.dump->Bytes().data(), 1, binding.dump->Bytes().size(), file);
            std::fclose(file);
        }
    }
    APS5_LOG_CHARS_OUT_DEBUG("Shader resources WriteBack");
    resources->WriteBack();
    APS5_LOG_CHARS_OUT_DEBUG("Shader resources WriteBack OK");
    static const bool skipTargetWrite = std::getenv("APS5_NO_TARGET_WRITEBACK") != nullptr;
    for (const auto& binding : targets) {
        if (skipTargetWrite) break;
        if (binding.resident != nullptr) {
            // The results stay on the GPU until something reads the target's memory.
            binding.resident->MarkSliceDirty(binding.color.arraySlice, binding.color.arrayLastSlice != 0 ? binding.color.layers : 1u);
            continue;
        }
        if (binding.gpuTiling) GuestMemory::WriteChanged(binding.color.address, binding.tiled->Bytes(), binding.original);
        else if (binding.banded) GuestMemory::WriteChanged(binding.color.address + binding.bandOffset, binding.transfer->Bytes(), binding.original);
        else WriteColorTarget(binding.color, binding.transfer->Bytes());
        // The stored texels are the whole target now, so later reads must see them rather than a fast clear.
        const auto targetBytes = ColorTargetLayout(binding.color.extent.width, binding.color.extent.height, binding.color.tileMode, binding.color.elementBytes).Bytes();
        MarkDccUncompressed(binding.color.dccAddress, targetBytes, colorKeyCount(binding.color, targetBytes));
    }
    timer.phase(PhaseWriteBack);
    if (profile && traceDraws) {
        // A synchronous draw's inputs and target sample are described as a rendering debug aid;
        // the ~0.5 ms that takes (Describe samples every bound range and reads its DCC keys) is
        // shown as its own phase and kept out of the plain profile.
        std::size_t nonzero = 0;
        std::size_t sampled = 0;
        if (!targets.empty() && targets.front().resident == nullptr) {
            const auto bytes = targets.front().gpuTiling ? targets.front().tiled->Bytes() : targets.front().transfer->Bytes();
            sampled = bytes.size() / 64;
            for (std::size_t i = 0; i < bytes.size(); i += 64) nonzero += bytes[i] != std::byte{0};
        }
        std::fprintf(stderr, "[draw]   inputs:%s\n", resources->Describe().c_str());
        if (targets.size() > 1) {
            std::string list;
            for (const auto& binding : targets) {
                char entry[64];
                std::snprintf(entry, sizeof(entry), " 0x%llx(%d,%ux%u)", static_cast<unsigned long long>(binding.color.address), static_cast<int>(binding.color.format), binding.color.extent.width, binding.color.extent.height);
                list += entry;
            }
            std::fprintf(stderr, "[draw]   targets:%s\n", list.c_str());
        }
        timer.phase(PhaseDescribe);
        char suffix[160];
        std::snprintf(suffix, sizeof(suffix), " synchronous (%s), first 0x%llx format %d (%zu of %zu sampled bytes nonzero)", SyncReasonNames[outcome.reason], static_cast<unsigned long long>(state.color.address), static_cast<int>(state.color.format), nonzero, sampled);
        report(suffix);
    } else {
        report(" synchronous");
    }
    APS5_LOG_CHARS_OUT_DEBUG("Draw finished");
}

}

constexpr std::uint64_t MaxCapturedTriangles = 1ull << 20u;

}
}
}
}
}
}
