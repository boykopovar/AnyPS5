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
std::span<const double, 5> VulkanDevice::PreparePhaseMs(const PreparedDispatch& prepared) {
    static_assert(PreparedDispatch::PreparePhaseCount == 5);
    return std::span<const double, 5>(prepared.phaseMs);
}

std::shared_ptr<RecipeHit> VulkanDevice::PrepareRecipe(const std::shared_ptr<const Recipe>& recipe, bool indirect) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& counters = Recipes().kinds[indirect ? 1 : 0];
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto miss = [&](RecipeMiss reason) {
        counters.precheckMisses[reason].fetch_add(1, std::memory_order_relaxed);
        return std::shared_ptr<RecipeHit>{};
    };
    if (!DispatchRecipes() || recipe == nullptr) return miss(MissNoRecipe);
    if (recipe->device != state->device) return miss(MissDevice);
    auto hit = std::make_shared<RecipeHit>();
    hit->recipe = recipe;
    hit->indirect = indirect;
    hit->resources = recipe->templateRef.lock();
    if (hit->resources == nullptr) return miss(MissTemplateGone);
    hit->objects = recipe->objects.lock();
    if (hit->objects == nullptr) return miss(MissObjectsGone);
    if (CachedPrecollect()) hit->resources->PrecollectSurfaces();
    if (!NoPresync() && CanWaitUnlocked()) {
        Presyncs().checked.fetch_add(1, std::memory_order_relaxed);
        if (Graphics::HostImportsUnchanged(graphicsContext(), recipe->presyncProof)) {
            counters.presyncsFromRecipe.fetch_add(1, std::memory_order_relaxed);
            hit->presyncSerial = presync(recipe->presyncSurfaces);
        } else {
            counters.presyncsRecomputed.fetch_add(1, std::memory_order_relaxed);
            hit->presyncSerial = presync(hit->resources->PresyncSurfaces());
        }
        if (hit->presyncSerial != 0) counters.presyncWaits.fetch_add(1, std::memory_order_relaxed);
    }
    if (profile) {
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        hit->precheckMs = static_cast<double>(ns) / 1e6;
        counters.precheckNs.fetch_add(static_cast<std::uint64_t>(ns), std::memory_order_relaxed);
    }
    return hit;
}

void VulkanDevice::Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared, std::shared_ptr<const Recipe>* recipe, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear, const std::vector<ShaderRecompiler::RegisteredImageView>* registeredImageViews) {
    static_cast<void>(dispatch(shader, x, y, z, 0, snapshots, programAddress, std::move(prepared), recipe, {}, htileClear, registeredImageViews));
}

VulkanDevice::IndirectOutcome VulkanDevice::DispatchIndirect(const ShaderRecompiler::RecompileResult& shader, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared, std::shared_ptr<const Recipe>* recipe, const std::array<std::uint32_t, 3>& threadGroup, const std::vector<ShaderRecompiler::RegisteredImageView>* registeredImageViews) {
    return dispatch(shader, 0, 0, 0, arguments, snapshots, programAddress, std::move(prepared), recipe, threadGroup, std::nullopt, registeredImageViews);
}

