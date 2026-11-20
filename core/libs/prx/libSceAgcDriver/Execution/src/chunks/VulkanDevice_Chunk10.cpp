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
VulkanDevice::IndirectOutcome VulkanDevice::recordCompletedDispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::uint64_t programAddress, std::shared_ptr<const Recipe>* recipeOut, const std::array<std::uint32_t, 3>& threadGroup, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear, DispatchCompletion& completion) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto& context = completion.context;
    const auto& shaders = completion.shaders;
    const auto pushStages = completion.pushStages;
    const auto& pushBytes = completion.pushBytes;
    const auto& objects = completion.objects;
    const auto& resources = completion.resources;
    const auto& contentKey = completion.contentKey;
    const bool cacheable = completion.cacheable;
    const bool fromCache = completion.fromCache;
    const double revalidateMs = completion.revalidateMs;
    const auto& overlay = completion.overlay;
    const auto lookupsBefore = completion.lookupsBefore;
    auto& timer = completion.timer;
    auto& timing = completion.timing;
    auto* groupsText = completion.groupsText;
    // Recorded into the device's open batch: the CPU moves on to the next command while the GPU
    // works; the batch is waited for where the guest expects results (labels, flips, CPU reads).
    auto& recorder = *state->recorder;
    IndirectOutcome outcome{0, 0};
    RecordedDispatch record{&context, &shaders[0], resources, objects, pushStages, &pushBytes, x, y, z, arguments, nullptr, programAddress, fromCache && TemplateDataRefresh() ? RecordedDispatch::DataRefresh::Words : RecordedDispatch::DataRefresh::None, 0, revalidateMs, &timer, htileClear};
    record.resourceKeyMatched = fromCache;
    record.bindings = overlay;
    record.threadGroup = threadGroup;
    if (arguments != 0) decideIndirect(record, outcome, groupsText);
    // The indirect hold total below still covers the whole call of a CPU-resolved one.
    const bool indirect = record.arguments != 0 || outcome.cpuReason != 0;
    recordDispatch(record);
    timing.Mark("command_record");
    timer.phase(PhaseRecord);
    APS5_LOG_OUT_DEBUG("Dispatch recorded groups=%s", groupsText);
    // Address-based shaders pin guest allocations until their write-back, which the completion runs
    // when the batch finished (deferred lease release, see Graphics::SyncLeaseWork): a guest thread
    // that needs a leased allocation syncs the recorder itself through the registry's pin waiter.
    // APS5_SYNC_LEASE_DISPATCH=1 completes such dispatches at once, as before.
    if (SyncEachDispatch() || (resources->HoldsLease() && Graphics::SyncLeaseWork())) {
        Graphics::Recorder::CountSync(3);
        const auto syncStart = std::chrono::steady_clock::now();
        recorder.Sync();
        timer.phase(PhaseSync);
        if (resources->HoldsLease()) Graphics::CountLeaseOutcome(true, 0, !resources->BdaReadOnly());
        if (profile) {
            // APS5_PROFILE_DRAW: the [recorder] line's "address-based" syncs by program, every 10 s,
            // so the programs still waiting here are known (under the GpuMutex, like the rest).
            struct Waits { std::uint64_t count = 0; double ms = 0; };
            static std::map<std::uint64_t, Waits> byProgram;
            static std::uint64_t syncs = 0;
            static double waitedMs = 0;
            static auto lastReport = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(now - syncStart).count();
            auto& waits = byProgram[programAddress != 0 ? programAddress : shader.PipelineVariantId()];
            ++waits.count;
            waits.ms += ms;
            ++syncs;
            waitedMs += ms;
            if (now - lastReport > std::chrono::seconds(10)) {
                lastReport = now;
                std::vector<std::pair<std::uint64_t, Waits>> hot(byProgram.begin(), byProgram.end());
                std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
                AgcDriver::ProfilePrint_nid_no_patch("[address-sync] %llu address-based dispatch syncs waited %.1f s in total; by program (10 s):", static_cast<unsigned long long>(syncs), waitedMs / 1000);
                for (std::size_t i = 0; i < hot.size() && i < 8; ++i) AgcDriver::ProfilePrint_nid_no_patch(" 0x%llx x%llu %.0fms", static_cast<unsigned long long>(hot[i].first), static_cast<unsigned long long>(hot[i].second.count), hot[i].second.ms);
                AgcDriver::ProfilePrint_nid_no_patch("\n");
                byProgram.clear();
            }
        }
    } else if (resources->HoldsLease()) {
        // The lease is in the open batch (kept above, nothing submitted since): the pin waiter
        // finishes the recorder up to that batch's serial. Prints the [address-sync] leases line.
        Graphics::CountLeaseOutcome(false, recorder.Submissions() + 1, !resources->BdaReadOnly());
    }
    if (TraceDispatchIo()) std::fprintf(stderr, "[dispatch-io] %s:%s\n", groupsText, resources->Describe().c_str());
    WatchMemory(programAddress);
    // The recipe for the caller's dispatch-cache variant (design_cpu_final M4, rule R3): only an
    // object the resource cache serves under this content key (reusable: no lease, no copied
    // writes, every direct region import- or mirror-served), so a hit's proof is the template's
    // Revalidate and nothing needs completion work.
    if (recipeOut != nullptr && DispatchRecipes() && cacheable && overlay == nullptr && resources->Reusable() && !resources->NeedsCompletion() && objects != nullptr && !contentKey.empty()) {
        auto recipe = std::make_shared<Recipe>();
        recipe->device = state->device;
        recipe->templateRef = resources;
        recipe->key = contentKey;
        recipe->objects = objects;
        recipe->pushBytes = pushBytes;
        recipe->pushes = pushStages != 0;
        recipe->dataWordsHash = Graphics::ShaderResources::DataWordsHash(shaders[0]);
        if (!NoPresync() && CanWaitUnlocked()) {
            recipe->presyncSurfaces = resources->PresyncSurfaces();
            recipe->presyncProof = Graphics::HostImportsIdentity(context);
        }
        recipe->needsCompletion = resources->NeedsCompletion();
        recipe->holdsLease = resources->HoldsLease();
        *recipeOut = std::move(recipe);
    }
    timer.finish(lookupsBefore, groupsText, indirect);
    return outcome;
}

