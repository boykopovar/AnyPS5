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
std::vector<std::uint32_t> VulkanDevice::DrawContentKey(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders) const {
    if (!state->contextReady) return {};
    return Graphics::DrawResourceContentKey(state->context, graphics, draw, shaders);
}

void VulkanDevice::Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::shared_ptr<const DrawRecipe>* recipe, const DrawTraceContext* trace, std::uint64_t shape, const std::vector<std::uint32_t>* contentKey) {
    startJournalReplay();
    for (const auto& shader : shaders) {
        require(shader.program != nullptr, "missing compiled shader");
        ShaderRecompiler::RuntimeAbi::RequireVersion(shader.program->runtimeAbiVersion);
    }
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    if (traceDraws) APS5_LOG_OUT("VulkanDevice::Draw indices=%u instances=%u indexSize=%u address=0x%llx shaders=%zu colorTarget=%u", draw.indexCount, draw.instanceCount, draw.indexSize, static_cast<unsigned long long>(draw.indexAddress), shaders.size(), static_cast<unsigned>(graphics.hasColorTarget));
    const auto context = graphicsContext();
    Graphics::Draw(context, graphics, draw, shaders, snapshots, recipe, trace, shape, contentKey);
    if (traceDraws) APS5_LOG_CHARS_OUT("VulkanDevice::Draw complete");
}

bool VulkanDevice::DrawInputsInPlace() const {
    return state->drawInputsInPlace;
}

namespace {

bool ResourceCacheEnabled() {
    static const bool noResourceCache = std::getenv("APS5_NO_RESOURCE_CACHE") != nullptr;
    static const bool noTextureCache = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    return !noResourceCache && !noTextureCache;
}

bool TemplateDataRefresh() {
    static const bool enabled = std::getenv("APS5_NO_TEMPLATE_DATA_REFRESH") == nullptr;
    return enabled;
}

bool DispatchMove() {
    static const bool enabled = std::getenv("APS5_NO_DISPATCH_MOVE") == nullptr && TemplateDataRefresh();
    return enabled;
}

std::size_t DispatchMoveBytes() {
    static const std::size_t bytes = [] {
        const char* text = std::getenv("APS5_DISPMOVE_MAX_KIB");
        return static_cast<std::size_t>(text != nullptr ? std::strtoull(text, nullptr, 10) : 1024ull) * 1024u;
    }();
    return bytes;
}

ResourceCache::Key DispatchContentKey(const Graphics::CompiledShader& shader, VkDevice device, bool movable = true) {
    auto key = Graphics::ShaderResources::ContentKey(shader, !TemplateDataRefresh(), movable && DispatchMove());
    const auto deviceHandle = reinterpret_cast<std::uint64_t>(device);
    key.push_back(static_cast<std::uint32_t>(deviceHandle));
    key.push_back(static_cast<std::uint32_t>(deviceHandle >> 32u));
    return key;
}

enum DispatchPhase : std::size_t { PhaseReap, PhaseResources, PhaseResourcesInsert, PhaseResourcesComplete, PhaseResourcesImages, PhaseResourcesUpload, PhaseResourcesDescriptors, PhaseResourcesOther, PhaseResourcesALocked, PhaseResourcesAddress, PhaseResourcesAUnlocked, PhaseResourcesRevalidate, PhaseResourcesFullBuild, PhaseResourcesHookWaits, PhaseProof, PhasePipeline, PhaseDecide, PhaseArgumentRead, PhaseRecordCommands, PhaseRecordKeeps, PhaseRecordDataRefresh, PhaseRecordBind, PhaseRecordMarks, PhaseRecordCompletion, PhaseRecord, PhaseSync, DispatchPhaseCount };
constexpr std::array<const char*, DispatchPhaseCount> DispatchPhaseNames{"reap", "resources", "resources: cache insert", "resources: complete (wall)", "resources: B images", "resources: B upload", "resources: B descriptors", "resources: B bda+other", "resources: A locked (bda)", "resources: address bindings (bda)", "resources A (unlocked)", "resources: revalidate", "resources: full build (locked)", "resources: hook waits", "proof", "pipeline", "decide", "argument read", "record: commands", "record: keeps", "record: data refresh", "record: bind+dispatch", "record: marks", "record: completion", "record", "sync"};
constexpr std::size_t DispatchRows = static_cast<std::size_t>(DispatchPhaseCount) + static_cast<std::size_t>(Graphics::LookupOutcomes::Count);

const char* DispatchRowName(std::size_t row) {
    if (row < DispatchPhaseCount) return DispatchPhaseNames[row];
    static const std::array<std::string, Graphics::LookupOutcomes::Count> imageRows = [] {
        std::array<std::string, Graphics::LookupOutcomes::Count> rows;
        for (std::size_t kind = 0; kind < Graphics::LookupOutcomes::Count; ++kind) rows[kind] = std::string("images: ") + Graphics::LookupOutcomes::Name(static_cast<Graphics::LookupOutcomes::Kind>(kind));
        return rows;
    }();
    return imageRows[row - DispatchPhaseCount].c_str();
}

}

