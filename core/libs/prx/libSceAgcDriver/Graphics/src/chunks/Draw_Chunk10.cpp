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
void drawTessellationGeometry(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, const DrawTraceContext* trace) {
    using Stage = ShaderRecompiler::ShaderStage;
    Require(context.meshShader && context.geometryShader, "tessellation followed by a geometry stage needs mesh and geometry shaders");
    Require(!draw.indirect, "an indirect draw with tessellation and a geometry stage must be resolved by the driver");
    Require(draw.instanceCount <= 1u, "tessellation followed by a geometry stage is drawn for one instance only");
    if (draw.indexCount == 0 || draw.instanceCount == 0) return;
    const auto& tessellation = *state.stages.tessellation;
    const auto& geometry = *state.stages.tessellationGeometry;
    std::vector<CompiledShader> first;
    std::vector<CompiledShader> second;
    for (const auto& shader : shaders) {
        if (shader.stage != Stage::Mesh && shader.stage != Stage::Fragment) first.push_back(shader);
        if (shader.stage == Stage::Mesh || shader.stage == Stage::Fragment) second.push_back(shader);
    }
    Require(second.size() == 2u && second.front().stage == Stage::Mesh && std::ranges::count_if(first, [](const CompiledShader& shader) { return shader.stage == Stage::Geometry; }) == 1, "tessellation followed by a geometry stage needs its capture and mesh stages");
    const auto patches = static_cast<std::uint64_t>(draw.indexCount / tessellation.inputControlPoints);
    const auto level = static_cast<std::uint64_t>(std::ceil(std::clamp(state.stages.maxTessellationLevel, 1.0f, 64.0f))) + 1u;
    const auto perPatch = 2u * level * level;
    const auto capacity = static_cast<std::uint32_t>(std::min(perPatch * patches, MaxCapturedTriangles));
    auto capture = std::make_shared<TessellationCapture>();
    capture->header = std::make_shared<Buffer>(context, ShaderRecompiler::MeshCapturedHeaderBytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    std::array<std::uint32_t, ShaderRecompiler::MeshCapturedHeaderBytes / 4> header{};
    header[1] = 1u;
    header[2] = 1u;
    header[ShaderRecompiler::MeshCapturedCapacityDword] = capacity;
    std::memcpy(capture->header->Bytes().data(), header.data(), sizeof(header));
    capture->items = std::make_shared<Buffer>(context, static_cast<std::size_t>(capacity) * 3u * geometry.esgsItemSize * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto tessellated = state;
    tessellated.stages.path = ShaderPath::Tessellation;
    tessellated.stages.tessellationGeometry.reset();
    tessellated.rasterizerDiscard = true;
    tessellated.tessellationCapture = capture;
    Draw(context, tessellated, draw, first, snapshots, nullptr, trace);
    auto meshed = state;
    meshed.stages.path = ShaderPath::Geometry;
    meshed.stages.mesh = geometry;
    meshed.stages.tessellation.reset();
    meshed.stages.tessellationGeometry.reset();
    meshed.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    meshed.tessellationCapture = capture;
    const Pm4::DrawParameters captured{0, 3u, 0, 1u, 0, false, 0, 0};
    Draw(context, meshed, captured, second, snapshots, nullptr, trace);
}

std::vector<std::uint32_t> DrawResourceContentKey(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders) {
    if (!EarlyDrawKeys() || shaders.empty() || std::any_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.program == nullptr; })) return {};
    static const bool trimKey = std::getenv("APS5_NO_DRAW_KEY_TRIM") == nullptr;
    const auto indexBytes = static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize;
    return DrawResourceKey(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), draw.indexAddress, indexBytes, !trimKey);
}

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, std::shared_ptr<const DrawRecipe>* recipeOut, const DrawTraceContext* trace, std::uint64_t shape, const std::vector<std::uint32_t>* contentKey) {
    const DepthCopySourceScope depthCopySource(state.depthCopySource);
    if (state.stages.tessellationGeometry) {
        drawTessellationGeometry(context, state, draw, shaders, snapshots, trace);
        return;
    }
    if (state.stages.mesh && state.layerExports != 0u && state.renderLayers > context.meshLimits.maxMeshOutputLayers && (draw.indirect || (draw.indexCount != 0u && draw.instanceCount != 0u))) {
        const auto limit = context.meshLimits.maxMeshOutputLayers;
        Require(limit != 0u && !state.depth, "mesh layer windows require a nonzero output limit and color-only layered rendering");
        Require(std::ranges::all_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->layerReplaySafe; }), "mesh layer windows refuse shaders with guest memory writes, clocks or unproven side effects");
        if (recipeOut != nullptr) recipeOut->reset();
        for (std::uint32_t base = 0; base < state.renderLayers;) {
            auto window = state;
            window.renderLayers = std::min(limit, state.renderLayers - base);
            window.meshLayerBase = base;
            window.meshLayerCount = window.renderLayers;
            for (auto& color : window.colors) {
                Require(color.layers == state.renderLayers, "mesh layer window targets have different layer counts");
                if (color.depth > 1u) color.depthSlice += base;
                else color.arraySlice += base;
                color.layers = window.renderLayers;
            }
            if (!window.colors.empty()) window.color = window.colors.front();
            Draw(context, window, draw, shaders, snapshots, nullptr, trace);
            base += window.renderLayers;
        }
        return;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PreparedDraw prepared(profile);
    prepared.recipeOut = recipeOut;
    prepared.trace = trace;
    prepared.shape = shape;
    prepared.contentKey = contentKey;
    prepared.passAttachments = PassAttachmentMemoEnabled();
    const struct HeldSpace {
        PreparedDraw& prepared;
        ~HeldSpace() {
            const auto& resources = prepared.resolved.resources;
            if (resources == nullptr || !resources->SpaceReuse() || !resources->SpaceHeld()) return;
            if (prepared.spaceKeep != nullptr) *prepared.spaceKeep = resources->ReleaseSpace();
            else if (prepared.recorder != nullptr) resources->HoldSpaceThrough(prepared.recorder->Id(), prepared.recorder->OpenSerial());
            else static_cast<void>(resources->ReleaseSpace());
        }
    } heldSpace{prepared};
    try {
        if (prepareDraw(context, state, draw, shaders, snapshots, prepared)) recordPreparedDraw(context, state, draw, shaders, prepared);
    } catch (...) {
        ThreadTargetMemo().Invalidate();
        keepHeldSpace(prepared);
        throw;
    }
    keepHeldSpace(prepared);
}