void VulkanDevice::decideIndirect(RecordedDispatch& record, IndirectOutcome& outcome, char* groupsText) {
    auto& recorder = *state->recorder;
    auto& timer = *record.timer;
    const auto& context = *record.context;
    const auto arguments = record.arguments;
    const bool threadDimensions = record.threadGroup != std::array<std::uint32_t, 3>{};
    const auto labelPending = [&] {
        for (std::uint64_t dword = arguments; dword < arguments + 12; dword += 4) {
            if (recorder.PendingLabel(dword, 4, 0).has_value()) return true;
        }
        return false;
    };
    if (Graphics::StorageTexture::FlushPending(arguments, 12, nullptr, "indirect dispatch arguments")) {
        outcome.cpuReason = 1;
        Graphics::Recorder::CountSync(2);
        recorder.Sync();
    } else if (record.shader->program->workgroupMemoryDwords != 0) {
        outcome.cpuReason = 8;
    } else if (labelPending() || state->CopiedWriterOverlaps(arguments, 12)) {
        outcome.cpuReason = 2;
    } else if ((record.argumentImport = Graphics::HostImportFor(context, arguments, 12)) == nullptr || (threadDimensions && record.argumentImport->address == 0)) {
        record.argumentImport = nullptr;
        outcome.cpuReason = 3;
    }
    if (timer.profile) {
        auto& hold = Dispatches().indirectHold;
        hold.writersScanned += state->copiedWriters->size();
        hold.drawWritersScanned += Graphics::DrawCopiedWriters()->size();
        hold.maxWriters = std::max(hold.maxWriters, state->copiedWriters->size());
    }
    timer.phase(PhaseDecide);
    if (outcome.cpuReason == 0) return;
    const auto readStart = std::chrono::steady_clock::now();
    Graphics::FinishLeaseWork();
    std::array<std::uint32_t, 3> groups{};
    GuestMemory::Read(arguments, std::as_writable_bytes(std::span(groups)), 4);
    if (threadDimensions) {
        record.threadLimit = groups;
        for (std::size_t axis = 0; axis < groups.size(); ++axis) groups[axis] = groups[axis] / record.threadGroup[axis] + (groups[axis] % record.threadGroup[axis] != 0 ? 1u : 0u);
    }
    outcome.argumentReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
    record.x = groups[0];
    record.y = groups[1];
    record.z = groups[2];
    record.cpuArguments = arguments;
    record.cpuReason = outcome.cpuReason;
    timer.phase(PhaseArgumentRead);
    record.arguments = 0;
    timer.indirect = false;
    std::snprintf(groupsText, 40, "%ux%ux%u", record.x, record.y, record.z);
}

