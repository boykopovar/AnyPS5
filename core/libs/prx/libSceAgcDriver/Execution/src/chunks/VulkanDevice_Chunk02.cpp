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
bool VulkanDevice::HasComputePipeline(const ShaderRecompiler::RecompileResult& shader) const {
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto key = ComputePipelineKey::Of(shader, Graphics::PushConstantStages(shaders) != 0);
    std::lock_guard pipelines(state->computePipelinesMutex);
    return !key.identity.Empty() && (state->computePipelines.contains(key) || state->computePipelinesBuilding.contains(key));
}

void VulkanDevice::PrebuildComputePipeline(const ShaderRecompiler::RecompileResult& shader, std::uint64_t programAddress) {
    if (shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) return;
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    const auto context = graphicsContext();
    if (context.descriptorCache == nullptr) return;
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::ComputePushConstantBytes) return;
    if (HasComputePipeline(shader)) return;
    const CompileBudget::Hold hold(BackgroundCompileBudget(), shader.spirv.size() * sizeof(std::uint32_t));
    if (HasComputePipeline(shader)) return;
    const auto layouts = Graphics::DescribeSetLayouts(context, shaders);
    static_cast<void>(computePipeline(context, shader, layouts.layouts.main, layouts.mainKey, pushStages, programAddress));
}

bool VulkanDevice::ImportGuestMemory(const GuestAllocations::Mapped& ranges, std::uint64_t generation, bool adoptDevice) {
    if (adoptDevice) GuestMemory::AssertGpuLockHeld("VulkanDevice::ImportGuestMemory");
    return Graphics::ImportMappedRanges(graphicsContext(), ranges, generation, adoptDevice);
}

void VulkanDevice::PrepareForReplacement() {
    GuestMemory::AssertGpuLockHeld("VulkanDevice::PrepareForReplacement");
    require(state->recorder != nullptr && Graphics::Recorder::Active() == state->recorder.get(), "device replacement requires its active recorder");
    state->journalReplayPending.store(false, std::memory_order_release);
    if (state->journalReplay != nullptr) {
        state->journalReplay->Cancel();
        state->journalReplay->Wait();
        state->journalReplay.reset();
    }
    DriverDetail::AwaitLibraryPrebuilds();
    Graphics::FlushCachedTextures(state->device);
    Graphics::PublishAllShadows(state->context, Graphics::PublishReason::Teardown);
    WaitIdle();
    Graphics::DestroyShadows(state->device);
    Graphics::ClearCachedPipelines(state->device);
    std::lock_guard pipelines(state->computePipelinesMutex);
    state->computePipelines.clear();
}

void VulkanDevice::WaitIdle() {
    APS5_LOG_CHARS_OUT_DEBUG("VulkanDevice::WaitIdle begin");
    static_cast<void>(Graphics::StorageTexture::StoreDrawnLinear("wait idle"));
    RetirePresents(0);
    if (state->recorder) {
        if (!state->recorder->Idle()) Graphics::Recorder::CountSync(0, __builtin_return_address(0));
        state->recorder->Sync();
    }
    std::unique_lock presentQueueLock(state->presentQueueMutex, std::defer_lock);
    if (state->presentQueue != VK_NULL_HANDLE) presentQueueLock.lock();
    std::unique_lock settleQueueLock(state->settleQueueMutex, std::defer_lock);
    if (state->settleQueue != VK_NULL_HANDLE) settleQueueLock.lock();
    std::unique_lock sparseQueueLock(state->sparseQueueMutex, std::defer_lock);
    if (state->sparseQueue != VK_NULL_HANDLE) sparseQueueLock.lock();
    check(state->DeviceFunction<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(state->device), "vkDeviceWaitIdle");
    Graphics::CheckPixelFunctionLds(graphicsContext());
    APS5_LOG_CHARS_OUT_DEBUG("VulkanDevice::WaitIdle complete");
}

namespace {

bool OpportunisticReap() {
    static const bool enabled = std::getenv("APS5_NO_OPPORTUNISTIC_REAP") == nullptr;
    return enabled;
}

}

void VulkanDevice::StoreIdleResults(bool reapFirst, const std::function<bool()>& stop) {
    if (!Graphics::StorageTexture::AnyPending()) return;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    static_cast<void>(Graphics::StorageTexture::FlushAllPendingUntil("idle", GuestMemory::YieldToGpuMutexWaiters(stop), state->device));
    SubmitRecorded(reapFirst);
}