DrawRecipeOutcome DrawWithRecipe(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, const DrawRecipe& recipe, const DrawTraceContext* trace) {
    const DepthCopySourceScope depthCopySource(state.depthCopySource);
    if (context.recorder != nullptr) context.recorder->BoundKeptBytes();
    static_cast<void>(snapshots);
    PerformanceTimer timing("Graphics.DrawWithRecipe");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    DrawTimer timer(profile);
    DrawRecipeOutcome result;
    DrawOutcome outcome;
    outcome.kind = KindRecipeHit;
    const auto waitedBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
    double ownWaitedMs = 0;
    const auto miss = [&](DrawRecipeMiss reason) {
        result.miss = reason;
        if (profile) {
            auto& stats = Profile();
            std::lock_guard lock(stats.mutex);
            ++stats.recipeMisses[static_cast<std::size_t>(reason)];
        }
        return result;
    };
    Require(!draw.indirect, "a draw recipe covers direct draws only");
    auto* recorder = Recorder::Active();
    if (state.quadList || StripPatches(state, shaders) || !RecordDraws() || recorder == nullptr || DumpTargetLimit() != 0) return miss(DrawRecipeMiss::NotRecordable);
    if (recipe.hasDepth != state.depth.has_value()) return miss(DrawRecipeMiss::TargetGone);
    const std::size_t expectedViews = state.colors.size() + (recipe.hasDepth ? 1u : 0u);
    Require(recipe.targets.size() == state.colors.size() && recipe.targetViews.size() == expectedViews, "draw recipe targets do not match the state");
    const auto plan = planDraw(context, state, draw, shaders, &recipe, outcome, timer);
    DrawInputs inputs;
    if (plan.nothing) inputs.nothing = true;
    else inputs = prepareDrawInputs(context, state, draw, shaders, plan, timer, timing);
    if (inputs.nothing) {
        result.recorded = true;
        return result;
    }
    // The resident-target proof on the stored images (design_cpu_final 3.7): while an image is
    // still the storage cache's for its surface (StorageImageCached: the cache entry, removed under
    // the cache mutex before an eviction's flush, and the LRU touch a lookup would make), it is
    // what the lookup would return, and Refresh brings it up to date exactly as the lookup's does
    // (FlushPending, the whole-surface collect and UnchangedSince, the key scan). An image gone
    // from the cache is a miss: the ordinary path looks up anew.
    auto& memo = ThreadTargetMemo();
    if (ownsTargetMemo(memo) && (memo.colors.valid || memo.depth.valid)) noteTargetMemo(TargetMemoEvent::RecipeInvalidated);
    memo.Invalidate();
    std::vector<std::shared_ptr<StorageTexture>> targets;
    targets.reserve(recipe.targets.size());
    for (std::size_t index = 0; index < recipe.targets.size(); ++index) {
        timer.phase(PhaseSetup);
        if (state.colors[index].fmaskAddress != 0) GuardOneFragmentFmask(context, state.colors[index]);
        auto stored = recipe.targets[index].lock();
        if (stored == nullptr || !StorageImageCached(context, stored.get()) || !StorageImageServesKeys(*stored, SurfaceForTarget(state.colors[index]).dccAddress)) return miss(DrawRecipeMiss::TargetGone);
        auto resident = refreshResidentTarget(context, state, state.colors[index], outcome, profile, [&] {
            if (state.colors[index].arrayLastSlice != 0) stored->RefreshLayers(state.colors[index].arraySlice, state.colors[index].layers);
            else stored->Refresh();
            return stored;
        });
        timer.phase(PhaseReadTarget);
        if (resident == nullptr) return miss(DrawRecipeMiss::TargetGone);
        targets.push_back(std::move(resident));
    }
    if (recipe.hasDepth) {
        timer.phase(PhaseSetup);
        const auto depthView = DepthSurfaceViewWithStencilWriteIntent(context, *state.depth, state.depthWrite, state.stencilTest, writesStencilTarget(state), overwrittenStencil(state));
        timer.phase(PhaseReadTarget);
        if (recipe.targetViews.back() != depthView) return miss(DrawRecipeMiss::TargetGone);
    }
    timer.phase(PhasePrepare);
    // The template's proof (rules R6/R7): ProveCurrent, T1 included, the alias checks the trimmed
    // key leaves to a hit repeated; a failure removes the template from the cache (the batch keeps
    // it) and the caller rebuilds.
    auto resources = recipe.templateRef.lock();
    if (resources == nullptr) return miss(DrawRecipeMiss::TemplateGone);
    Require(resources->Reusable(), "draw recipe over a non-reusable template");
    const auto proofStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool proved = resources->ProveCurrent(shaders, &result.proof);
    if (profile) result.proofUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - proofStart).count();
    if (!proved) {
        SharedResourceCache().Remove(recipe.key, resources.get());
        countCache(&DrawProfile::cacheInvalidated);
        resources->KeepStorageImageBytes(*recorder);
        recorder->Keep(std::move(resources));
        timer.phase(PhaseLookup);
        return miss(DrawRecipeMiss::Proof);
    }
    const auto moved = resources->MovedReadOnlyBuffers(shaders, *recorder, &state.color);
    if (!moved.has_value()) return miss(DrawRecipeMiss::Buffers);
    CheckBufferAliases(shaders, state.color, draw.indexAddress, inputs.indexBytes);
    if (recipe.hasDepth && writesDepthTarget(state)) NoteDepthSurfaceWrite(context, *state.depth);
    SharedResourceCache().Touch(recipe.key);
    countCache(&DrawProfile::cacheHits);
    timer.phase(PhaseLookup);
    outcome.completion = resources->ChecksFaults();
    outcome.recorded = true;
    // The pipeline is the recipe's while the pipeline store still holds it (its device's teardown
    // or its eviction bound drops it: a miss). The framebuffer is the recipe's while the pipeline's
    // list holds it: the views are the stored targets' (stable while they live, and they are the
    // same objects), so AcquireFramebuffer only runs when the list evicted it.
    auto pipeline = recipe.pipeline.lock();
    if (pipeline == nullptr) return miss(DrawRecipeMiss::ObjectsGone);
    auto framebuffer = recipe.framebuffer.lock();
    if (framebuffer == nullptr) {
        const std::vector<std::shared_ptr<void>> framebufferOwners(targets.begin(), targets.end());
        framebuffer = pipeline->AcquireFramebuffer(recipe.targetViews, framebufferOwners, state.renderExtent, state.renderLayers);
    }
    timer.phase(PhasePipeline);
    const auto recordStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    RecordedDraw record;
    record.recorder = recorder;
    record.masked = recipe.masked.has_value() ? &*recipe.masked : nullptr;
    record.completion = outcome.completion;
    record.resources = std::move(resources);
    record.moved = *moved;
    record.pipeline = std::move(pipeline);
    record.framebuffer = std::move(framebuffer);
    record.targetViews = recipe.targetViews;
    record.targets = std::move(targets);
    for (const auto& color : state.colors) record.targetSlices.emplace_back(color.arraySlice, color.arrayLastSlice != 0 ? color.layers : 1u);
    auto pushBytes = recipe.pushBytes;
    if (recipe.pushStages != 0) {
        pushBytes = AssemblePushConstants(shaders);
        record.resources->PatchPushConstants(pushBytes);
    }
    record.pushBytes = &pushBytes;
    record.pushStages = recipe.pushStages;
    record.depthLocations = passDepthLocations(context, state, *record.pipeline);
    recordDraw(context, state, draw, shaders, inputs, record, outcome, timer, ownWaitedMs, trace);
    if (profile) {
        result.recordUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - recordStart).count();
        auto& stats = Profile();
        std::lock_guard lock(stats.mutex);
        ++stats.recipeHits;
    }
    timing.Mark("draw_and_resource_release");
    reportDrawEnd(state, timer, nullptr, outcome, waitedBefore, ownWaitedMs, " recorded from recipe");
    result.recorded = true;
    return result;
}

