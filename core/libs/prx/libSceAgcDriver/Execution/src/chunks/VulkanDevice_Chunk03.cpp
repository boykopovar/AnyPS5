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
bool CopyAliasDisabled() {
    static const bool disabled = std::getenv("APS5_NO_COPY_ALIAS") != nullptr;
    return disabled;
}

struct AliasPlan {
    std::shared_ptr<Graphics::StorageTexture> from;
    std::shared_ptr<Graphics::StorageTexture> to;
    const char* refusal = nullptr;
};

AliasPlan PlanAlias(const Graphics::Context& context, std::uint64_t destination, std::uint64_t source, std::size_t bytes) {
    AliasPlan plan;
    plan.from = Graphics::StorageTexture::FindLive(source, bytes);
    if (plan.from == nullptr) {
        plan.refusal = "no source";
        return plan;
    }
    if (!plan.from->ImageCoversGuestBytes()) {
        plan.refusal = "image byte coverage";
        return plan;
    }
    plan.to = Graphics::StorageTexture::FindLive(destination, bytes);
    plan.refusal = Graphics::StorageTexture::CopyRefusal(context, plan.to.get(), *plan.from, destination);
    return plan;
}

bool AliasCopy(const Graphics::Context& context, std::uint64_t destination, std::uint64_t source, std::size_t bytes) {
    if (CopyAliasDisabled()) return false;
    auto plan = PlanAlias(context, destination, source, bytes);
    if (plan.from == nullptr) {
        copyAliasNoSource.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (plan.refusal != nullptr) {
        if (plan.to != nullptr && !plan.to->SameSurfaceShape(*plan.from)) {
            copyAliasShape.fetch_add(1, std::memory_order_relaxed);
        } else {
            static std::atomic<int> reported{0};
            if (reported.fetch_add(1) < 8) std::fprintf(stderr, "[copy] image copy 0x%llx -> 0x%llx (0x%zx bytes) refused: %s\n", static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), bytes, plan.refusal);
            copyAliasRefused.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }
    if (plan.to == nullptr) {
        auto resource = plan.from->Descriptor();
        resource.baseAddress = destination;
        resource.dccAddress = 0;
        plan.to = Graphics::CachedStorageSurface(context, resource);
        copyAliasCreated.fetch_add(1, std::memory_order_relaxed);
    }
    const char* refusal = nullptr;
    if (!plan.to->CopyFrom(*plan.from, refusal)) throw std::runtime_error(std::string("AGC graphics: an image copy its check allowed was refused: ") + refusal);
    copiesAliased.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool AliasCopyPossible(const Graphics::Context& context, std::uint64_t destination, std::uint64_t source, std::size_t bytes) {
    return !CopyAliasDisabled() && PlanAlias(context, destination, source, bytes).refusal == nullptr;
}

}

VulkanDevice::CopyVerification VulkanDevice::CopyVerifyCounts() {
    return {copiesVerified.load(std::memory_order_relaxed), copySourceChanged.load(std::memory_order_relaxed), copyDestinationChanged.load(std::memory_order_relaxed), copyReaderMissed.load(std::memory_order_relaxed)};
}

namespace {
std::atomic<std::uint64_t> computePipelinesCreated{0};
std::atomic<std::uint64_t> computePipelinesCreatedLocked{0};
}

VulkanDevice::ComputePipelineCreation VulkanDevice::ComputePipelineCounts() {
    return {computePipelinesCreated.load(std::memory_order_relaxed), computePipelinesCreatedLocked.load(std::memory_order_relaxed)};
}

VulkanDevice::CopyAliasing VulkanDevice::CopyAliasCounts() {
    return {copiesAliased.load(std::memory_order_relaxed), copyAliasCreated.load(std::memory_order_relaxed), copyAliasNoSource.load(std::memory_order_relaxed), copyAliasShape.load(std::memory_order_relaxed), copyAliasRefused.load(std::memory_order_relaxed)};
}

bool VulkanDevice::completionStoresOver(std::uint64_t address, std::size_t bytes) const {
    static const bool alwaysSync = std::getenv("APS5_COPY_SYNC") != nullptr;
    if (alwaysSync || state->CopiedWriterOverlaps(address, bytes)) return true;
    return Graphics::Recorder::PendingCompletionLabels() != 0 && state->recorder->CompletionLabelIn(address, bytes);
}

bool VulkanDevice::copyWaitsOnCompletions(std::uint64_t address, std::size_t bytes) const {
    return state->recorder->PendingWriteOverlaps(address, bytes) && completionStoresOver(address, bytes);
}

bool VulkanDevice::GpuCopyPossible(std::uint64_t destination, std::uint64_t source, std::size_t bytes) const {
    if (!state->recorder || bytes == 0 || copyWaitsOnCompletions(source, bytes) || copyWaitsOnCompletions(destination, bytes)) return false;
    const auto context = graphicsContext();
    if (AliasCopyPossible(context, destination, source, bytes)) return true;
    return Graphics::HostImportFor(context, destination, bytes) != nullptr && Graphics::HostImportFor(context, source, bytes) != nullptr;
}

VulkanDevice::CopyOutcome VulkanDevice::CopyBuffer(std::uint64_t destination, std::uint64_t source, std::size_t bytes, std::size_t cpuMax, std::size_t gpuMax, std::size_t knownMax, std::uint64_t programAddress, std::uint32_t queue, const CopyWriterNote& noteWriter) {
    CopyOutcome outcome{2, false, 0, CopyOutcome::None, false, false, 0, 0, 0, 0, false};
    if (!state->recorder || bytes == 0) return outcome;
    auto& recorder = *state->recorder;
    // Finished batches were retired by the caller (Driver.cpp copyBuffer, on the graphics worker
    // only, as WriteLabelOnGpu reaps), so their writes neither refuse the CPU copy nor force a wait.
    static const bool waitForSource = std::getenv("APS5_COPY_WAIT_SOURCE") != nullptr;
    static const bool verify = std::getenv("APS5_COPY_VERIFY") != nullptr;
    // A store a batch's completion makes on the CPU over the range (a copied buffer's write-back,
    // a completion-deferred label): it lands only when that batch is reaped, whatever its fence.
    const auto completionStoreOverlaps = [&](std::uint64_t address) { return completionStoresOver(address, bytes); };
    // The CPU decision (see the header): pure queries, in the order that makes the first failing
    // test the reason; a signaled writer of the source is fine (its in-place stores are final in
    // host memory) as long as no completion store or label of it is still to land.
    bool settledBySignal = false;
    const auto refusal = [&]() -> int {
        settledBySignal = false;
        if (bytes > cpuMax) return CopyOutcome::Size;
        // Both lists: an image whose write-back is in progress on another thread (moved to flushing,
        // waiting for the GPU mutex) still stores over the range once this hold ends.
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> ranges{{{source, source + bytes}, {destination, destination + bytes}}};
        if (Graphics::StorageTexture::AnyPendingOverlaps(ranges)) return CopyOutcome::Image;
        // Results retiled into a unit shadow are not in the import's bytes: the GPU path publishes them.
        if (Graphics::AnyShadowedOverlaps(ranges)) return CopyOutcome::Shadow;
        if (recorder.PendingWriteOverlaps(source, bytes)) {
            if (!recorder.PendingWriteSettled(source, bytes)) return CopyOutcome::SourcePending;
            if (completionStoreOverlaps(source) || recorder.PendingLabelIn(source, bytes)) return CopyOutcome::SourceUnsettled;
            settledBySignal = true;
        }
        if (recorder.PendingWriteOverlaps(destination, bytes)) {
            // As for the source: a signaled writer's in-place stores are final and the memcpy lands
            // after them; only a completion store of it still to run refuses (compute queues never
            // reap, so a finished GPU copy into the same record stays in flight for a while).
            // Debug aid: APS5_COPY_DESTINATION_STRICT=1 refuses any pending writer, as before.
            static const bool strict = std::getenv("APS5_COPY_DESTINATION_STRICT") != nullptr;
            if (strict || !recorder.PendingWriteSettled(destination, bytes)) return CopyOutcome::DestinationPending;
            if (completionStoreOverlaps(destination)) return CopyOutcome::DestinationUnsettled;
        }
        if (recorder.PendingLabelIn(destination, bytes)) return CopyOutcome::Label;
        if (Graphics::Recorder::ReadTracking() ? recorder.PendingReadOverlaps(destination, bytes) : !recorder.Idle()) return CopyOutcome::Reader;
        return CopyOutcome::None;
    };
    outcome.reason = refusal();
    if (outcome.reason == CopyOutcome::SourcePending && waitForSource && queue != 0) {
        // The experiment: only when the producer is the oldest batch in flight (the wait finishes
        // nothing else), never on the frame-critical queue 0, unlocked (this hold is the outermost).
        if (const auto info = recorder.DescribePendingWrite(source, bytes); info.has_value() && !info->open && info->batchesToFinish == 1) {
            const auto waitStart = std::chrono::steady_clock::now();
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(source, bytes, true);
            outcome.waitedForSource = true;
            outcome.waitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waitStart).count();
            outcome.reason = refusal();
        }
    }
    if (outcome.reason == CopyOutcome::None) {
        // A plain CPU store, stamped for the write watch, with no pending write to note (nothing is
        // outstanding on the range) and nothing recorded.
        std::vector<std::byte> expected;
        if (verify) expected.assign(reinterpret_cast<const std::byte*>(source), reinterpret_cast<const std::byte*>(source) + bytes);
        GuestMemory::StoreOwnBytes(destination, bytes, [&] { std::memcpy(reinterpret_cast<void*>(destination), reinterpret_cast<const void*>(source), bytes); });
        outcome.path = 0;
        outcome.sourceSettledBySignal = settledBySignal;
        if (verify) {
            // The rule's reader answer without the fence shortcut: a reader still unsignaled now
            // would have been unsignaled at the decision a moment ago.
            if (const auto reader = recorder.DescribePendingRead(destination, bytes); reader.has_value() && !reader->signaled) copyReaderMissed.fetch_add(1, std::memory_order_relaxed);
            Graphics::Recorder::CountSync(4);
            recorder.Sync();
            copiesVerified.fetch_add(1, std::memory_order_relaxed);
            if (GuestMemory::Accessible(reinterpret_cast<const void*>(source), bytes) && std::memcmp(expected.data(), reinterpret_cast<const void*>(source), bytes) != 0) copySourceChanged.fetch_add(1, std::memory_order_relaxed);
            if (GuestMemory::Accessible(reinterpret_cast<const void*>(destination), bytes) && std::memcmp(expected.data(), reinterpret_cast<const void*>(destination), bytes) != 0) copyDestinationChanged.fetch_add(1, std::memory_order_relaxed);
        }
        return outcome;
    }
    if (outcome.reason == CopyOutcome::Reader) {
        if (const auto reader = recorder.DescribePendingRead(destination, bytes)) {
            outcome.readerSerial = reader->serial;
            outcome.readerQueue = reader->queue;
            outcome.readerKind = static_cast<int>(reader->kind);
            outcome.readerOpen = reader->open;
        }
    }
    const auto context = graphicsContext();
    // The ordering decision of FillBuffer, for both ranges: GPU stores recorded earlier are ordered
    // before the transfer (or the device copy below) by its barrier; a store a batch's completion
    // makes on the CPU must land before the copy reads the source or writes the destination, so
    // the copy waits for that batch (SyncThrough) first.
    const bool waitSource = copyWaitsOnCompletions(source, bytes);
    const bool waitDestination = copyWaitsOnCompletions(destination, bytes);
    if (waitSource || waitDestination) {
        outcome.synced = true;
        const auto syncStart = std::chrono::steady_clock::now();
        if (waitSource) {
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(source, bytes);
        }
        if (waitDestination) {
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(destination, bytes);
        }
        outcome.syncMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - syncStart).count();
    }
    if (AliasCopy(context, destination, source, bytes)) {
        noteWriter({}, 0);
        recorder.NotePendingWrite(destination, bytes);
        Graphics::StorageTexture::FlushPending(destination, bytes, nullptr, "buffer copy destination", Graphics::PublishScope::Whole);
        outcome.path = 3;
        return outcome;
    }
    if (bytes > gpuMax) {
        outcome.path = 8;
        return outcome;
    }
    // The GPU path. Results still on the GPU: the source's must be stored before the transfer reads
    // it, the destination's would be stored over the copy later (a flush into an import records
    // the retile and notes it as a pending write, which is why it comes after the CPU decision).
    Graphics::StorageTexture::FlushPending(source, bytes, nullptr, "buffer copy source", Graphics::PublishScope::Whole);
    Graphics::StorageTexture::FlushPending(destination, bytes, nullptr, "buffer copy destination", Graphics::PublishScope::PartialUnits);
    // Looked up after the sync (its completions' write-backs can refresh the import table and
    // retire an import) and the flushes (which may import memory themselves).
    const Graphics::HostImport* destinationImport = Graphics::HostImportFor(context, destination, bytes);
    const Graphics::HostImport* sourceImport = destinationImport != nullptr ? Graphics::HostImportFor(context, source, bytes) : nullptr;
    if (destinationImport == nullptr || sourceImport == nullptr) return outcome;
    using CommandClass = Graphics::Recorder::CommandClass;
    // A queued DCC key store over either range must land before the transfer reads or writes it.
    recorder.FlushKeyStoresOverlapping(source, bytes);
    recorder.FlushKeyStoresOverlapping(destination, bytes);
    // A queued label store in either range lands before the transfer reads or writes it.
    recorder.FlushStoresOverlapping(source, bytes);
    recorder.FlushStoresOverlapping(destination, bytes);
    VkAccessFlags covered = 0;
    const auto commands = recorder.Commands(&covered);
    // The class range covers the barriers too; the program-keyed range inside it is the transfer.
    const auto classTiming = recorder.BeginGpuTiming(CommandClass::Copy);
    if (Graphics::Recorder::BarrierValidate()) {
        const std::pair<std::uint64_t, std::uint64_t> read{source, source + bytes}, written{destination, destination + bytes};
        recorder.NoteAccess(CommandClass::Copy, Graphics::Recorder::Access{std::span(&read, 1), std::span(&written, 1), {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    // Every earlier recorded read or write of either range precedes the transfer, whichever stage
    // made it, and host stores (the CPU copies above, the game's) are visible to it; the previous
    // command's trailing barrier already did that when it covered transfer reads and writes.
    constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    if ((covered & transferAccess) == transferAccess && Graphics::Recorder::MergeBarriers()) {
        Graphics::Recorder::CountMerged(CommandClass::Copy);
    } else {
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, transferAccess);
        Graphics::Recorder::CountBarriers(CommandClass::Copy);
    }
    const auto gpuTiming = recorder.BeginGpuTiming(programAddress);
    const VkBufferCopy region{Graphics::HostImportOffset(*sourceImport, source), Graphics::HostImportOffset(*destinationImport, destination), bytes};
    context.Resolved(&Graphics::DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer")(commands, sourceImport->buffer, destinationImport->buffer, 1, &region);
    recorder.EndGpuTiming(gpuTiming, bytes);
    // The copied bytes are visible to everything recorded after and to the host, as after a fill.
    constexpr VkAccessFlags copiedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, copiedAccess);
    Graphics::Recorder::CountBarriers(CommandClass::Copy);
    recorder.EndGpuTiming(classTiming, bytes);
    recorder.MarkCovered(copiedAccess);
    recorder.NotePendingRead(source, bytes, Graphics::Recorder::ReadKind::CopySource);
    GuestMemory::MarkWritten(destination, bytes);
    recorder.NotePendingWrite(destination, bytes);
    // The destination's bytes after the transfer are the source's now when nothing recorded,
    // pending or still to land can change the source before the transfer reads it (a CPU store
    // to it in between is a hardware race too): the caller's ring entry carries them, stamped
    // with the generation this store gets, and goes in ahead of the note.
    const auto generation = GuestMemory::MarkWritten(destination, bytes);
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> sourceRange{{{source, source + bytes}}};
    const bool sourceCurrent = bytes <= knownMax && generation != 0 && !recorder.PendingWriteOverlaps(source, bytes) && !recorder.PendingLabelIn(source, bytes) && !completionStoreOverlaps(source) && !Graphics::StorageTexture::AnyPendingOverlaps(sourceRange) && !Graphics::AnyShadowedOverlaps(sourceRange);
    noteWriter(sourceCurrent ? std::span(reinterpret_cast<const std::byte*>(source), bytes) : std::span<const std::byte>{}, sourceCurrent ? generation : 0);
    recorder.NotePendingWrite(destination, bytes);
    outcome.path = 1;
    return outcome;
}

void* VulkanDevice::Window() const {
    return state->window;
}

void VulkanDevice::Resize(std::uint32_t width, std::uint32_t height) {
    APS5_LOG_OUT_DEBUG("Resize requested=%ux%u current=%ux%u", width, height, state->extent.width, state->extent.height);
    require(state->swapchain != VK_NULL_HANDLE, "cannot resize an unavailable swapchain");
    if (width == 0 || height == 0) {
        state->extent = {0, 0};
        return;
    }
    if (state->extent.width == width && state->extent.height == height) return;
    recreateSwapchain(width, height, false);
}

void VulkanDevice::recreateSwapchain(std::uint32_t width, std::uint32_t height, bool force) {
    VkSurfaceCapabilitiesKHR surface{};
    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(state->physical, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR resize");
    // The window's drawable size and the surface's extent can disagree for a moment while the window
    // is being resized; the swapchain must match the surface, and the next present catches up.
    if (surface.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        width = surface.currentExtent.width;
        height = surface.currentExtent.height;
        if (width == 0 || height == 0) {
            WaitIdle();
            state->extent = {0, 0};
            return;
        }
        if (!force && state->extent.width == width && state->extent.height == height) return;
    }
    WaitIdle();
    refreshSurfaceFormats();
    if (state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
        const bool offered = std::any_of(state->surfaceFormats.begin(), state->surfaceFormats.end(), [&](const VkSurfaceFormatKHR& format) { return format.format == state->surfaceFormat && format.colorSpace == state->surfaceColorSpace; });
        if (!offered || !AgcDriver::CurrentHostHdr().available) useSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    }
    require(width >= surface.minImageExtent.width && width <= surface.maxImageExtent.width && height >= surface.minImageExtent.height && height <= surface.maxImageExtent.height, "unsupported resized output extent");
    require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 && (surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0 && (surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "resized surface capabilities are unsupported");
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = state->surface;
    create.minImageCount = AgcDriver::SwapchainImageCount(surface);
    create.imageFormat = state->surfaceFormat;
    create.imageColorSpace = state->surfaceColorSpace;
    create.imageExtent = {width, height};
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = state->presentMode;
    create.oldSwapchain = state->swapchain;
    state->retiredSwapchains.reserve(state->retiredSwapchains.size() + 1);
    VkSwapchainKHR replacement = VK_NULL_HANDLE;
    check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &create, nullptr, &replacement), "vkCreateSwapchainKHR resize");
    state->retiredSwapchains.push_back({state->swapchain, std::move(state->rendered)});
    state->swapchain = replacement;
    state->extent = create.imageExtent;
    auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
    std::uint32_t count = 0;
    check(getImages(state->device, replacement, &count, nullptr), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    check(getImages(state->device, replacement, &count, state->images.data()), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    state->rendered.assign(count, VK_NULL_HANDLE);
    if (state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) setHdrMetadata();
    APS5_LOG_OUT_DEBUG("Resize complete swapchain=%p extent=%ux%u images=%u", reinterpret_cast<void*>(state->swapchain), state->extent.width, state->extent.height, count);
}

void VulkanDevice::refreshSurfaceFormats() {
    const auto getFormats = state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR");
    const auto displayGeneration = AgcDriver::CurrentHostHdr().displayGeneration;
    std::uint32_t count = 0;
    check(getFormats(state->physical, state->surface, &count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::vector<VkSurfaceFormatKHR> formats(count);
    check(getFormats(state->physical, state->surface, &count, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    formats.resize(count);
    constexpr VkFormatFeatureFlags target = VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    std::erase_if(formats, [&](const VkSurfaceFormatKHR& format) {
        if (format.colorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT) return false;
        VkFormatProperties properties{};
        graphicsContext().formatProperties(state->physical, format.format, &properties);
        return (properties.optimalTilingFeatures & target) != target;
    });
    state->surfaceFormats = std::move(formats);
    state->surfaceDisplayGeneration = displayGeneration;
    constexpr VkFormatFeatureFlags scalerSource = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    const std::array<VkFormat, 2> pqTexels{VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32};
    const bool sources = std::all_of(pqTexels.begin(), pqTexels.end(), [&](VkFormat format) {
        VkFormatProperties properties{};
        graphicsContext().formatProperties(state->physical, format, &properties);
        return (properties.optimalTilingFeatures & scalerSource) == scalerSource;
    });
    AgcDriverSetSurfaceHdr10_nid_postfix(state->swapchainColorSpaces && state->hdrMetadata && sources && Hdr10SurfaceFormat(state->surfaceFormats, VK_FORMAT_UNDEFINED).has_value());
}

void VulkanDevice::setHdrMetadata() {
    const auto host = AgcDriver::CurrentHostHdr();
    state->metadataGeneration = host.generation;
    require(host.available && state->hdrMetadata && state->surfaceColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT && state->swapchain != VK_NULL_HANDLE, "HDR metadata needs HDR output, an HDR10 swapchain and VK_EXT_hdr_metadata");
    VkHdrMetadataEXT metadata{VK_STRUCTURE_TYPE_HDR_METADATA_EXT};
    metadata.displayPrimaryRed = {0.708f, 0.292f};
    metadata.displayPrimaryGreen = {0.170f, 0.797f};
    metadata.displayPrimaryBlue = {0.131f, 0.046f};
    metadata.whitePoint = {0.3127f, 0.3290f};
    metadata.maxLuminance = host.display.maxLuminance;
    metadata.minLuminance = host.display.minLuminance;
    metadata.maxContentLightLevel = PqToSdrWhiteNits(host);
    metadata.maxFrameAverageLightLevel = std::min(host.display.maxFullFrameLuminance, metadata.maxContentLightLevel);
    state->DeviceFunction<PFN_vkSetHdrMetadataEXT>("vkSetHdrMetadataEXT")(state->device, 1, &state->swapchain, &metadata);
    std::fprintf(stderr, "[videoout] HDR metadata: mastering %.1f to %.4f nits, MaxCLL %.1f nits, MaxFALL %.1f nits\n", metadata.maxLuminance, metadata.minLuminance, metadata.maxContentLightLevel, metadata.maxFrameAverageLightLevel);
}

}
}
}