void VulkanDevice::SubmitRecorded(bool reapFirst) {
    if (!state->recorder) return;
    if (reapFirst && OpportunisticReap()) state->recorder->Reap();
    state->recorder->Submit();
}

bool VulkanDevice::CanWaitUnlocked() const {
    return state->recorder && state->recorder->HasTimeline();
}

std::uint64_t VulkanDevice::SubmitAndEpoch() {
    if (!state->recorder) return 0;
    if (OpportunisticReap()) state->recorder->Reap();
    return state->recorder->SubmitAndEpoch();
}

void VulkanDevice::WaitRecorded(std::uint64_t serial) {
    if (state->recorder) state->recorder->WaitSerial(serial);
}

std::uint64_t VulkanDevice::SubmittedEpoch() const {
    return state->recorder ? state->recorder->NewestSubmitted() : 0;
}

bool VulkanDevice::WaitRecordedFor(std::uint64_t serial, std::uint64_t timeoutNanos) {
    return state->recorder && state->recorder->WaitSerialFor(serial, timeoutNanos);
}

void VulkanDevice::ReapRecorded(std::uint64_t serial) {
    if (!state->recorder) return;
    Graphics::Recorder::AnnounceSyncSite(__builtin_return_address(0));
    state->recorder->FinishUpTo(serial);
}

void VulkanDevice::ReapRecorded() {
    if (state->recorder) state->recorder->Reap();
}

std::optional<Graphics::Recorder::LabelHit> VulkanDevice::PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, Graphics::Recorder::LabelRefusal* refusal) const {
    if (!state->recorder) return std::nullopt;
    return state->recorder->PendingLabel(address, bytes, afterStamp, refusal);
}

bool VulkanDevice::OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const {
    return state->recorder && state->recorder->OpenWriteOverlaps(address, bytes);
}

void VulkanDevice::SealPassWriteOnly() {
    if (state->recorder) state->recorder->SealPassWriteOnly();
}

bool VulkanDevice::RecordedWritesSettled(std::uint64_t address, std::size_t bytes) const {
    if (!state->recorder) return true;
    const auto& recorder = *state->recorder;
    if (!recorder.PendingWriteSettled(address, bytes)) return false;
    return Graphics::Recorder::PendingCompletionLabels() == 0 || !recorder.CompletionLabelIn(address, bytes);
}

bool VulkanDevice::StoresPendingOver(std::uint64_t address, std::size_t bytes) const {
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{address, address + bytes}}};
    if (Graphics::StorageTexture::AnyPendingOverlaps(range) || Graphics::AnyShadowedOverlaps(range) || state->CopiedWriterOverlaps(address, bytes)) return true;
    return state->recorder && (state->recorder->PendingWriteOverlaps(address, bytes) || state->recorder->PendingLabelIn(address, bytes));
}