namespace {

std::shared_ptr<StorageTexture> metadataPassResident(const Context& context, const ColorTarget& color) {
    if (ColorTileModeIsLinear(color.tileMode) || context.detiler == nullptr) return nullptr;
    const auto resident = CachedStorageSurface(context, SurfaceForTarget(color));
    return resident != nullptr && resident->GuestBytes() == color.bytes ? resident : nullptr;
}

}

std::vector<std::pair<std::uint64_t, std::uint64_t>> PipelinedColorMetadataPassWrites(const ColorMetadataPass& pass) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    if (pass.mode == ColorMetadataPass::Mode::Resolve) {
        if (!pass.source.has_value() || pass.targets.size() != 1) return writes;
        const auto& destination = pass.targets.front();
        if (destination.samples != 1u || destination.bytes == 0 || destination.cmaskAddress != 0) return writes;
        writes.emplace_back(destination.MemoryAddress(), destination.MemoryAddress() + destination.bytes);
        if (destination.dccAddress != 0) writes.emplace_back(destination.dccAddress, destination.dccAddress + std::max<std::size_t>(colorKeyCount(destination, destination.bytes), 1u));
        return writes;
    }
    if (pass.mode != ColorMetadataPass::Mode::EliminateFastClear && pass.mode != ColorMetadataPass::Mode::DccDecompress) return writes;
    for (const auto& color : pass.targets) {
        if (color.samples != 1u || color.bytes == 0) return {};
        writes.emplace_back(color.MemoryAddress(), color.MemoryAddress() + color.bytes);
        if (color.cmaskAddress != 0) writes.emplace_back(color.cmaskAddress, color.cmaskAddress + std::max<std::size_t>(color.cmaskBytes, 1u));
        if (color.dccAddress != 0) writes.emplace_back(color.dccAddress, color.dccAddress + std::max<std::size_t>(colorKeyCount(color, color.bytes), 1u));
    }
    return writes;
}