struct PreparedDispatch {
    std::shared_ptr<Graphics::ShaderResources> resources;
    std::shared_ptr<Graphics::ShaderResources> cached;
    ResourceCache::Key key;
    bool exact = false;
    double prepareMs = 0;
    std::uint64_t dispatchThreads = 0;
    enum PreparePhase : std::size_t { PrepareKey, PrepareFind, PreparePrecollect, PreparePresync, PrepareStageA, PreparePhaseCount };
    std::array<double, PreparePhaseCount> phaseMs{};
    std::uint64_t presyncSerial = 0;
    std::shared_ptr<ComputePipelineObjects> objects;
    std::vector<std::uint32_t> describedLayoutKey;
};

struct RecipeHit {
    std::shared_ptr<const Recipe> recipe;
    std::shared_ptr<Graphics::ShaderResources> resources;
    std::shared_ptr<ComputePipelineObjects> objects;
    std::uint64_t presyncSerial = 0;
    bool indirect = false;
    double precheckMs = 0;
};

namespace {

struct PresyncCounters {
    std::atomic<std::uint64_t> checked{0};
    std::atomic<std::uint64_t> presyncs{0};
    std::atomic<std::uint64_t> waitedUs{0};
    std::atomic<std::uint64_t> keyChecks{0};
    std::atomic<std::uint64_t> keyWaits{0};
    std::atomic<std::int64_t> lastReport{0};
};

PresyncCounters& Presyncs() {
    static PresyncCounters counters;
    return counters;
}

bool CachedPrecollect() {
    static const bool cachedPrecollect = std::getenv("APS5_NO_CACHED_PRECOLLECT") == nullptr;
    return cachedPrecollect;
}

bool NoPresync() {
    static const bool noPresync = std::getenv("APS5_NO_PRESYNC") != nullptr;
    return noPresync;
}

bool SyncEachDispatch() {
    static const bool syncEachDispatch = std::getenv("APS5_SYNC_DISPATCH") != nullptr;
    return syncEachDispatch;
}

bool TraceDispatchIo() {
    static const bool traceIo = std::getenv("APS5_TRACE_DISPATCH_IO") != nullptr;
    return traceIo;
}

void WatchMemory(std::uint64_t programAddress) {
    struct Range {
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
    };
    static const std::vector<Range> ranges = [] {
        std::vector<Range> parsed;
        const char* text = std::getenv("APS5_WATCH_MEMORY");
        if (text == nullptr) return parsed;
        while (true) {
            char* end = nullptr;
            const auto address = std::strtoull(text, &end, 16);
            const auto bytes = *end == ':' ? std::strtoull(end + 1, &end, 16) : 0ull;
            if (address == 0 || bytes == 0 || (*end != ',' && *end != '\0')) throw std::runtime_error("APS5_WATCH_MEMORY takes <hex address>:<hex bytes>[,...]");
            parsed.push_back({address, bytes});
            if (*end == '\0') break;
            text = end + 1;
        }
        if (!SyncEachDispatch()) throw std::runtime_error("APS5_WATCH_MEMORY needs APS5_SYNC_DISPATCH=1");
        return parsed;
    }();
    if (ranges.empty()) return;
    static AgcDriver::Mutex mutex;
    static std::deque<std::pair<std::uint64_t, std::vector<std::uint8_t>>> history;
    static bool written = false;
    std::lock_guard lock(mutex);
    if (written) return;
    static std::vector<std::uint64_t> addresses;
    if (addresses.empty()) {
        std::vector<std::uint64_t> found;
        for (const auto& range : ranges) {
            std::uint64_t address = 0;
            for (std::uint64_t high = 0; high < 16 && address == 0; ++high) {
                const auto candidate = (range.address & 0xfffffffffull) | (high << 36u);
                if (GuestMemory::Accessible(reinterpret_cast<const void*>(candidate), static_cast<std::size_t>(range.bytes))) address = candidate;
            }
            if (address == 0) return;
            found.push_back(address);
        }
        addresses = std::move(found);
        for (std::size_t i = 0; i < ranges.size(); ++i) std::fprintf(stderr, "[watch] watching 0x%llx+0x%llx\n", static_cast<unsigned long long>(addresses[i]), static_cast<unsigned long long>(ranges[i].bytes));
    }
    std::vector<std::uint8_t> contents;
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(addresses[i]);
        if (!GuestMemory::Accessible(bytes, static_cast<std::size_t>(ranges[i].bytes))) return;
        contents.insert(contents.end(), bytes, bytes + ranges[i].bytes);
    }
    if (history.empty() || history.back().second != contents) {
        history.emplace_back(programAddress, std::move(contents));
        if (history.size() > 64) history.pop_front();
    }
    if (!Graphics::LoopGuardTripped()) return;
    written = true;
    for (std::size_t i = 0; i < history.size(); ++i) {
        char name[64];
        std::snprintf(name, sizeof(name), "watch_%02zu_%llx.bin", i, static_cast<unsigned long long>(history[i].first));
        std::ofstream(name, std::ios::binary).write(reinterpret_cast<const char*>(history[i].second.data()), static_cast<std::streamsize>(history[i].second.size()));
    }
    std::fprintf(stderr, "[watch] loop guard tripped: %zu contents written (watch_*.bin, oldest first)\n", history.size());
}