int VulkanDevice::WriteLabelOnGpu(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool reapFirst) {
    static const bool storeAtLabel = Graphics::StorageTexture::StoreAtLabelRequested(std::getenv("APS5_STORE_AT_LABEL"));
    if (storeAtLabel) Graphics::StorageTexture::FlushAllPending("label", state->device);
    if (queue == 0) static_cast<void>(Graphics::StorageTexture::StoreDrawnLinear("label"));
    Graphics::StorageTexture::StoreReadbackBeforeLabel("readback before label", state->device);
    if (!state->recorder || bytes.empty() || bytes.size() > 65536 || address % 4 != 0 || bytes.size() % 4 != 0) return 4;
    auto& recorder = *state->recorder;
    static const bool reapAllQueues = std::getenv("APS5_LABEL_REAP_ALL_QUEUES") != nullptr;
    if (reapFirst && (queue == 0 || reapAllQueues) && OpportunisticReap()) recorder.Reap();
    if (recorder.Idle()) return 1;
    static const bool drain = std::getenv("APS5_DRAIN_COMPLETION_LABELS") != nullptr;
    const auto context = graphicsContext();
    const auto* import = Graphics::HostImportFor(context, address, bytes.size());
    const bool listed = !state->copiedWriters->empty() || !Graphics::DrawCopiedWriters()->empty();
    const bool storesPending = Graphics::Recorder::PendingCompletionStores() != 0;
    const auto unregistered = import == nullptr && !listed && !storesPending ? Graphics::UnregisteredHostImport(context, address, bytes.size()) : nullptr;
    if (unregistered != nullptr) import = unregistered.get();
    if (import == nullptr) return 3;
    if (Graphics::AnyShadowedOverlaps(address, bytes.size())) Graphics::PublishShadow(address, bytes.size(), Graphics::PublishScope::PartialUnits, Graphics::PublishReason::Label);
    const auto recordStore = [&] {
        recorder.RecordStore(import->buffer, Graphics::HostImportOffset(*import, address), bytes, address);
        if (unregistered != nullptr) recorder.Keep(unregistered);
    };
    if (drain) {
        if (listed) return 2;
    } else if (bytes.size() <= 8 ? (listed || storesPending) : (storesPending && recorder.CompletionLabelIn(address, bytes.size()))) {
        recorder.AfterCompletions(address, bytes, stamp, queue, false);
        return 5;
    }
    static const bool gateAll = std::getenv("APS5_LABEL_GATE_ALL") != nullptr;
    if (gateAll ? recorder.HasCompletions() : state->CopiedWriterOverlaps(address, bytes.size())) {
        if (drain) return 2;
        recordStore();
        recorder.AfterCompletions(address, bytes, stamp, queue, true);
        GuestMemory::MarkWritten(address, bytes.size());
        return 5;
    }
    recordStore();
    recorder.NoteLabel(address, bytes, stamp, queue);
    GuestMemory::MarkWritten(address, bytes.size());
    static const bool submitNow = std::getenv("APS5_LABEL_SUBMIT_NOW") != nullptr;
    if (submitNow) recorder.Submit();
    return 0;
}

bool VulkanDevice::AfterRecordedWork(std::function<void()> action, bool reapFirst) {
    if (!state->recorder) return false;
    if (reapFirst && OpportunisticReap()) state->recorder->Reap();
    return state->recorder->AfterRecordedWork(std::move(action));
}

void VulkanDevice::NoteHtileMask(const Graphics::HtileMaskClear& clear) const {
    Graphics::NoteHtileMaskOnGpu(graphicsContext(), clear);
}

bool VulkanDevice::DumpSamplesOnGpu(std::uint64_t address) {
    static const bool syncDumps = std::getenv("APS5_SYNC_SAMPLE_DUMPS") != nullptr || std::getenv("APS5_NO_GPU_SAMPLE_DUMPS") != nullptr;
    if (syncDumps) return false;
    if (!state->recorder) return false;
    auto& recorder = *state->recorder;
    constexpr std::size_t bytes = Pm4::SampleDumpBytes;
    if (OpportunisticReap()) recorder.Reap();
    if (state->CopiedWriterOverlaps(address, bytes) || (Graphics::Recorder::PendingCompletionLabels() != 0 && recorder.CompletionLabelIn(address, bytes))) return false;
    const auto context = graphicsContext();
    Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "occlusion counter dump", Graphics::PublishScope::PartialUnits);
    const auto* import = Graphics::HostImportFor(context, address, bytes);
    if (import == nullptr || import->address == 0) return false;
    if (Graphics::AnyShadowedOverlaps(address, bytes)) Graphics::PublishShadow(address, bytes, Graphics::PublishScope::PartialUnits, Graphics::PublishReason::Label);
    if (!recorder.QueuesSampleDumps()) recorder.FlushStoresOverlapping(address, bytes);
    recorder.FlushKeyStoresOverlapping(address, bytes);
    if (!recorder.DumpSamples(import->address + (address - import->base), address)) return false;
    recorder.NotePendingWrite(address, bytes);
    GuestMemory::MarkWritten(address, bytes);
    return true;
}