void VulkanDevice::recordDispatch(RecordedDispatch& record) {
    auto& d = Dispatches();
    auto& recorder = *state->recorder;
    auto& timer = *record.timer;
    const auto& context = *record.context;
    auto& resources = *record.resources;
    const auto arguments = record.arguments;
    const auto* argumentImport = record.argumentImport;
    const auto* limit = state->properties.limits.maxComputeWorkGroupCount;
    if (argumentImport == nullptr && record.shader->program->workgroupMemoryDwords != 0 && (record.x > limit[0] || record.y > limit[1] || record.z > limit[2])) {
        char counted[128] = "";
        if (record.cpuArguments != 0) std::snprintf(counted, sizeof(counted), " (read on the CPU from the indirect arguments at 0x%llx, cpuReason %d)", static_cast<unsigned long long>(record.cpuArguments), record.cpuReason);
        char text[384];
        std::snprintf(text, sizeof(text), "Vulkan dispatch: workgroup count %ux%ux%u%s exceeds device limits %ux%ux%u and a split aliases the device-memory LDS slots, whose base is computed from gl_NumWorkGroups of a single chunk", record.x, record.y, record.z, counted, limit[0], limit[1], limit[2]);
        throw std::runtime_error(text);
    }
    auto recordFrom = timer.profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto recordStep = [&](DispatchPhase which) {
        if (!timer.profile) return;
        const auto now = std::chrono::steady_clock::now();
        timer.add(which, std::chrono::duration<double, std::milli>(now - recordFrom).count());
        recordFrom = now;
    };
    const bool barrierElision = Graphics::Recorder::MergeBarriers();
    const auto touches = [&](std::uint64_t begin, std::uint64_t end) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        return resources.WritesOverlap(begin, bytes) || resources.ReadsOverlap(begin, bytes) || (argumentImport != nullptr && begin < arguments + 12 && arguments < end);
    };
    if (recorder.HasQueuedKeyStores() && (resources.HoldsLease() || recorder.AnyQueuedKeyStore(touches))) recorder.FlushKeyStores();
    if (recorder.HasQueuedStores() && (resources.HoldsLease() || recorder.AnyQueuedStore(touches))) recorder.FlushStores();
    static_cast<void>(Graphics::StorageTexture::StoreDrawnLinearTouching("dispatch access", touches, resources.HoldsLease()));
    resources.RequireGuestBuffers();
    std::shared_ptr<ScratchPool> pool;
    if (const auto slotDwords = record.shader->program->scratchPoolSlotDwords; slotDwords != 0) {
        const std::uint64_t groups = argumentImport != nullptr ? std::numeric_limits<std::uint64_t>::max() : static_cast<std::uint64_t>(record.x) * record.y * record.z;
        pool = state->ScratchPoolFor(context, record.shader->program->Identity(), slotDwords, groups);
    }
    bool unordered = false;
    const auto overlapQueue = Graphics::Recorder::TakeDispatchOrder(unordered);
    const auto* groupLimit = state->properties.limits.maxComputeWorkGroupCount;
    const bool overlapCapable = overlapQueue != 0 && Graphics::Recorder::DispatchOverlapEnabled() && arguments == 0 && argumentImport == nullptr && record.threadGroup == std::array<std::uint32_t, 3>{} && record.shader->program->workgroupMemoryDwords == 0 && state->checkpointMarker == nullptr && state->bufferMarker == nullptr && record.x <= groupLimit[0] && record.y <= groupLimit[1] && record.z <= groupLimit[2] && !resources.HasHostGpuCopies();
    std::vector<Graphics::Recorder::RunAccess> runAccesses;
    if (overlapCapable) resources.RunAccesses(runAccesses);
    VkAccessFlags covered = 0;
    bool overlapped = false;
    const std::array<const void*, 2> runOwners{record.resources.get(), resources.MemoryOwner()};
    const auto commands = overlapCapable && unordered ? recorder.OverlapCommands(overlapQueue, runOwners, runAccesses, &covered, overlapped) : recorder.Commands(&covered);
    constexpr VkAccessFlags runLeadingCovered = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    if (overlapCapable && !overlapped && (covered & runLeadingCovered) != runLeadingCovered) covered = 0;
    recordStep(PhaseRecordCommands);
    recorder.Keep(record.objects);
    if (record.resources != nullptr) record.resources->KeepStorageImageBytes(recorder);
    recorder.Keep(record.resources);
    if (record.resources != nullptr) recorder.KeepBytes(record.resources.get(), record.resources->CopiedBytes());
    if (record.bindings != nullptr) {
        recorder.Keep(record.bindings);
        for (const auto& snapshot : record.bindings->snapshots) recorder.KeepBytes(snapshot.buffer.get(), snapshot.buffer->Bytes().size());
    }
    recordStep(PhaseRecordKeeps);
    if (record.dataRefresh != RecordedDispatch::DataRefresh::None) {
        const bool differs = record.dataRefresh == RecordedDispatch::DataRefresh::Words || resources.DataWordsHash() != record.dataWordsHash;
        if (timer.profile && record.shader->program != nullptr && record.shader->program->bindings.size() != resources.BindingCount()) ++d.templateByNumber;
        if (differs && (record.resourceKeyMatched ? resources.refreshData(commands, *record.shader, &recorder) : resources.RefreshData(commands, *record.shader, &recorder))) {
            covered = 0;
            ++d.templateRefreshed;
            d.templateRevalidateMs += record.revalidateMs;
            record.refreshed = true;
        } else {
            ++d.templateSameWords;
        }
        recordStep(PhaseRecordDataRefresh);
    }
    std::shared_ptr<Graphics::Buffer> threadArguments;
    if (record.threadGroup != std::array<std::uint32_t, 3>{}) {
        const auto data = resources.shaderDataBuffer(*record.shader);
        const auto limitDword = record.shader->program->imageMetadataDword;
        Graphics::Require(data != VK_NULL_HANDLE && limitDword >= 3, "Vulkan dispatch: a thread-dimension dispatch has no ShaderData thread limit");
        const VkDeviceSize limitByte = static_cast<VkDeviceSize>(limitDword - 3) * sizeof(std::uint32_t);
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        if (argumentImport != nullptr) {
            threadArguments = std::make_shared<Graphics::Buffer>(context, 24, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            Graphics::Require(recorder.RecordDispatchArguments(commands, argumentImport->address + (arguments - argumentImport->base), threadArguments->DeviceAddress(), &record.threadGroup), "Vulkan dispatch: thread-dimension indirect arguments need the argument kernel");
            const VkBufferCopy limit{12, limitByte, 12};
            context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, threadArguments->Handle(), data, 1, &limit);
        } else {
            context.Resolved(&Graphics::DeviceFunctions::cmdUpdateBuffer, "vkCmdUpdateBuffer")(commands, data, limitByte, sizeof(record.threadLimit), record.threadLimit.data());
        }
        resources.forgetShaderDataWords(*record.shader);
        covered = 0;
    }
    using CommandClass = Graphics::Recorder::CommandClass;
    if (Graphics::Recorder::BarrierValidate()) {
        if (argumentImport != nullptr) {
            const std::pair<std::uint64_t, std::uint64_t> argumentRange{arguments, arguments + 12};
            recorder.NoteAccess(CommandClass::IndirectArguments, Graphics::Recorder::Access{std::span(&argumentRange, 1), {}, {}, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT});
        }
        const auto reads = resources.InPlaceReads();
        const auto images = resources.StorageImages();
        recorder.NoteAccess(CommandClass::DispatchLeading, Graphics::Recorder::Access{reads, resources.GpuWrites(), images, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, resources.HoldsLease()});
    }
    if (argumentImport != nullptr) {
        const auto timing = recorder.BeginGpuTiming(CommandClass::IndirectArguments);
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        Graphics::Recorder::CountBarriers(CommandClass::IndirectArguments);
        recorder.EndGpuTiming(timing, 12);
        recorder.NotePendingRead(arguments, 12, Graphics::Recorder::ReadKind::Indirect);
    }
    constexpr VkAccessFlags shaderAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    if (overlapped) {
        if (record.refreshed) {
            Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT);
            Graphics::Recorder::CountBarriers(CommandClass::TemplateDataRefresh);
        }
    } else if ((covered & shaderAccess) == shaderAccess && barrierElision) {
        ++d.preBarriersSkipped;
        Graphics::Recorder::CountMerged(CommandClass::DispatchLeading);
    } else {
        const auto timing = recorder.BeginGpuTiming(CommandClass::DispatchLeading);
        const VkPipelineStageFlags leadingStages = overlapCapable ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        const VkAccessFlags leadingAccess = overlapCapable ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, leadingStages, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, leadingAccess);
        Graphics::Recorder::CountBarriers(CommandClass::DispatchLeading);
        recorder.EndGpuTiming(timing);
        ++d.preBarriersRecorded;
    }
    VkBuffer indirectBuffer = argumentImport != nullptr ? argumentImport->buffer : VK_NULL_HANDLE;
    VkDeviceSize indirectOffset = argumentImport != nullptr ? Graphics::HostImportOffset(*argumentImport, arguments) : 0;
    if (threadArguments != nullptr) {
        indirectBuffer = threadArguments->Handle();
        indirectOffset = 0;
        recorder.Keep(std::move(threadArguments));
    } else if (argumentImport != nullptr && argumentImport->address != 0) {
        auto checked = std::make_shared<Graphics::Buffer>(context, 12, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        if (recorder.RecordDispatchArguments(commands, argumentImport->address + (arguments - argumentImport->base), checked->DeviceAddress())) {
            indirectBuffer = checked->Handle();
            indirectOffset = 0;
            recorder.Keep(std::move(checked));
        }
    }
    context.BindComputePipeline(commands, record.objects->pipeline);
    if (record.bindings != nullptr) Graphics::Require(record.bindings->allocation.set != VK_NULL_HANDLE, "dispatch overlay has no descriptor set");
    if (record.bindings != nullptr) context.Resolved(&Graphics::DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout, 0, 1, &record.bindings->allocation.set, 0, nullptr);
    else resources.Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout);
    if (const auto dwords = record.shader->program->workgroupMemoryDwords; dwords != 0) {
        auto memory = state->WorkgroupMemoryFor(context, record.x, record.y, record.z, dwords);
        context.Resolved(&Graphics::DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout, ShaderRecompiler::WorkgroupMemoryDescriptorSet, 1, &memory->set, 0, nullptr);
        recorder.Keep(std::move(memory));
    }
    if (pool != nullptr) {
        context.Resolved(&Graphics::DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, record.objects->layout, ShaderRecompiler::ScratchPoolDescriptorSet, 1, &pool->set, 0, nullptr);
        recorder.Keep(std::move(pool));
    }
    if (record.pushStages != 0) {
        context.Resolved(&Graphics::DeviceFunctions::cmdPushConstants, "vkCmdPushConstants")(commands, record.objects->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::ComputePushConstantBytes, record.pushBytes->data());
    }
    static const bool reportingDispatch = LibcRunReportEnabled_nid_no_patch();
    if (reportingDispatch) recorder.NoteGuestWork(commands);
    const auto gpuTiming = recorder.BeginGpuTiming(record.programAddress != 0 ? record.programAddress : record.shader->program->variantId);
    AgcDriver::FaultToken faultToken = 0;
    if (state->checkpointMarker != nullptr || state->bufferMarker != nullptr) {
        faultToken = AgcDriver::FaultTokenRing::Instance().Claim();
        if (faultToken != 0) {
            if (auto* entry = AgcDriver::FaultTokenRing::Instance().Mutable(faultToken)) {
                entry->submitSerial = recorder.OpenSerial();
                entry->programAddress = record.programAddress;
                entry->variantId = record.shader != nullptr && record.shader->program != nullptr ? record.shader->program->variantId : 0;
                entry->stage = static_cast<std::uint32_t>(AgcDriver::FaultStage::compute);
                entry->argBuffer = 0;
                entry->argOffset = 0;
                entry->argStride = 0;
                entry->argCount = 0;
                for (auto& word : entry->argWords) word = 0;
                if (record.arguments != 0) {
                    entry->argBuffer = record.arguments;
                    entry->argOffset = 0;
                    std::array<std::uint32_t, 3> groups{};
                    entry->argStride = static_cast<std::uint32_t>(sizeof(groups));
                    try {
                        GuestMemory::ReadCommittedUnsynced(record.arguments, std::as_writable_bytes(std::span(groups)));
                        entry->argWords[0] = groups[0];
                        entry->argWords[1] = groups[1];
                        entry->argWords[2] = groups[2];
                        entry->argCount = static_cast<std::uint32_t>(groups.size());
                    } catch (...) {
                        entry->argCount = 0;
                    }
                } else if (record.cpuArguments != 0) {
                    constexpr std::uint32_t groupDwords = 3;
                    entry->argBuffer = record.cpuArguments;
                    entry->argOffset = 0;
                    entry->argStride = static_cast<std::uint32_t>(sizeof(std::uint32_t) * groupDwords);
                    entry->argWords[0] = record.x;
                    entry->argWords[1] = record.y;
                    entry->argWords[2] = record.z;
                    entry->argCount = groupDwords;
                }
                if (record.arguments != 0) std::snprintf(entry->desc, sizeof(entry->desc), "dispatch indirect");
                else std::snprintf(entry->desc, sizeof(entry->desc), "dispatch %ux%ux%u", record.x, record.y, record.z);
                recorder.NoteFaultToken(faultToken);
                std::atomic_ref<AgcDriver::FaultToken>(entry->token).store(faultToken, std::memory_order_release);
            }
        }
    }
    if (state->checkpointMarker != nullptr) state->checkpointMarker(commands, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(faultToken)));
    if (faultToken != 0 && state->bufferMarker != nullptr) state->bufferMarker(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state->markerBuffer, 0, faultToken);
    if (argumentImport != nullptr) {
        context.Resolved(&Graphics::DeviceFunctions::cmdDispatchIndirect, "vkCmdDispatchIndirect")(commands, indirectBuffer, indirectOffset);
    } else {
        if (record.x <= limit[0] && record.y <= limit[1] && record.z <= limit[2]) {
            context.Resolved(&Graphics::DeviceFunctions::cmdDispatch, "vkCmdDispatch")(commands, record.x, record.y, record.z);
        } else {
            char counted[128] = "";
            if (record.cpuArguments != 0) std::snprintf(counted, sizeof(counted), " (read on the CPU from the indirect arguments at 0x%llx, cpuReason %d)", static_cast<unsigned long long>(record.cpuArguments), record.cpuReason);
            const auto offsets = std::ranges::count_if(record.shader->program->specialization, [](const auto& constant) { return ShaderRecompiler::PipelineSpecialization::IsWorkgroupOffset(constant.id); });
            if (offsets != ShaderRecompiler::PipelineSpecialization::WorkgroupOffsetWords) {
                char text[320];
                std::snprintf(text, sizeof(text), "Vulkan dispatch: workgroup count %ux%ux%u%s exceeds device limits %ux%ux%u and the shader takes no workgroup offset", record.x, record.y, record.z, counted, limit[0], limit[1], limit[2]);
                throw std::runtime_error(text);
            }
            const auto dispatchChunk = context.Resolved(&Graphics::DeviceFunctions::cmdDispatch, "vkCmdDispatch");
            bool firstChunk = true;
            for (std::uint32_t zBase = 0; zBase < record.z; zBase += limit[2]) {
                for (std::uint32_t yBase = 0; yBase < record.y; yBase += limit[1]) {
                    for (std::uint32_t xBase = 0; xBase < record.x; xBase += limit[0]) {
                        if (!firstChunk) {
                            Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                            context.BindComputePipeline(commands, splitComputePipeline(context, *record.shader->program, *record.objects, {xBase, yBase, zBase}));
                        }
                        firstChunk = false;
                        dispatchChunk(commands, std::min(limit[0], record.x - xBase), std::min(limit[1], record.y - yBase), std::min(limit[2], record.z - zBase));
                    }
                }
            }
        }
    }
    recorder.EndGpuTiming(gpuTiming);
    if (faultToken != 0 && state->bufferMarker != nullptr) state->bufferMarker(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, state->markerBuffer, sizeof(std::uint32_t), faultToken);
    if (overlapCapable) {
        recorder.DeferDispatchBarrier(overlapQueue, runOwners, runAccesses);
        resources.CopyBacksAfterRun(record.resources);
    } else {
        const auto trailingTiming = recorder.BeginGpuTiming(CommandClass::DispatchTrailing);
        constexpr VkAccessFlags dispatchedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, dispatchedAccess);
        Graphics::Recorder::CountBarriers(CommandClass::DispatchTrailing);
        recorder.EndGpuTiming(trailingTiming);
        recorder.MarkCovered(dispatchedAccess);
    }
    recordStep(PhaseRecordBind);
    if (record.htileClear.has_value()) Graphics::ArmHtileDepthClear(record.htileClear->first, record.htileClear->second);
    resources.MarkGpuWrites(recorder);
    if (record.htileClear.has_value()) {
        Graphics::QueueHtileDepthClear(record.htileClear->first, record.htileClear->second);
        recorder.NotePendingWrite(record.htileClear->first, record.htileClear->second);
    }
    recordStep(PhaseRecordMarks);
    const bool completionNeeded = resources.NeedsCompletion() || record.htileClear.has_value();
    if (completionNeeded && resources.Reusable()) {
        auto kept = record.resources;
        recorder.OnComplete([kept, htileClear = record.htileClear] {
            kept->CheckFaults();
            if (htileClear.has_value()) Graphics::CompleteHtileDepthClear(htileClear->first, htileClear->second);
        });
    } else if (completionNeeded) {
        auto writers = state->copiedWriters;
        auto kept = record.resources;
        recorder.OnComplete([kept, writers, htileClear = record.htileClear] {
            writers->erase(std::remove(writers->begin(), writers->end(), kept), writers->end());
            kept->WriteBackBuffers();
            if (htileClear.has_value()) Graphics::CompleteHtileDepthClear(htileClear->first, htileClear->second);
        });
        if (resources.HasCopiedWrites()) writers->push_back(kept);
    }
    recordStep(PhaseRecordCompletion);
}

}
}
}
}
}
}
}
}