struct IndirectHold {
    std::array<double, DispatchRows> phaseMs{};
    std::array<std::uint64_t, DispatchRows> phaseCounts{};
    std::uint64_t count = 0;
    double totalMs = 0;
    double maxMs = 0;
    std::string maxPhases;
    std::uint64_t writersScanned = 0;
    std::uint64_t drawWritersScanned = 0;
    std::size_t maxWriters = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

struct DispatchCounters {
    std::array<double, DispatchRows> phaseTotals{};
    std::uint64_t profiledDispatches = 0;
    IndirectHold indirectHold;
    std::uint64_t cacheHits = 0, cacheMisses = 0, cacheInvalidated = 0, templateRefreshed = 0, templateSameWords = 0, templateByNumber = 0;
    double templateRevalidateMs = 0;
    std::uint64_t procLookups = 0, preBarriersRecorded = 0, preBarriersSkipped = 0;
    std::chrono::steady_clock::time_point cacheReport = std::chrono::steady_clock::now();
};

DispatchCounters& Dispatches() {
    static DispatchCounters counters;
    return counters;
}

enum RecipeMiss : std::size_t { MissNoRecipe, MissDevice, MissTemplateGone, MissObjectsGone, MissNotRecordable, RecipeMissCount };
constexpr std::array<const char*, RecipeMissCount> RecipeMissNames{"no recipe", "device", "template gone", "objects gone", "not recordable"};
constexpr std::array<const char*, RecipeMissCount> DrawRecipeMissNames{"no recipe", "device", "template gone", "targets gone", "not recordable"};
constexpr std::array<const char*, static_cast<std::size_t>(Graphics::ShaderResources::ProofPath::Count)> ProofPathNames{"fast", "T1 refreshed", "full"};
constexpr std::array<const char*, static_cast<std::size_t>(Graphics::ShaderResources::ProofFailure::Count)> ProofFailureNames{"none", "imports", "evicted", "pending", "changed", "keys", "other"};

struct RecipeCounters {
    struct Kind {
        std::atomic<std::uint64_t> hits{0};
        std::array<std::atomic<std::uint64_t>, RecipeMissCount> precheckMisses{};
        std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(Graphics::ShaderResources::ProofPath::Count)> proofPaths{};
        std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(Graphics::ShaderResources::ProofFailure::Count)> rebuilds{};
        std::atomic<std::uint64_t> rebuildDevice{0};
        std::atomic<std::uint64_t> presyncsFromRecipe{0}, presyncsRecomputed{0}, presyncWaits{0};
        std::atomic<std::uint64_t> dataRefreshed{0}, dataSkipped{0}, refreshByWords{0};
        std::atomic<std::uint64_t> restarts{0}, attaches{0}, verified{0}, verifyReplaced{0};
        std::atomic<std::uint64_t> precheckNs{0}, proofNs{0}, recordNs{0};
    };
    std::array<Kind, 3> kinds;
    std::atomic<std::int64_t> lastReport{0};
};

RecipeCounters& Recipes() {
    static RecipeCounters counters;
    return counters;
}

constexpr std::size_t MoveRefusals = static_cast<std::size_t>(Graphics::ShaderResources::MoveRefusal::Count);

struct DispatchMoveCounters {
    std::atomic<std::uint64_t> checked{0}, exact{0}, moved{0}, buffers{0}, bytes{0};
    std::atomic<std::uint64_t> rebuilt{0}, kept{0}, served{0};
    std::array<std::atomic<std::uint64_t>, 5> sizes{};
    std::array<std::atomic<std::uint64_t>, MoveRefusals> refused{};
    std::array<std::atomic<std::uint64_t>, MoveRefusals> filtered{};
    std::atomic<std::int64_t> lastReport{0};
};

DispatchMoveCounters& DispatchMoves() {
    static DispatchMoveCounters counters;
    return counters;
}

void reportDispatchMove() {
    auto& moves = DispatchMoves();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = moves.lastReport.load();
    if (nowMs - last < 10000 || !moves.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    const auto list = [&](std::array<std::atomic<std::uint64_t>, MoveRefusals>& counters) {
        std::string text;
        unsigned long long total = 0;
        char item[80];
        for (std::size_t i = 1; i < MoveRefusals; ++i) {
            const auto count = take(counters[i]);
            total += count;
            if (count == 0) continue;
            std::snprintf(item, sizeof(item), "%s %s %llu", text.empty() ? "" : ",", Graphics::ShaderResources::MoveRefusalName(static_cast<Graphics::ShaderResources::MoveRefusal>(i)), count);
            text += item;
        }
        std::snprintf(item, sizeof(item), "%llu (", total);
        return std::string(item) + text + ")";
    };
    const auto checked = take(moves.checked);
    const auto exact = take(moves.exact);
    const auto moved = take(moves.moved);
    const auto buffers = take(moves.buffers);
    const auto bytes = take(moves.bytes);
    const auto rebuilt = take(moves.rebuilt);
    const auto kept = take(moves.kept);
    const auto served = take(moves.served);
    unsigned long long sizes[5];
    for (std::size_t i = 0; i < 5; ++i) sizes[i] = take(moves.sizes[i]);
    const auto refused = list(moves.refused);
    const auto filtered = list(moves.filtered);
    AgcDriver::ProfilePrint_nid_no_patch("[dispmove] (10 s) %llu template hits checked: %llu with every read-only buffer where it was built, %llu with %llu moved ones (%.2f MiB snapshotted; per dispatch up to 1 KiB %llu, 16 KiB %llu, 256 KiB %llu, 4 MiB %llu, more %llu); refused %s at stage A (the hit was dropped before the record), %s at record; after a refusal: %llu rebuilt, %llu inserted beside the movable template (kept), %llu served by an exact entry\n", checked, exact, moved, buffers, static_cast<double>(bytes) / 1048576.0, sizes[0], sizes[1], sizes[2], sizes[3], sizes[4], filtered.c_str(), refused.c_str(), rebuilt, kept, served);
}

void noteDispatchMoveSize(std::size_t bytes) {
    auto& moves = DispatchMoves();
    const std::size_t bucket = bytes <= 1024 ? 0 : bytes <= 16384 ? 1 : bytes <= 262144 ? 2 : bytes <= 4194304 ? 3 : 4;
    moves.sizes[bucket].fetch_add(1, std::memory_order_relaxed);
}

void reportRecipes() {
    auto& recipes = Recipes();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = recipes.lastReport.load();
    if (nowMs - last < 10000 || !recipes.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto take = [](std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.exchange(0, std::memory_order_relaxed)); };
    for (std::size_t kind = 0; kind < 3; ++kind) {
        auto& counters = recipes.kinds[kind];
        const auto hits = take(counters.hits);
        std::string misses, proofs, rebuilds;
        char text[64];
        for (std::size_t i = 0; i < RecipeMissCount; ++i) {
            std::snprintf(text, sizeof(text), "%s%s %llu", i == 0 ? "" : ", ", (kind == 2 ? DrawRecipeMissNames : RecipeMissNames)[i], take(counters.precheckMisses[i]));
            misses += text;
        }
        for (std::size_t i = 0; i < ProofPathNames.size(); ++i) {
            std::snprintf(text, sizeof(text), "%s%s %llu", i == 0 ? "" : ", ", ProofPathNames[i], take(counters.proofPaths[i]));
            proofs += text;
        }
        for (std::size_t i = 1; i < ProofFailureNames.size(); ++i) {
            std::snprintf(text, sizeof(text), "%s%s %llu", i == 1 ? "" : ", ", ProofFailureNames[i], take(counters.rebuilds[i]));
            rebuilds += text;
        }
        std::snprintf(text, sizeof(text), ", device %llu", take(counters.rebuildDevice));
        rebuilds += text;
        const auto perHit = [&](std::atomic<std::uint64_t>& ns) { return hits != 0 ? static_cast<double>(take(ns)) / 1000.0 / static_cast<double>(hits) : 0.0; };
        const auto precheckUs = perHit(counters.precheckNs);
        const auto proofUs = perHit(counters.proofNs);
        const auto recordUs = perHit(counters.recordNs);
        AgcDriver::ProfilePrint_nid_no_patch("[recipe] %s (10 s): hits %llu; pre-check misses: %s; proofs: %s; rebuilds: %s; presyncs from the recipe %llu (recomputed %llu, waited %llu); data refreshes recorded %llu / skipped by hash %llu (%llu decided by words: data hits); restarts %llu, attaches %llu; us per hit: pre-check %.1f, proof %.1f, record %.1f; verified %llu (object replaced %llu)\n", kind == 0 ? "dispatch" : kind == 1 ? "indirect" : "draw", hits, misses.c_str(), proofs.c_str(), rebuilds.c_str(), take(counters.presyncsFromRecipe), take(counters.presyncsRecomputed), take(counters.presyncWaits), take(counters.dataRefreshed), take(counters.dataSkipped), take(counters.refreshByWords), take(counters.restarts), take(counters.attaches), precheckUs, proofUs, recordUs, take(counters.verified), take(counters.verifyReplaced));
    }
}

struct DispatchTimer {
    bool profile;
    bool indirect;
    std::size_t spirvWords;
    std::uint64_t programAddress;
    std::array<double, DispatchRows> callMs{};
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point phaseStart;
    DispatchTimer(bool profile, bool indirect, std::size_t spirvWords, std::uint64_t programAddress) : profile(profile), indirect(indirect), spirvWords(spirvWords), programAddress(programAddress), start(std::chrono::steady_clock::now()), phaseStart(start) {}
    void add(DispatchPhase which, double ms) {
        if (!profile) return;
        auto& d = Dispatches();
        callMs[which] += ms;
        d.phaseTotals[which] += ms;
        if (indirect) d.indirectHold.phaseMs[which] += ms;
    }
    void phase(DispatchPhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(now - phaseStart).count();
        add(which, ms);
        phaseStart = now;
        if (ms > 1000) AgcDriver::ProfilePrint_nid_no_patch("[dispatch] %s took %.0f ms (%zu words, program 0x%llx)\n", DispatchRowName(which), ms, spirvWords, static_cast<unsigned long long>(programAddress));
    }
    void restart() { phaseStart = std::chrono::steady_clock::now(); }
    std::string formatCall() const {
        std::string text;
        char row[96];
        for (std::size_t i = 0; i < DispatchRows; ++i) {
            if (callMs[i] == 0) continue;
            std::snprintf(row, sizeof(row), " %s=%.2fms", DispatchRowName(i), callMs[i]);
            text += row;
        }
        return text;
    }
    static void countDispatch(bool profile) {
        if (!profile) return;
        auto& d = Dispatches();
        if (++d.profiledDispatches % 1000 != 0) return;
        std::string report;
        for (std::size_t row = 0; row < DispatchPhaseCount; ++row) {
            if (d.phaseTotals[row] != 0) report += " " + std::string(DispatchRowName(row)) + "=" + std::to_string(static_cast<long long>(d.phaseTotals[row] / 1000)) + "s";
        }
        AgcDriver::ProfilePrint_nid_no_patch("[dispatch] %llu dispatches, phase totals:%s\n", static_cast<unsigned long long>(d.profiledDispatches), report.c_str());
    }
    void finish(std::uint64_t lookupsBefore, const char* groupsText, bool indirectHold) const {
        if (!profile) return;
        auto& d = Dispatches();
        d.procLookups += Graphics::DeviceProcLookups() - lookupsBefore;
        const auto now = std::chrono::steady_clock::now();
        const auto totalMs = std::chrono::duration<double, std::milli>(now - start).count();
        if (totalMs > 100) AgcDriver::ProfilePrint_nid_no_patch("[dispatch] %s %zu words:%s\n", groupsText, spirvWords, formatCall().c_str());
        if (!indirectHold) return;
        auto& hold = d.indirectHold;
        ++hold.count;
        hold.totalMs += totalMs;
        if (totalMs > hold.maxMs) {
            hold.maxMs = totalMs;
            hold.maxPhases = formatCall();
        }
        if (now - hold.lastReport <= std::chrono::seconds(10)) return;
        hold.lastReport = now;
        std::string report;
        for (std::size_t i = 0; i < DispatchRows; ++i) {
            if (hold.phaseMs[i] == 0 && hold.phaseCounts[i] == 0) continue;
            char text[112];
            if (hold.phaseCounts[i] != 0) std::snprintf(text, sizeof(text), " %s %llux %.0f ms", DispatchRowName(i), static_cast<unsigned long long>(hold.phaseCounts[i]), hold.phaseMs[i]);
            else std::snprintf(text, sizeof(text), " %s %.0f ms", DispatchRowName(i), hold.phaseMs[i]);
            report += text;
        }
        AgcDriver::ProfilePrint_nid_no_patch("[indirect] %llu indirect dispatches spent %.0f ms inside the device call (10 s; max %.1f ms:%s), by phase (the 'resources: ...' rows split 'resources'; 'hook waits' overlaps them; 'images: ...' rows are the lookups by outcome, count x ms, inside 'B images'):%s; copied-writer lists scanned per decision: dispatch avg %.1f (max %zu), draw avg %.1f\n", static_cast<unsigned long long>(hold.count), hold.totalMs, hold.maxMs, hold.maxPhases.c_str(), report.c_str(), hold.count != 0 ? static_cast<double>(hold.writersScanned) / hold.count : 0.0, hold.maxWriters, hold.count != 0 ? static_cast<double>(hold.drawWritersScanned) / hold.count : 0.0);
        hold.phaseMs = {};
        hold.phaseCounts = {};
        hold.count = 0;
        hold.totalMs = 0;
        hold.maxMs = 0;
        hold.maxPhases.clear();
        hold.writersScanned = 0;
        hold.drawWritersScanned = 0;
        hold.maxWriters = 0;
    }
};

}

struct RecordedDispatch {
    const Graphics::Context* context;
    const Graphics::CompiledShader* shader;
    std::shared_ptr<Graphics::ShaderResources> resources;
    std::shared_ptr<ComputePipelineObjects> objects;
    VkShaderStageFlags pushStages;
    const std::array<std::byte, Graphics::PipelinePushConstantBytes>* pushBytes;
    std::uint32_t x, y, z;
    std::uint64_t arguments;
    const Graphics::HostImport* argumentImport;
    std::uint64_t programAddress;
    enum class DataRefresh { None, Words, Hash };
    DataRefresh dataRefresh;
    std::uint64_t dataWordsHash;
    double revalidateMs;
    DispatchTimer* timer;
    std::optional<std::pair<std::uint64_t, std::size_t>> htileClear;
    bool refreshed = false;
    bool resourceKeyMatched = false;
    std::shared_ptr<Graphics::ShaderResources::DrawBindings> bindings{};
    std::uint64_t cpuArguments = 0;
    int cpuReason = 0;
    std::array<std::uint32_t, 3> threadGroup{};
    std::array<std::uint32_t, 3> threadLimit{};
};

struct DispatchCompletion {
    const Graphics::Context& context;
    const std::array<Graphics::CompiledShader, 1>& shaders;
    VkShaderStageFlags pushStages;
    const std::array<std::byte, Graphics::PipelinePushConstantBytes>& pushBytes;
    const std::shared_ptr<ComputePipelineObjects>& objects;
    const std::shared_ptr<Graphics::ShaderResources>& resources;
    const ResourceCache::Key& contentKey;
    bool cacheable;
    bool fromCache;
    double revalidateMs;
    const std::shared_ptr<Graphics::ShaderResources::DrawBindings>& overlay;
    std::uint64_t lookupsBefore;
    DispatchTimer& timer;
    PerformanceTimer& timing;
    char* groupsText;
};

VkDevice VulkanDevice::Device() const {
    return state->device;
}

bool VulkanDevice::DmaBufImportSelected() const {
    return Graphics::DmaBufImportSelected(graphicsContext());
}

VulkanDevice::DispatchMoveCount VulkanDevice::DispatchMoveCounts() {
    auto& moves = DispatchMoves();
    DispatchMoveCount count{moves.checked.load(), moves.exact.load(), moves.moved.load(), 0, 0, moves.rebuilt.load(), moves.kept.load(), moves.served.load()};
    for (std::size_t i = 1; i < MoveRefusals; ++i) {
        count.refused += moves.refused[i].load();
        count.filtered += moves.filtered[i].load();
    }
    return count;
}

bool VulkanDevice::DispatchRecipes() {
    static const bool noDispatchRecipe = std::getenv("APS5_NO_DISPATCH_RECIPE") != nullptr;
    return !noDispatchRecipe;
}

bool VulkanDevice::VerifyRecipes() {
    static const bool verifyRecipe = std::getenv("APS5_VERIFY_RECIPE") != nullptr;
    return verifyRecipe;
}

bool VulkanDevice::TemplateDataRefresh() {
    return AgcDriver::TemplateDataRefresh();
}

void VulkanDevice::NoteRecipe(RecipeEvent event, bool indirect) {
    NoteRecipe(event, indirect ? RecipeKind::Indirect : RecipeKind::Dispatch);
}

void VulkanDevice::NoteRecipe(RecipeEvent event, RecipeKind kind) {
    auto& counters = Recipes().kinds[static_cast<std::size_t>(kind)];
    (event == RecipeEvent::Restart ? counters.restarts : counters.attaches).fetch_add(1, std::memory_order_relaxed);
}

void VulkanDevice::NoteDrawRecipeMiss(DrawRecipePrecheck miss) {
    auto& counters = Recipes().kinds[static_cast<std::size_t>(RecipeKind::Draw)];
    counters.precheckMisses[miss == DrawRecipePrecheck::NoRecipe ? MissNoRecipe : MissDevice].fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t VulkanDevice::DrawRecipeHits() {
    return Recipes().kinds[static_cast<std::size_t>(RecipeKind::Draw)].hits.load(std::memory_order_relaxed);
}

RecipeOutcome VulkanDevice::DrawFromRecipe(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots, const std::shared_ptr<const DrawRecipe>& recipe, const DrawTraceContext* trace) {
    PerformanceTimer timing("Vulkan.DrawFromRecipe");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& counters = Recipes().kinds[static_cast<std::size_t>(RecipeKind::Draw)];
    if (recipe->device != state->device) {
        counters.rebuildDevice.fetch_add(1, std::memory_order_relaxed);
        return RecipeOutcome::Rebuild;
    }
    const auto context = graphicsContext();
    const auto outcome = Graphics::DrawWithRecipe(context, graphics, draw, shaders, snapshots, *recipe, trace);
    if (!outcome.recorded) {
        switch (outcome.miss) {
            case Graphics::DrawRecipeMiss::NotRecordable: counters.precheckMisses[MissNotRecordable].fetch_add(1, std::memory_order_relaxed); break;
            case Graphics::DrawRecipeMiss::TargetGone:
            case Graphics::DrawRecipeMiss::ObjectsGone: counters.precheckMisses[MissObjectsGone].fetch_add(1, std::memory_order_relaxed); break;
            case Graphics::DrawRecipeMiss::TemplateGone: counters.precheckMisses[MissTemplateGone].fetch_add(1, std::memory_order_relaxed); break;
            case Graphics::DrawRecipeMiss::Proof: counters.rebuilds[static_cast<std::size_t>(outcome.proof.failure)].fetch_add(1, std::memory_order_relaxed); break;
            default: break;
        }
        return RecipeOutcome::Rebuild;
    }
    counters.proofPaths[static_cast<std::size_t>(outcome.proof.path)].fetch_add(1, std::memory_order_relaxed);
    counters.hits.fetch_add(1, std::memory_order_relaxed);
    if (profile) {
        counters.proofNs.fetch_add(static_cast<std::uint64_t>(outcome.proofUs * 1000.0), std::memory_order_relaxed);
        counters.recordNs.fetch_add(static_cast<std::uint64_t>(outcome.recordUs * 1000.0), std::memory_order_relaxed);
        reportRecipes();
    }
    return RecipeOutcome::Recorded;
}

std::uint64_t VulkanDevice::presync(std::span<const Graphics::PresyncSurface> surfaces) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& counters = Presyncs();
    bool overlaps = false;
    for (const auto& surface : surfaces) {
        const auto bytes = static_cast<std::size_t>(surface.bytes);
        if (surface.dccKeys) counters.keyChecks.fetch_add(1, std::memory_order_relaxed);
        overlaps = overlaps || (surface.dccKeys ? Graphics::DccKeyWritersPending(surface.address, bytes) : Graphics::Recorder::SnapshotWriteOverlaps(surface.address, bytes));
    }
    std::uint64_t serial = 0;
    bool texelsPending = false;
    if (overlaps) {
        std::lock_guard lock(GuestMemory::GpuMutex());
        if (state->recorder) {
            bool open = false;
            for (const auto& surface : surfaces) {
                const auto bytes = static_cast<std::size_t>(surface.bytes);
                if (surface.dccKeys && !Graphics::DccKeyWriterNeedsSync(surface.address, bytes)) continue;
                const auto info = state->recorder->DescribePendingWrite(surface.address, bytes);
                if (!info.has_value()) continue;
                open = open || info->open;
                serial = std::max(serial, info->serial);
                texelsPending = texelsPending || !surface.dccKeys;
            }
            if (open) serial = state->recorder->SubmitAndEpoch();
        }
    }
    if (serial != 0) {
        const auto waitStart = std::chrono::steady_clock::now();
        WaitRecorded(serial);
        counters.presyncs.fetch_add(1, std::memory_order_relaxed);
        if (!texelsPending) counters.keyWaits.fetch_add(1, std::memory_order_relaxed);
        counters.waitedUs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - waitStart).count()), std::memory_order_relaxed);
    }
    if (profile) {
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto last = counters.lastReport.load();
        if (nowMs - last >= 10000 && counters.lastReport.compare_exchange_strong(last, nowMs)) {
            const auto keyWaitCounts = Graphics::KeyWaitCounts();
            AgcDriver::ProfilePrint_nid_no_patch("[presync] %llu dispatches checked, %llu pre-syncs waited %.0f ms (cumulative); key ranges %llu checked, %llu pre-syncs that only the keys required; WaitForKeyWriters %llu calls: %llu without a pending writer, %llu known keys, %llu fills leaving uncompressed, %llu synced\n", static_cast<unsigned long long>(counters.checked.load()), static_cast<unsigned long long>(counters.presyncs.load()), counters.waitedUs.load() / 1000.0, static_cast<unsigned long long>(counters.keyChecks.load()), static_cast<unsigned long long>(counters.keyWaits.load()), static_cast<unsigned long long>(keyWaitCounts.calls), static_cast<unsigned long long>(keyWaitCounts.noPending), static_cast<unsigned long long>(keyWaitCounts.known), static_cast<unsigned long long>(keyWaitCounts.fillsUncompressed), static_cast<unsigned long long>(keyWaitCounts.synced));
        }
    }
    return serial;
}