bool VulkanDevice::FillBuffer(std::uint64_t address, std::size_t bytes, std::span<const std::uint32_t, 4> pattern) {
    if (!state->recorder || bytes == 0 || bytes % 16 != 0 || address % 16 != 0) return false;
    auto& recorder = *state->recorder;
    if (OpportunisticReap()) recorder.Reap();
    const auto context = graphicsContext();
    const Graphics::HostImport* import = Graphics::HostImportFor(context, address, bytes);
    if (import == nullptr) return false;
    static const bool alwaysSync = std::getenv("APS5_FILL_SYNC") != nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static std::uint64_t fills = 0, synced = 0, ordered = 0;
    static double syncedMs = 0;
    static double decideMs = 0, recordMs = 0, notesMs = 0;
    static std::uint64_t uniformFills = 0, patternFills = 0, doublingSteps = 0, patternBuffersReused = 0, patternBuffersMade = 0, elidedFills = 0, elidedBytes = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    static const bool chainInPlace = std::getenv("APS5_FILL_CHAIN") != nullptr;
    constexpr std::size_t PatternBufferBytes = 16u << 20u;
    constexpr std::size_t PatternBuffersKept = 8;
    const auto fillStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    ++fills;
    if (recorder.PendingWriteOverlaps(address, bytes)) {
        bool sync = alwaysSync || state->CopiedWriterOverlaps(address, bytes);
        if (!sync && Graphics::Recorder::PendingCompletionLabels() != 0) {
            sync = recorder.CompletionLabelIn(address, bytes);
            if (sync) ++fillCompletionSyncs;
        }
        if (sync) {
            ++synced;
            const auto syncStart = std::chrono::steady_clock::now();
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(address, bytes);
            syncedMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - syncStart).count();
            import = Graphics::HostImportFor(context, address, bytes);
            if (import == nullptr) return false;
        } else {
            ++ordered;
        }
    }
    if (profile) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport > std::chrono::seconds(10)) {
            lastReport = now;
            AgcDriver::ProfilePrint_nid_no_patch("[fill-sync] fills (cumulative): %llu synced (%llu with a completion label pending) waited %.0f ms, %llu ordered by barrier, %llu with no pending write; phases ms: decide %.0f record %.0f notes %.0f; %llu uniform, %llu pattern (%llu doubling steps in place; pattern buffers reused %llu, made %llu); %llu repeated fills elided (%.0f MiB)\n", static_cast<unsigned long long>(synced), static_cast<unsigned long long>(fillCompletionSyncs), syncedMs, static_cast<unsigned long long>(ordered), static_cast<unsigned long long>(fills - synced - ordered), decideMs, recordMs, notesMs, static_cast<unsigned long long>(uniformFills), static_cast<unsigned long long>(patternFills), static_cast<unsigned long long>(doublingSteps), static_cast<unsigned long long>(patternBuffersReused), static_cast<unsigned long long>(patternBuffersMade), static_cast<unsigned long long>(elidedFills), static_cast<double>(elidedBytes) / 1048576.0);
        }
    }
    auto phaseStart = fillStart;
    const auto phase = [&](double& total) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        total += std::chrono::duration<double, std::milli>(now - phaseStart).count();
        phaseStart = now;
    };
    phase(decideMs);
    using CommandClass = Graphics::Recorder::CommandClass;
    recorder.FlushKeyStoresOverlapping(address, bytes);
    const bool uniform = pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3];
    const std::array<std::uint32_t, 4> patternWords{pattern[0], pattern[1], pattern[2], pattern[3]};
    static const bool elideRepeats = std::getenv("APS5_NO_FILL_ELISION") == nullptr;
    const auto importSerial = elideRepeats ? Graphics::HostImportSerial(context, address, bytes, false) : 0;
    const auto unwatchSerial = GuestMemory::UnwatchSerial();
    if (elideRepeats) {
        const auto found = state->repeatedFills.find(address);
        if (found != state->repeatedFills.end() && found->second.bytes == bytes && found->second.pattern == patternWords && found->second.importSerial == importSerial && found->second.unwatchSerial == unwatchSerial && !Graphics::LeaseWorkPending() && GuestMemory::CollectWritesUncached(address, bytes) != 0 && GuestMemory::UnchangedSince(address, bytes, found->second.generation)) {
            ++elidedFills;
            elidedBytes += bytes;
            found->second.generation = GuestMemory::MarkWritten(address, bytes);
            phase(notesMs);
            return true;
        }
    }
    std::shared_ptr<Graphics::DeviceBuffer> patternBuffer;
    if (!uniform && !chainInPlace) {
        auto& buffers = state->patternBuffers;
        const auto found = std::find_if(buffers.begin(), buffers.end(), [&](const auto& entry) { return entry.first == patternWords; });
        if (found != buffers.end()) {
            patternBuffer = found->second;
            std::rotate(found, std::next(found), buffers.end());
            ++patternBuffersReused;
        } else {
            patternBuffer = std::make_shared<Graphics::DeviceBuffer>(context, PatternBufferBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            auto seed = std::make_shared<Graphics::Buffer>(context, 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            std::memcpy(seed->Bytes().data(), pattern.data(), 16);
            recorder.Keep(seed);
            const auto chain = recorder.Commands();
            const auto copyBuffer = context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
            const VkBufferCopy first{0, 0, 16};
            copyBuffer(chain, seed->Handle(), patternBuffer->Handle(), 1, &first);
            for (std::size_t done = 16; done < PatternBufferBytes; done *= 2) {
                Graphics::RecordMemoryBarrier(context, chain, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                const VkBufferCopy copy{0, done, done};
                copyBuffer(chain, patternBuffer->Handle(), patternBuffer->Handle(), 1, &copy);
            }
            Graphics::RecordMemoryBarrier(context, chain, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            if (buffers.size() >= PatternBuffersKept) buffers.erase(buffers.begin());
            buffers.emplace_back(patternWords, patternBuffer);
            ++patternBuffersMade;
        }
        recorder.Keep(patternBuffer);
    }
    recorder.FlushStoresOverlapping(address, bytes);
    VkAccessFlags covered = 0;
    const auto commands = recorder.Commands(&covered);
    const auto timing = recorder.BeginGpuTiming(CommandClass::Fill);
    if (Graphics::Recorder::BarrierValidate()) {
        const std::pair<std::uint64_t, std::uint64_t> range{address, address + bytes};
        recorder.NoteAccess(CommandClass::Fill, Graphics::Recorder::Access{{}, std::span(&range, 1), {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    if ((covered & VK_ACCESS_TRANSFER_WRITE_BIT) != 0 && Graphics::Recorder::MergeBarriers()) {
        Graphics::Recorder::CountMerged(CommandClass::Fill);
    } else {
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        Graphics::Recorder::CountBarriers(CommandClass::Fill);
    }
    const auto offset = Graphics::HostImportOffset(*import, address);
    if (uniform) {
        ++uniformFills;
        context.Resolved(&Graphics::DeviceFunctions::cmdFillBuffer, "vkCmdFillBuffer")(commands, import->buffer, offset, bytes, pattern[0]);
    } else if (patternBuffer != nullptr) {
        ++patternFills;
        std::vector<VkBufferCopy> copies;
        for (std::size_t done = 0; done < bytes; done += PatternBufferBytes) copies.push_back({0, offset + done, std::min(PatternBufferBytes, bytes - done)});
        context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, patternBuffer->Handle(), import->buffer, static_cast<std::uint32_t>(copies.size()), copies.data());
    } else {
        ++patternFills;
        auto seed = std::make_shared<Graphics::Buffer>(context, 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(seed->Bytes().data(), pattern.data(), 16);
        recorder.Keep(seed);
        const VkBufferCopy first{0, offset, 16};
        const auto copyBuffer = context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
        copyBuffer(commands, seed->Handle(), import->buffer, 1, &first);
        for (std::size_t done = 16; done < bytes;) {
            const auto chunk = std::min(done, bytes - done);
            Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            Graphics::Recorder::CountBarriers(CommandClass::Fill);
            const VkBufferCopy copy{offset, offset + done, chunk};
            copyBuffer(commands, import->buffer, import->buffer, 1, &copy);
            done += chunk;
            ++doublingSteps;
        }
    }
    constexpr VkAccessFlags filledAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, filledAccess);
    Graphics::Recorder::CountBarriers(CommandClass::Fill);
    recorder.EndGpuTiming(timing, bytes);
    recorder.MarkCovered(filledAccess);
    phase(recordMs);
    if (uniform) recorder.NotePendingWordFill(address, bytes, pattern[0]);
    else recorder.NotePendingWrite(address, bytes);
    const auto generation = GuestMemory::MarkWritten(address, bytes);
    if (elideRepeats && generation != 0) {
        constexpr std::size_t RepeatedFillsKept = 256;
        if (state->repeatedFills.size() >= RepeatedFillsKept && !state->repeatedFills.contains(address)) state->repeatedFills.clear();
        state->repeatedFills[address] = {bytes, patternWords, generation, importSerial, unwatchSerial};
    }
    phase(notesMs);
    return true;
}

namespace {

std::atomic<std::uint64_t> copiesVerified{0}, copySourceChanged{0}, copyDestinationChanged{0}, copyReaderMissed{0};
std::atomic<std::uint64_t> copiesAliased{0}, copyAliasCreated{0}, copyAliasNoSource{0}, copyAliasShape{0}, copyAliasRefused{0};

}
}
}