void RunColorMetadataPass(const Context& context, const ColorMetadataPass& pass) {
    if (pass.mode == ColorMetadataPass::Mode::Resolve) {
        Require(pass.source.has_value() && pass.targets.size() == 1, "CB resolve without its source and destination");
        const auto& destination = pass.targets.front();
        Require(!ColorTileModeIsLinear(destination.tileMode) && context.detiler != nullptr, "CB resolve into a target without a resident image is not implemented");
        const auto resident = CachedStorageSurface(context, SurfaceForTarget(destination));
        Require(resident->Attachable(), "storage format cannot be a color attachment");
        ResolveMultisampleTarget(context, *pass.source, *resident, resident->AttachmentView(destination.format, destination.mip));
        return;
    }
    if (pass.mode == ColorMetadataPass::Mode::FmaskDecompress) {
        for (const auto& color : pass.targets) MaterializeMultisampleTargetClear(context, color);
        return;
    }
    for (const auto& color : pass.targets) {
        if (color.cmaskAddress != 0) {
            const auto resident = metadataPassResident(context, color);
            if (materializeCmaskClear(context, color, resident) && resident != nullptr) resident->MarkDirty();
        }
        if (color.dccAddress == 0) continue;
        auto keys = CurrentDccKeys(color.dccAddress, color.bytes);
        if (keys == DccKeys::Uncompressed) continue;
        if (keys == DccKeys::ClearSingle) {
            Require(pass.mode == ColorMetadataPass::Mode::DccDecompress, "CB fast-clear eliminate over comp-to-single DCC keys (whether it expands them is not modeled)");
            storeSingleTexels(context, color);
            if (const auto resident = metadataPassResident(context, color)) resident->Refresh();
            continue;
        }
        Require(IsDccClear(keys), std::string("CB metadata pass over DCC keys that are ") + DccKeysName(keys) + " (per-block metadata is not modeled)");
        const auto texel = clearTexel(color, keys);
        const auto resident = metadataPassResident(context, color);
        if (resident != nullptr) {
            const char* refusal = nullptr;
            const bool current = keys == DccKeys::ClearRegister ? clearToTexel(*resident, texel, color.elementBytes, refusal, true) : StorageTexture::FindPending(color.address, color.bytes) == resident || resident->UploadedKeys() == keys;
            if (current) {
                resident->MarkDirty();
                MarkDccUncompressed(context, color.dccAddress, color.bytes, colorKeyCount(color, color.bytes));
                continue;
            }
        }
        storeClearTexels(context, color, texel);
    }
}