RecipeOutcome VulkanDevice::DispatchRecipe(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::uint64_t programAddress, const std::shared_ptr<RecipeHit>& hit, IndirectOutcome& outcome, const std::shared_ptr<PreparedDispatch>& verify, bool refreshByWords, const std::array<std::uint32_t, 3>& threadGroup) {
    state->recorder->BoundKeptBytes();
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    PerformanceTimer timing("Vulkan.DispatchRecipe");
    outcome = {0, 0};
    char groupsText[40];
    if (arguments != 0) std::snprintf(groupsText, sizeof(groupsText), "indirect");
    else std::snprintf(groupsText, sizeof(groupsText), "%ux%ux%u", x, y, z);
    const auto& recipe = *hit->recipe;
    auto& counters = Recipes().kinds[hit->indirect ? 1 : 0];
    auto& d = Dispatches();
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto context = graphicsContext();
    if (recipe.needsCompletion || recipe.holdsLease || !hit->resources->Reusable()) throw std::runtime_error("Vulkan dispatch: recipe over a non-reusable template");
    // Without the refresh the template holds the words it was keyed with: a data-only hit takes
    // the ordinary path, whose word-keyed lookup serves or builds the object for the live words.
    if (refreshByWords && !TemplateDataRefresh()) return RecipeOutcome::Rebuild;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    DispatchTimer timer(profile, arguments != 0, shader.spirv.size(), programAddress);
    DispatchTimer::countDispatch(profile);
    const auto lookupsBefore = Graphics::DeviceProcLookups();
    // R10: the recipe names the device it was built on (the pre-check read it too, but the driver
    // may replace the device between the two).
    if (recipe.device != state->device) {
        counters.rebuildDevice.fetch_add(1, std::memory_order_relaxed);
        return RecipeOutcome::Rebuild;
    }
    if (hit->presyncSerial != 0) {
        timer.restart();
        ReapRecorded(hit->presyncSerial);
        timer.phase(PhaseReap);
    }
    if (verify != nullptr) {
        // APS5_VERIFY_RECIPE=1: the ordinary path's answers beside the recipe's. The found object
        // is the cache's under the content key (PrepareDispatch's Find), the pipeline objects the
        // map's, and the data refresh decision the per-word compare RefreshData makes; the proof
        // itself is the same Revalidate on the same object, so it needs no second run.
        // A found object that is not the recipe's template is no disagreement: the cache no longer
        // serves the template under its key (removed by a sibling variant's failed proof, evicted,
        // replaced by another worker's Insert) while the batch keeps it alive, so the ordinary path
        // would use a content-equal object and the comparison has no counterpart. Counted, and the
        // ordinary path runs with the prepared find (C10: the template goes to the batch).
        if (verify->cached.get() != hit->resources.get()) {
            counters.verifyReplaced.fetch_add(1, std::memory_order_relaxed);
            hit->resources->KeepStorageImageBytes(*state->recorder);
            state->recorder->Keep(std::move(hit->resources));
            return RecipeOutcome::Rebuild;
        }
        std::shared_ptr<ComputePipelineObjects> mapped;
        const auto pipelineKey = ComputePipelineKey::Of(shader, recipe.pushes);
        {
            std::lock_guard pipelines(state->computePipelinesMutex);
            if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) mapped = found->second;
        }
        // A data-only hit's words are the shader's, not the recipe's: its hash decision is
        // computed from the shader, as the record below decides by words.
        const bool byHash = TemplateDataRefresh() && hit->resources->DataWordsHash() != (refreshByWords ? Graphics::ShaderResources::DataWordsHash(shaders[0]) : recipe.dataWordsHash);
        const bool byWords = TemplateDataRefresh() && hit->resources->DataWordsDiffer(shaders[0]);
        const char* disagreement = mapped != hit->objects ? "the pipeline objects" : byHash != byWords ? "the RefreshData decision" : nullptr;
        if (disagreement != nullptr) {
            std::fprintf(stderr, "[recipe] APS5_VERIFY_RECIPE: %s of the recipe disagrees with the ordinary path (program 0x%llx)\n", disagreement, static_cast<unsigned long long>(programAddress));
            std::fflush(stderr);
            std::abort();
        }
        counters.verified.fetch_add(1, std::memory_order_relaxed);
    }
    // The proof (rules R6/R7): the template's Revalidate, T1 included; a failure removes the
    // template from the cache (the batch keeps it) and the caller rebuilds.
    const auto waitedBefore = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    const auto proofStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    Graphics::ShaderResources::ProofReport report;
    const bool proved = hit->resources->ProveCurrent(shaders[0], &report);
    if (profile) {
        counters.proofNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - proofStart).count()), std::memory_order_relaxed);
        timer.add(PhaseResourcesHookWaits, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    }
    timer.phase(PhaseProof);
    if (!proved) {
        state->resourceCache.Remove(recipe.key, hit->resources.get());
        ++d.cacheInvalidated;
        // Off the mutex with its batch, as an evicted entry.
        hit->resources->KeepStorageImageBytes(*state->recorder);
        state->recorder->Keep(std::move(hit->resources));
        counters.rebuilds[static_cast<std::size_t>(report.failure)].fetch_add(1, std::memory_order_relaxed);
        return RecipeOutcome::Rebuild;
    }
    counters.proofPaths[static_cast<std::size_t>(report.path)].fetch_add(1, std::memory_order_relaxed);
    ++d.cacheHits;
    state->resourceCache.Touch(recipe.key);
    // The recipe's hash names the words it was attached with, not a data-only hit's: that hit
    // refreshes by the per-word compare (RefreshData keeps the template's own hash exact, so a
    // later exact hit of a sibling variant compares correctly against it).
    const auto dataRefresh = !TemplateDataRefresh() ? RecordedDispatch::DataRefresh::None : refreshByWords ? RecordedDispatch::DataRefresh::Words : RecordedDispatch::DataRefresh::Hash;
    if (dataRefresh == RecordedDispatch::DataRefresh::Words) counters.refreshByWords.fetch_add(1, std::memory_order_relaxed);
    RecordedDispatch record{&context, &shaders[0], hit->resources, hit->objects, recipe.pushes ? VkShaderStageFlags{VK_SHADER_STAGE_COMPUTE_BIT} : VkShaderStageFlags{0}, &recipe.pushBytes, x, y, z, arguments, nullptr, programAddress, dataRefresh, recipe.dataWordsHash, 0, &timer};
    record.resourceKeyMatched = verify != nullptr;
    record.threadGroup = threadGroup;
    if (arguments != 0) decideIndirect(record, outcome, groupsText);
    const bool indirect = record.arguments != 0 || outcome.cpuReason != 0;
    const auto recordStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    recordDispatch(record);
    if (profile) counters.recordNs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - recordStart).count()), std::memory_order_relaxed);
    timing.Mark("command_record");
    timer.phase(PhaseRecord);
    counters.hits.fetch_add(1, std::memory_order_relaxed);
    if (record.dataRefresh != RecordedDispatch::DataRefresh::None) (record.refreshed ? counters.dataRefreshed : counters.dataSkipped).fetch_add(1, std::memory_order_relaxed);
    APS5_LOG_OUT_DEBUG("Dispatch recorded from recipe groups=%s", groupsText);
    if (SyncEachDispatch()) {
        Graphics::Recorder::CountSync(3);
        state->recorder->Sync();
        timer.phase(PhaseSync);
    }
    if (TraceDispatchIo()) std::fprintf(stderr, "[dispatch-io] %s:%s\n", groupsText, hit->resources->Describe().c_str());
    WatchMemory(programAddress);
    timer.finish(lookupsBefore, groupsText, indirect);
    if (profile) reportRecipes();
    return RecipeOutcome::Recorded;
}

}
}
}
}
}
}
}
}