VulkanDevice::PresyncStatistics VulkanDevice::PresyncCounts() {
    const auto& counters = Presyncs();
    return {counters.checked.load(std::memory_order_relaxed), counters.presyncs.load(std::memory_order_relaxed), counters.keyChecks.load(std::memory_order_relaxed), counters.keyWaits.load(std::memory_order_relaxed)};
}

std::shared_ptr<PreparedDispatch> VulkanDevice::PrepareDispatch(const ShaderRecompiler::RecompileResult& shader, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t dispatchThreads, std::uint64_t programAddress, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear, const std::vector<ShaderRecompiler::RegisteredImageView>* registeredImageViews) {
    ShaderRecompiler::RuntimeAbi::RequireVersion(shader.runtimeAbiVersion);
    static const bool lockedBuild = std::getenv("APS5_LOCKED_BUILD") != nullptr;
    if (lockedBuild || shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) return nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = std::chrono::steady_clock::now();
    const Graphics::CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &shader, 0, registeredImageViews};
    const auto context = graphicsContext();
    auto prepared = std::make_shared<PreparedDispatch>();
    prepared->dispatchThreads = dispatchThreads;
    std::shared_ptr<Graphics::ShaderResources> cached;
    auto phaseStart = start;
    const auto phase = [&](PreparedDispatch::PreparePhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        prepared->phaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseStart).count();
        phaseStart = now;
    };
    if (!htileClear.has_value() && ResourceCacheEnabled() && shader.PipelineVariantId() != 0) {
        prepared->key = DispatchContentKey(compute, context.device);
        phase(PreparedDispatch::PrepareKey);
        cached = state->resourceCache.Find(prepared->key);
        if (cached != nullptr && DispatchMove()) {
            Graphics::ShaderResources::MoveProof proof;
            if (!cached->MovedDispatchBuffers(compute, nullptr, DispatchMoveBytes(), proof).has_value()) {
                DispatchMoves().filtered[static_cast<std::size_t>(proof.refusal)].fetch_add(1, std::memory_order_relaxed);
                prepared->key = DispatchContentKey(compute, context.device, false);
                prepared->exact = true;
                cached = state->resourceCache.Find(prepared->key);
            }
        }
        prepared->cached = cached;
        phase(PreparedDispatch::PrepareFind);
    }
    if (cached == nullptr) {
        prepared->resources = std::make_shared<Graphics::ShaderResources>(context, compute, snapshots, true, dispatchThreads, htileClear);
        phase(PreparedDispatch::PrepareStageA);
    }
    if (cached != nullptr && CachedPrecollect()) {
        cached->PrecollectSurfaces();
        phase(PreparedDispatch::PreparePrecollect);
    }
    if (!NoPresync() && CanWaitUnlocked()) {
        Presyncs().checked.fetch_add(1, std::memory_order_relaxed);
        const auto surfaces = cached != nullptr ? cached->PresyncSurfaces() : prepared->resources->PresyncSurfaces();
        prepared->presyncSerial = presync(surfaces);
        phase(PreparedDispatch::PreparePresync);
    }
    if (profile) prepared->prepareMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const auto& built = cached != nullptr ? *cached : *prepared->resources;
    const std::array<Graphics::CompiledShader, 1> shaders{{compute}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::ComputePushConstantBytes) return prepared;
    if (built.Layout() != VK_NULL_HANDLE) {
        const auto layouts = built.SetLayouts();
        prepared->objects = computePipeline(context, shader, layouts.main, layouts.mainKey, pushStages, programAddress);
    } else if (context.descriptorCache != nullptr) {
        const auto described = Graphics::DescribeSetLayouts(context, shaders);
        prepared->describedLayoutKey = described.mainKey;
        prepared->objects = computePipeline(context, shader, described.layouts.main, described.mainKey, pushStages, programAddress);
    }
    return prepared;
}

}
}
}
}
}
}