AddressDrawReuseCounts AddressDrawReuseCounters() {
    AddressDrawReuseCounts counts{};
    const auto& counters = addressDrawReuses();
    for (std::size_t index = 0; index < counts.size(); ++index) counts[index] = counters[index].load(std::memory_order_relaxed);
    return counts;
}

const char* AddressDrawReuseName(AddressDrawReuse outcome) {
    return static_cast<std::size_t>(outcome) < AddressDrawReuseNames.size() ? AddressDrawReuseNames[static_cast<std::size_t>(outcome)] : "?";
}

void SetSpaceKeepFailuresForTest(std::uint32_t count) {
    spaceKeepFailures.store(count, std::memory_order_relaxed);
}

void SetDrawStopAfterResolveForTest(DrawStopForTest stop) {
    stopAfterResolve.store(static_cast<int>(stop), std::memory_order_relaxed);
}

std::uint64_t TargetMemoEvents(TargetMemoEvent event) {
    return targetMemoEvents()[static_cast<std::size_t>(event)].Sum();
}

const char* TargetMemoEventName(TargetMemoEvent event) {
    return static_cast<std::size_t>(event) < TargetMemoEventNames.size() ? TargetMemoEventNames[static_cast<std::size_t>(event)] : "?";
}

std::uint64_t TargetMemoBuildsForTest() {
    return targetMemoBuilds;
}

std::uint64_t TargetMemoHitNanosecondsForTest(const Context& context, const State& state, std::size_t repeats, std::size_t& hits) {
    std::vector<TargetBinding> targets;
    std::vector<VkImageView> views;
    const auto proof = currentTargetProof(context);
    hits = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        targets.clear();
        targets.resize(state.colors.size());
        views.clear();
        if (repeatColors(context, state, proof, targets, views)) ++hits;
    }
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
}

std::uint64_t AddressDrawReuseVerifications() {
    return addressDrawReuseVerifications().load(std::memory_order_relaxed);
}

VertexPlanScratchState ThreadVertexPlanScratch() {
    const auto& scratch = VertexPlanLease::Resting();
    VertexPlanScratchState state;
    state.busy = VertexPlanLease::Busy();
    state.resting = scratch.fetches.size() + scratch.fetchValid.size() + scratch.fetchOf.size() + scratch.zeroed.size() + scratch.isSolo.size() + scratch.plannedFetches.size() + scratch.plannedOf.size() + scratch.rangeHandles.size() + scratch.rangeOffsets.size();
    state.capacity = scratch.fetches.capacity() + scratch.fetchValid.capacity() + scratch.fetchOf.capacity() + scratch.zeroed.capacity() + scratch.isSolo.capacity() + scratch.plannedFetches.capacity() + scratch.plannedOf.capacity() + scratch.rangeHandles.capacity() + scratch.rangeOffsets.capacity();
    return state;
}

}
}
}
}
}
}
}
