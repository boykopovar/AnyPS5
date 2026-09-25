#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

// A guest texture format code with the same Vulkan format and texel size as a color buffer.
std::uint32_t GuestFormatFor(VkFormat format, std::uint32_t elementBytes) {
    static std::mutex mutex;
    static std::map<std::pair<VkFormat, std::uint32_t>, std::uint32_t> known;
    std::lock_guard lock(mutex);
    if (const auto found = known.find({format, elementBytes}); found != known.end()) return found->second;
    for (std::uint32_t code = 0; code < 256; ++code) {
        try {
            if (!IsBlockCompressed(code) && ResolveTextureFormat(code) == format && BytesPerElement(code) == elementBytes) {
                known.emplace(std::make_pair(format, elementBytes), code);
                return code;
            }
        } catch (const std::exception&) {
        }
    }
    throw std::runtime_error("AGC graphics: no guest texture format matches the color buffer format " + std::to_string(static_cast<int>(format)));
}

// The color buffer as a single-mip 2D surface descriptor (tile mode SW_64KB_R_X).
GuestTextureResource SurfaceForTarget(const ColorTarget& color) {
    Require(color.tileMode == ColorTileMode::RenderTarget, "only 64 KiB tiled color targets are resident");
    GuestTextureResource surface{};
    surface.baseAddress = color.address;
    surface.width = color.extent.width;
    surface.height = color.extent.height;
    surface.depthOrLastArray = 0;
    surface.baseArray = 0;
    surface.mipCount = 1;
    surface.baseLevel = 0;
    surface.lastLevel = 0;
    surface.tileMode = TextureTileMode::kR64KBX;
    surface.dimension = TextureDimension::k2D;
    surface.format = GuestFormatFor(color.format, color.elementBytes);
    surface.dstSelX = 4;
    surface.dstSelY = 5;
    surface.dstSelZ = 6;
    surface.dstSelW = 7;
    surface.dccAddress = color.dccAddress;
    surface.dccAlphaOnMsb = color.dccAlphaOnMsb;
    return surface;
}

}

namespace {

void imageBarrier(const Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void memoryBarrier(const Context& context, VkCommandBuffer commands, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, sourceAccess, destinationAccess};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// The color surface as one untailed SW_64KB_R_X mip for the GPU detiler, with tightly packed linear rows.
TileMipLayout ColorTargetMip(const ColorTarget& color, const ColorTargetLayout& layout) {
    TileMipLayout mip{};
    mip.width = color.extent.width;
    mip.height = color.extent.height;
    mip.blocksPerRow = layout.BlocksPerRow();
    mip.pitchBytes = color.extent.width * color.elementBytes;
    mip.tiledSize = layout.Bytes();
    mip.linearSize = layout.LinearBytes();
    return mip;
}

// APS5_PROFILE_DRAW: per-draw phase timers in microseconds (a recorded draw's phases are far below
// the millisecond the old print rounded to), totalled over 10 s in the [draws] line.
enum DrawPhase : std::size_t { PhaseValidate, PhaseVertex, PhaseSetup, PhaseReadTarget, PhasePrepare, PhaseLookup, PhaseResources, PhasePipeline, PhaseRecord, PhaseKeep, PhaseSync, PhaseWriteBack, PhaseDescribe, PhaseCount };
constexpr std::array<const char*, PhaseCount> DrawPhaseNames{"validate", "vertex", "setup", "readTarget", "prepare", "lookup", "resources", "pipeline", "record", "keep", "sync", "writeBack", "describe"};

// Why a draw did not go into the recorder without a wait (counted in the [draws] line): its targets
// are not all resident, no recorder is active, a switch (APS5_SYNC_DRAWS, APS5_DUMP_TARGETS,
// APS5_SYNC_COMPLETION_DRAWS) forced it, or its completion work writes copied guest buffers or
// releases an address-based build's lease, which the CPU must not outrun (see `recorded` in Draw).
enum SyncReason : std::size_t { SyncNone, SyncNotResident, SyncNoRecorder, SyncDisabled, SyncCopiedWrites, SyncLease, SyncCount };
constexpr std::array<const char*, SyncCount> SyncReasonNames{"none", "non-resident target", "no recorder", "disabled", "copied writes", "lease"};

// What became of one draw, for the totals.
struct DrawOutcome {
    // In the recorder; `waited` when the recorder was synced right after (copied writes or a lease).
    bool recorded = false;
    bool waited = false;
    bool completion = false;
    SyncReason reason = SyncNone;
    // Shader validation memo (see CachedFragmentOutputs).
    bool validateMemoized = false;
    bool validateHit = false;
    // A build that acquired the allocation registry lease (BDA), whose cost sits outside the
    // build's own sub-phases.
    bool addressBased = false;
    // Resident target lookups, and those that took a millisecond or more. Whether a slow one
    // re-uploaded the image or just walked its pages under contention is not visible from here
    // (StorageTexture::Version advances on every MarkDirty as well as on an upload, and Generation
    // is stamped afresh by every write-watch walk): the [texture] line's "reused" and "direct
    // uploads" counters and APS5_TRACE_UPLOAD name the actual uploads.
    std::uint64_t targetLookups = 0;
    std::uint64_t slowLookups = 0;
    double slowLookupUs = 0;
    // The flush hook's fence waits made inside this draw (index and vertex reads, the target
    // lookups' flushes, the build's texture lookups): nested in the draw's GpuMutex hold, so they
    // are the part of the hold that is waiting rather than working. The draw's own syncs (a
    // recorded-then-waited draw's recorder Sync, a synchronous draw's SubmitAndWait) are kept apart
    // in ownSyncUs: those wait by design and would otherwise count as nested hook waits.
    double hookWaitUs = 0;
    double ownSyncUs = 0;
};

struct DrawProfile {
    std::mutex mutex;
    std::array<double, PhaseCount> totalsUs{};
    // The longest single draw's time per phase, and the longest draw: the [lock] line's 'draw'
    // hold max (tens of ms against an average well under a millisecond) needs a phase name.
    std::array<double, PhaseCount> maxUs{};
    double maxDrawUs = 0;
    double hookWaitUs = 0;
    double maxHookWaitUs = 0;
    double ownSyncUs = 0;
    // The resources build's sub-phases (ShaderResources::Timing) of draws only; dispatches report
    // theirs in the [resources] line. `other` is the rest of the build (prepareAddressBindings:
    // the registry lease and snapshots of an address-based build, the BDA table, the layout
    // lookup), and how much of it address-based builds account for.
    double bindingsUs = 0;
    double uploadUs = 0;
    double descriptorsUs = 0;
    double otherUs = 0;
    double addressOtherUs = 0;
    std::uint64_t addressBuilds = 0;
    std::uint64_t draws = 0;
    std::uint64_t recorded = 0;
    std::uint64_t waited = 0;
    // Recorded draws whose write-back (BDA fault check) runs as a completion action instead of
    // making the draw synchronous (see APS5_SYNC_COMPLETION_DRAWS).
    std::uint64_t completion = 0;
    std::array<std::uint64_t, SyncCount> reasons{};
    // The CPU inside Graphics::Draw, split by outcome: a synchronous draw costs ten times a recorded
    // one, so one average would only show the synchronous population.
    double recordedUs = 0;
    double waitedUs = 0;
    double synchronousUs = 0;
    std::uint64_t targetLookups = 0;
    std::uint64_t slowLookups = 0;
    double slowLookupUs = 0;
    // Resource cache outcomes: hits, misses (built and inserted when reusable), entries that failed
    // Revalidate, and draws that could not use the cache (synchronous, no variant id, or disabled).
    // Which key words the misses differ in is the cache's own "[rescache] miss churn" line
    // (ResourceCache::noteMiss compares each miss with the last key of the same variants).
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    std::uint64_t cacheInvalidated = 0;
    std::uint64_t uncacheable = 0;
    // Shader validation memo (see CachedFragmentOutputs).
    std::uint64_t validateHits = 0;
    std::uint64_t validateMisses = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

DrawProfile& Profile() {
    static DrawProfile profile;
    return profile;
}

// Adds one draw's phases to the totals and prints the [draws] and [rescache] lines every 10 s.
void reportDraw(const std::array<double, PhaseCount>& us, const ShaderResources::BuildTiming* built, const DrawOutcome& outcome) {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    double drawUs = 0;
    for (std::size_t i = 0; i < PhaseCount; ++i) {
        profile.totalsUs[i] += us[i];
        profile.maxUs[i] = std::max(profile.maxUs[i], us[i]);
        drawUs += us[i];
    }
    profile.maxDrawUs = std::max(profile.maxDrawUs, drawUs);
    profile.hookWaitUs += outcome.hookWaitUs;
    profile.maxHookWaitUs = std::max(profile.maxHookWaitUs, outcome.hookWaitUs);
    profile.ownSyncUs += outcome.ownSyncUs;
    if (built != nullptr) {
        profile.bindingsUs += built->bindingsMs * 1000.0;
        profile.uploadUs += built->uploadMs * 1000.0;
        profile.descriptorsUs += built->descriptorsMs * 1000.0;
        const auto other = std::max(0.0, us[PhaseResources] - (built->bindingsMs + built->uploadMs + built->descriptorsMs) * 1000.0);
        profile.otherUs += other;
        if (outcome.addressBased) {
            profile.addressOtherUs += other;
            ++profile.addressBuilds;
        }
    }
    ++profile.draws;
    if (outcome.recorded) ++(outcome.waited ? profile.waited : profile.recorded);
    if (outcome.completion) ++profile.completion;
    ++profile.reasons[outcome.reason];
    (outcome.recorded ? (outcome.waited ? profile.waitedUs : profile.recordedUs) : profile.synchronousUs) += drawUs;
    profile.targetLookups += outcome.targetLookups;
    profile.slowLookups += outcome.slowLookups;
    profile.slowLookupUs += outcome.slowLookupUs;
    if (outcome.validateMemoized) ++(outcome.validateHit ? profile.validateHits : profile.validateMisses);
    const auto now = std::chrono::steady_clock::now();
    if (now - profile.lastReport < std::chrono::seconds(10)) return;
    profile.lastReport = now;
    const auto synchronous = profile.draws - profile.recorded - profile.waited;
    const auto average = [](double total, std::uint64_t count) { return count != 0 ? total / static_cast<double>(count) : 0.0; };
    char line[1536];
    int n = std::snprintf(line, sizeof(line), "[draws] %llu draws over 10 s (%llu recorded avg %.0f us, of them %llu with completion; %llu recorded then waited avg %.0f us; %llu synchronous avg %.0f us; waited or synchronous because:", static_cast<unsigned long long>(profile.draws), static_cast<unsigned long long>(profile.recorded), average(profile.recordedUs, profile.recorded), static_cast<unsigned long long>(profile.completion), static_cast<unsigned long long>(profile.waited), average(profile.waitedUs, profile.waited), static_cast<unsigned long long>(synchronous), average(profile.synchronousUs, synchronous));
    const auto room = [&] { return n > 0 && static_cast<std::size_t>(n) < sizeof(line); };
    for (std::size_t i = SyncNone + 1; i < SyncCount && room(); ++i) {
        if (profile.reasons[i] != 0) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu", SyncReasonNames[i], static_cast<unsigned long long>(profile.reasons[i]));
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "):");
    for (std::size_t i = 0; i < PhaseCount && room(); ++i) {
        if (profile.totalsUs[i] <= 0) continue;
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s=%.1fms", DrawPhaseNames[i], profile.totalsUs[i] / 1000.0);
        if (i == PhaseResources && room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " (bindings %.1f, upload %.1f, descriptors %.1f, other %.1f of which %.1f in %llu address-based builds)", profile.bindingsUs / 1000.0, profile.uploadUs / 1000.0, profile.descriptorsUs / 1000.0, profile.otherUs / 1000.0, profile.addressOtherUs / 1000.0, static_cast<unsigned long long>(profile.addressBuilds));
        if (i == PhaseReadTarget && room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " (%llu resident lookups, %llu of them >= 1 ms = %.1f)", static_cast<unsigned long long>(profile.targetLookups), static_cast<unsigned long long>(profile.slowLookups), profile.slowLookupUs / 1000.0);
    }
    // The longest draw and the longest single phase of any draw (which phase a long 'draw' hold
    // was), plus the hook waits nested inside the draws.
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; longest draw %.1f ms, longest phase of any draw:", profile.maxDrawUs / 1000.0);
    for (std::size_t i = 0; i < PhaseCount && room(); ++i) {
        if (profile.maxUs[i] < 500.0) continue;
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %.1f", DrawPhaseNames[i], profile.maxUs[i] / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; hook waits inside draws %.1f ms (max %.1f; the draws' own syncs, %.1f ms, not counted)", profile.hookWaitUs / 1000.0, profile.maxHookWaitUs / 1000.0, profile.ownSyncUs / 1000.0);
    std::fprintf(stderr, "%s\n", line);
    std::fprintf(stderr, "[rescache] draws: %llu hits, %llu misses, %llu invalidated, %llu uncacheable; validation memo %llu hits / %llu misses (which key words the misses differ in: the miss churn line)\n", static_cast<unsigned long long>(profile.cacheHits), static_cast<unsigned long long>(profile.cacheMisses), static_cast<unsigned long long>(profile.cacheInvalidated), static_cast<unsigned long long>(profile.uncacheable), static_cast<unsigned long long>(profile.validateHits), static_cast<unsigned long long>(profile.validateMisses));
    profile.totalsUs.fill(0);
    profile.maxUs.fill(0);
    profile.maxDrawUs = profile.hookWaitUs = profile.maxHookWaitUs = profile.ownSyncUs = 0;
    profile.bindingsUs = profile.uploadUs = profile.descriptorsUs = profile.otherUs = profile.addressOtherUs = 0;
    profile.addressBuilds = 0;
    profile.draws = profile.recorded = profile.waited = profile.completion = 0;
    profile.reasons.fill(0);
    profile.recordedUs = profile.waitedUs = profile.synchronousUs = 0;
    profile.targetLookups = profile.slowLookups = 0;
    profile.slowLookupUs = 0;
    profile.cacheHits = profile.cacheMisses = profile.cacheInvalidated = profile.uncacheable = 0;
    profile.validateHits = profile.validateMisses = 0;
}

void countCache(std::uint64_t DrawProfile::*counter) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++(stats.*counter);
}

// ValidateShaders decodes every SPIR-V instruction of every stage on every draw. Its outcome depends
// only on the stages' compiled variants (a variant id names identical SPIR-V and binding layout),
// their push constant placement and vertex attribute shapes, and the state fields it checks, so the
// fragment output set is remembered by exactly those. Nothing here is keyed by pointer: the results
// a draw hands in live in a per-draw vector. A stage without a variant id is validated as before,
// except the rect-list control and evaluation stages, which are generated from the vertex and
// fragment results the key already names (the pipeline key treats them the same way). `memoized`
// says whether the memo applied, `hit` whether it answered. APS5_NO_VALIDATE_CACHE=1 validates
// every draw.
std::set<std::uint32_t> CachedFragmentOutputs(const Context& context, std::span<const CompiledShader> shaders, const State& state, bool& memoized, bool& hit) {
    using Stage = ShaderRecompiler::ShaderStage;
    static const bool disabled = std::getenv("APS5_NO_VALIDATE_CACHE") != nullptr;
    memoized = false;
    hit = false;
    std::vector<std::uint64_t> key;
    const auto add = [&](auto value) { key.push_back(static_cast<std::uint64_t>(value)); };
    bool keyed = !disabled;
    if (keyed) {
        key.reserve(40 + shaders.size() * 12);
        add(shaders.size());
        for (const auto& shader : shaders) {
            Require(shader.program != nullptr, "missing compiled shader");
            const auto& program = *shader.program;
            const bool generated = state.rectList && (shader.stage == Stage::TessellationControl || shader.stage == Stage::TessellationEvaluation);
            if (!generated && program.variantId == 0) {
                keyed = false;
                break;
            }
            add(shader.stage);
            add(generated ? std::uint64_t{0} : program.variantId);
            add(shader.pushConstantOffset);
            add(program.pushConstants.size());
            add(program.bdaAbiVersion);
            add(program.vertexAttributes.size());
            for (const auto& attribute : program.vertexAttributes) {
                add(attribute.location);
                add(attribute.components);
                add(attribute.fetchIndex);
                // The data format bits of the V# decide the attribute's signature.
                add((attribute.resource.fields[3] >> 12u) & 0x7fu);
            }
        }
    }
    if (keyed) {
        add(state.stages.path);
        add(state.rectList);
        add(state.topology);
        add(state.cullMode);
        add(state.colors.size());
        add(context.subgroup.subgroupSize);
        add(context.subgroup.supportedStages);
        add(context.subgroup.supportedOperations);
        add(context.fragmentShaderBarycentric);
        add(state.stages.mesh.has_value());
        if (state.stages.mesh) {
            const auto& mesh = *state.stages.mesh;
            add(mesh.inputPrimitive);
            add(mesh.primitivesPerGroup);
            add(mesh.verticesPerGroup);
            add(mesh.maxVertices);
            add(mesh.maxPrimitives);
            add(mesh.threadsPerGroup);
            add(mesh.ldsSizeDwords);
            add(mesh.provokingVertex);
        }
        add(state.stages.tessellation.has_value());
        if (state.stages.tessellation) {
            const auto& tessellation = *state.stages.tessellation;
            add(tessellation.inputControlPoints);
            add(tessellation.outputControlPoints);
            add(tessellation.domain);
            add(tessellation.partitioning);
            add(tessellation.outputTopology);
        }
    }
    static std::mutex mutex;
    static std::map<std::vector<std::uint64_t>, std::set<std::uint32_t>> memo;
    if (keyed) {
        memoized = true;
        std::lock_guard lock(mutex);
        if (const auto found = memo.find(key); found != memo.end()) {
            hit = true;
            return found->second;
        }
    }
    auto outputs = ValidateShaders(shaders, state, context.subgroup, context.fragmentShaderBarycentric);
    if (keyed) {
        std::lock_guard lock(mutex);
        // A handful of configurations recur; a runaway key space is dropped wholesale.
        if (memo.size() >= 1024) memo.clear();
        memo.emplace(std::move(key), outputs);
    }
    return outputs;
}

// The resource cache key of a recorded draw: a marker no compute key starts with (those begin with
// the stage), the device handle (the cache is process-wide and the driver replaces the headless
// device with the windowed one while workers may still use the old one: an entry's descriptor set
// and pooled buffers belong to the device that built it) and every stage's content key
// (ResourceCache::noteMiss walks this layout to attribute misses, so it changes together with it).
// With `ranges`, also the render target and the index buffer the build's alias checks compared the
// guest buffers against; without it the checks are repeated for a hit (CheckBufferAliases), since
// neither range is part of the descriptor set (the target is attached, the index buffer copied per
// draw), so a target or index ring that moves per frame does not miss on every draw. That guards a
// workload with such rings; in the profiled menu stage every draw was DRAW_INDEX_AUTO (index range
// 0) onto one fixed target, so its misses come from the stages' descriptor words themselves.
ResourceCache::Key DrawResourceKey(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::uint64_t indexBytes, bool ranges) {
    ResourceCache::Key key{0xffffffffu};
    const auto append64 = [&](std::uint64_t value) {
        key.push_back(static_cast<std::uint32_t>(value));
        key.push_back(static_cast<std::uint32_t>(value >> 32u));
    };
    append64(reinterpret_cast<std::uint64_t>(context.device));
    key.push_back(static_cast<std::uint32_t>(shaders.size()));
    for (const auto& shader : shaders) {
        const auto part = ShaderResources::ContentKey(shader);
        key.push_back(static_cast<std::uint32_t>(part.size()));
        key.insert(key.end(), part.begin(), part.end());
    }
    if (ranges) {
        append64(target.address);
        append64(target.bytes);
        append64(indexAddress);
        append64(indexBytes);
    }
    return key;
}

// The alias checks the build makes for every guest buffer descriptor (ShaderResources::
// addGuestBuffer), repeated for a cached build whose own checks compared the buffers against
// another draw's render target and index buffer. The same conditions and messages, so a draw that
// would have failed its build fails its hit.
void CheckBufferAliases(std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::uint64_t indexBytes) {
    const auto overlap = [](std::uint64_t first, std::uint64_t firstSize, std::uint64_t second, std::uint64_t secondSize) { return first < second + secondSize && second < first + firstSize; };
    for (const auto& shader : shaders) {
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
            const auto& words = binding.guestDescriptor;
            for (std::size_t offset = 0; offset + 4 <= words.size(); offset += 4) {
                const ShaderRecompiler::ShaderBufferResource descriptor{{words[offset], words[offset + 1], words[offset + 2], words[offset + 3]}};
                const auto address = descriptor.Base48();
                const auto size = descriptor.GetSize();
                Require(!overlap(address, size, target.address, target.bytes), "shader buffer aliases the render target");
                Require(!overlap(address, size, indexAddress, indexBytes), "writable shader buffer aliases the index buffer");
            }
        }
    }
}

}

std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>> DrawCopiedWriters() {
    // Never destroyed: a completion action of a batch still in flight at static teardown may erase from it.
    static auto* const writers = new std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>>(std::make_shared<std::vector<std::shared_ptr<ShaderResources>>>());
    return *writers;
}

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots) {
    PerformanceTimer timing("Graphics.Draw");
    // APS5_PROFILE_DRAW prints the time of each phase of the draw (microseconds) and the [draws] totals.
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto phaseStart = std::chrono::steady_clock::now();
    std::array<double, PhaseCount> us{};
    const auto phase = [&](DrawPhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        us[which] += std::chrono::duration<double, std::micro>(now - phaseStart).count();
        phaseStart = now;
    };
    // APS5_TRACE_DRAWS (together with APS5_PROFILE_DRAW; alone it only enables Driver.cpp's own
    // [draw] lines) additionally prints one line per draw with its phases and, for synchronous
    // draws, their inputs: per-draw string building at ~1400 draws per 10 s is itself a cost the
    // totals must not carry.
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    DrawOutcome outcome;
    const ShaderResources::BuildTiming* built = nullptr;
    // The thread's GPU waits so far: the difference at the report, less the draw's own syncs
    // (`ownWaitedMs`, sampled around them below), is what this draw waited for through the flush
    // hook (nested in the draw's hold, see DrawOutcome::hookWaitUs).
    const auto waitedBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
    double ownWaitedMs = 0;
    // One line per draw with the phases that took time (tracing), and the 10 s totals.
    const auto report = [&](const char* suffix) {
        if (!profile) return;
        outcome.hookWaitUs = std::max(0.0, Recorder::ThreadWaitedMs() - waitedBefore - ownWaitedMs) * 1000.0;
        outcome.ownSyncUs = ownWaitedMs * 1000.0;
        if (traceDraws) {
            char line[512];
            int n = std::snprintf(line, sizeof(line), "[draw] %ux%u %zu targets%s:", state.renderExtent.width, state.renderExtent.height, state.colors.size(), suffix);
            for (std::size_t i = 0; i < PhaseCount && n > 0 && static_cast<std::size_t>(n) < sizeof(line); ++i) {
                if (us[i] <= 0) continue;
                n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s=%.0fus", DrawPhaseNames[i], us[i]);
            }
            std::fprintf(stderr, "%s\n", line);
        }
        reportDraw(us, built, outcome);
    };
    APS5_LOG_OUT_DEBUG("Draw indices=%u instances=%u indexSize=%u flags=%u indexAddress=0x%llx", draw.indexCount, draw.instanceCount, draw.indexSize, draw.flags, static_cast<unsigned long long>(draw.indexAddress));
    APS5_LOG_OUT_DEBUG("State colorTarget=%u render=%ux%u colorAddress=0x%llx colorBytes=%llu colorExtent=%ux%u", state.hasColorTarget ? 1u : 0u, state.renderExtent.width, state.renderExtent.height, static_cast<unsigned long long>(state.color.address), static_cast<unsigned long long>(state.color.bytes), state.color.extent.width, state.color.extent.height);
    APS5_LOG_OUT_DEBUG("Viewport x=%f y=%f w=%f h=%f minDepth=%f maxDepth=%f", state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height, state.viewport.minDepth, state.viewport.maxDepth);
    APS5_LOG_OUT_DEBUG("Scissor x=%d y=%d w=%u h=%u topology=%u cullMode=0x%x frontFace=%u", state.scissor.offset.x, state.scissor.offset.y, state.scissor.extent.width, state.scissor.extent.height, static_cast<unsigned>(state.topology), static_cast<unsigned>(state.cullMode), static_cast<unsigned>(state.frontFace));
    Require(draw.indexed ? draw.flags == 0 : (draw.flags & ~0x20u) == 0, "draw modifiers are unsupported");
    if (draw.indexed) {
        Require(draw.indexSize == 2 || draw.indexSize == 4, "only uint16 and uint32 index buffers are supported");
        Require(draw.firstVertex == 0 && draw.firstInstance == 0, "indexed draw offsets are unsupported");
    } else {
        Require(draw.indexAddress == 0 && draw.indexSize == 0, "auto draw must not reference an index buffer");
        if (draw.indexCount == 0 || draw.instanceCount == 0) return;
        Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - (draw.indexCount - 1u), "auto draw vertex range overflow");
        Require(draw.firstInstance <= std::numeric_limits<std::uint32_t>::max() - (draw.instanceCount - 1u), "auto draw instance range overflow");
    }
    Require(draw.indexCount != 0 && draw.instanceCount != 0, "zero-count indexed draws are unsupported");
    const auto indexBytes = static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize;
    APS5_LOG_OUT_DEBUG("Index buffer bytes=%llu", static_cast<unsigned long long>(indexBytes));
    Require(indexBytes <= std::numeric_limits<std::size_t>::max(), "index buffer size overflow");
    if (draw.indexed) GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
    APS5_LOG_CHARS_OUT_DEBUG("Index buffer range OK");
    Require(!draw.indexed || !state.hasColorTarget || draw.indexAddress + indexBytes <= state.color.address || state.color.address + state.color.bytes <= draw.indexAddress, "index buffer aliases the render target");
    if (state.rectList) Require(draw.indexCount % 3 == 0, "incomplete rect-list primitive");
    APS5_LOG_CHARS_OUT_DEBUG("ValidateShaders");
    const auto fragmentOutputs = CachedFragmentOutputs(context, shaders, state, outcome.validateMemoized, outcome.validateHit);
    APS5_LOG_CHARS_OUT_DEBUG("ValidateShaders OK");
    const auto shaderStages = PipelineStages(shaders);
    APS5_LOG_OUT_DEBUG("PipelineStages=0x%x", static_cast<unsigned>(shaderStages));
    std::uint32_t meshGroups = 0;
    if (state.stages.mesh) {
        Require(draw.firstVertex == 0 && draw.firstInstance == 0, "mesh draw offsets are unsupported");
        APS5_LOG_CHARS_OUT_DEBUG("Mesh path");
        Require(context.meshShader, "device does not support mesh shaders");
        const auto& mesh = *state.stages.mesh;
        const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
        Require(draw.indexCount >= inputSize && mesh.primitivesPerGroup != 0, "mesh draw contains no complete primitive");
        const auto step = mesh.inputPrimitive == 6 ? 1u : inputSize;
        const auto primitives = (draw.indexCount - inputSize) / step + 1u;
        meshGroups = (primitives - 1u) / mesh.primitivesPerGroup + 1u;
        APS5_LOG_OUT_DEBUG("Mesh primitives=%u groups=%u", primitives, meshGroups);
        Require(meshGroups <= context.meshLimits.maxMeshWorkGroupCount[0] && draw.instanceCount <= context.meshLimits.maxMeshWorkGroupCount[1] && static_cast<std::uint64_t>(meshGroups) * draw.instanceCount <= context.meshLimits.maxMeshWorkGroupTotalCount, "mesh draw exceeds workgroup count limits");
    }
    if (state.stages.tessellation) Require(draw.indexCount % state.stages.tessellation->inputControlPoints == 0, "incomplete tessellation patch");
    // Viewport and scissor are dynamic pipeline state, so their limits are checked here per draw.
    ValidateViewport(context, state.viewport);
    timing.Mark("validate");
    phase(PhaseValidate);
    std::unique_ptr<Buffer> indices;
    std::uint32_t maxIndex = draw.indexed ? 0u : draw.firstVertex + draw.indexCount - 1u;
    if (draw.indexed) {
        indices = std::make_unique<Buffer>(context, static_cast<std::size_t>(indexBytes), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        GuestMemory::Read(draw.indexAddress, indices->Bytes(), draw.indexSize);
        for (std::size_t offset = 0; offset < indexBytes; offset += draw.indexSize) {
            std::uint32_t index = 0;
            if (draw.indexSize == 2) {
                std::uint16_t value = 0;
                std::memcpy(&value, indices->Bytes().data() + offset, sizeof(value));
                index = value;
            } else {
                std::memcpy(&index, indices->Bytes().data() + offset, sizeof(index));
            }
            Require(index <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
            maxIndex = std::max(maxIndex, index);
        }
    }
    APS5_LOG_CHARS_OUT_DEBUG("Index validation OK");
    timing.Mark("index_upload");
    const auto& attributes = shaders.front().program->vertexAttributes;
    // Validates the vertex descriptors; the layout also keys and builds the pipeline below.
    const auto vertexInput = BuildVertexInputLayout(context, attributes);
    std::vector<std::unique_ptr<Buffer>> vertexBuffers;
    std::vector<VkBuffer> vertexHandles;
    std::vector<VkDeviceSize> vertexOffsets(attributes.size(), 0);
    for (const auto& attribute : attributes) {
        const auto bytes = VertexBufferReadSize(attribute, maxIndex, draw.instanceCount, draw.firstInstance);
        const auto& fields = attribute.resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(!state.hasColorTarget || address + bytes <= state.color.address || state.color.address + state.color.bytes <= address, "vertex buffer aliases the render target");
        GuestMemory::CheckRange(reinterpret_cast<const void*>(address), bytes, 1);
        auto buffer = std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        GuestMemory::Read(address, buffer->Bytes(), 1);
        vertexHandles.push_back(buffer->Handle());
        vertexBuffers.push_back(std::move(buffer));
    }
    phase(PhaseVertex);
    // One binding per color attachment. Tiled targets are detiled and retiled on the GPU in video memory;
    // only changed guest blocks are stored back.
    struct TargetBinding {
        ColorTarget color;
        bool gpuTiling = false;
        // Resident target: the surface's cached storage image is attached directly and marked dirty
        // afterwards, so nothing is copied in or out per draw.
        std::shared_ptr<StorageTexture> resident;
        // Linear pixels converted on the CPU, for targets the GPU detiler does not handle.
        std::unique_ptr<Buffer> transfer;
        // Guest bytes in host memory, and their device-local copies for the detiler.
        std::unique_ptr<Buffer> tiled;
        std::unique_ptr<DeviceBuffer> tiledDevice;
        std::unique_ptr<DeviceBuffer> linearDevice;
        std::vector<std::byte> original;
        TileMipLayout mip{};
        std::unique_ptr<RenderTarget> target;
        // APS5_DUMP_TARGETS: the rendered linear pixels, read back for target_<address>_<n>.raw.
        std::unique_ptr<Buffer> dump;
    };
    // Debug aid: APS5_DUMP_TARGETS=<n> saves the first n renders of every color target as a raw file
    // (u32 width, u32 height, u32 VkFormat, then tightly packed rows).
    static const int dumpLimit = [] { const char* text = std::getenv("APS5_DUMP_TARGETS"); return text ? std::atoi(text) : 0; }();
    static std::mutex dumpMutex;
    static std::map<std::uint64_t, int> dumped;
    std::vector<TargetBinding> targets(state.colors.size());
    std::vector<VkImageView> targetViews;
    for (std::size_t index = 0; index < state.colors.size(); ++index) {
        auto& binding = targets[index];
        binding.color = state.colors[index];
        const auto& color = binding.color;
        binding.gpuTiling = color.tileMode == ColorTileMode::RenderTarget && context.detiler != nullptr;
        APS5_LOG_OUT_DEBUG("Creating color target %zu address=0x%llx bytes=%llu extent=%ux%u", index, static_cast<unsigned long long>(color.address), static_cast<unsigned long long>(color.bytes), color.extent.width, color.extent.height);
        const ColorTargetLayout colorLayout(color.extent.width, color.extent.height, color.tileMode, color.elementBytes);
        constexpr VkBufferUsageFlags copies = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        phase(PhaseSetup);
        // Debug aid: APS5_NO_RESIDENT_TARGETS=1 copies every target in and out again.
        static const bool residentTargets = std::getenv("APS5_NO_RESIDENT_TARGETS") == nullptr;
        if (binding.gpuTiling && residentTargets) {
            // The lookup refreshes the image on every draw (StorageTexture::Refresh: FlushPending,
            // CollectWrites over the target's pages, the DCC key scan of TextureClearKeys, then
            // UnchangedSince). The page walk is skipped while the worker's collect epoch lasts
            // (GuestMemory::BumpCollectEpoch: ordering points of the queue, or every packet under
            // APS5_PACKET_EPOCH=1); the key scan runs on every lookup regardless. Lookups of a
            // millisecond or more are counted apart; the [texture] line says how many uploaded.
            const auto lookupStart = std::chrono::steady_clock::now();
            try {
                auto resident = CachedStorageSurface(context, SurfaceForTarget(color));
                Require(resident->Attachable(), "storage format cannot be a color attachment");
                Require(resident->GuestBytes() == colorLayout.Bytes(), "resident image layout differs from the color layout");
                binding.resident = std::move(resident);
            } catch (const std::exception& error) {
                static std::mutex reportMutex;
                static std::set<std::uint64_t> reported;
                std::lock_guard lock(reportMutex);
                if (reported.insert(color.address).second) std::fprintf(stderr, "[gpu] color target 0x%llx stays non-resident: %s\n", static_cast<unsigned long long>(color.address), error.what());
            }
            if (profile) {
                const auto lookupUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - lookupStart).count();
                ++outcome.targetLookups;
                if (lookupUs >= 1000.0) {
                    ++outcome.slowLookups;
                    outcome.slowLookupUs += lookupUs;
                }
            }
        }
        if (binding.resident != nullptr) {
            phase(PhaseReadTarget);
            targetViews.push_back(binding.resident->AttachmentView(color.format));
            continue;
        }
        if (binding.gpuTiling) {
            binding.mip = ColorTargetMip(color, colorLayout);
            binding.tiled = std::make_unique<Buffer>(context, colorLayout.Bytes(), copies);
            binding.tiledDevice = std::make_unique<DeviceBuffer>(context, colorLayout.Bytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            binding.linearDevice = std::make_unique<DeviceBuffer>(context, colorLayout.LinearBytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            binding.original.resize(colorLayout.Bytes());
            GuestMemory::Read(color.address, binding.original, colorLayout.Alignment());
            std::memcpy(binding.tiled->Bytes().data(), binding.original.data(), binding.original.size());
        } else {
            binding.transfer = std::make_unique<Buffer>(context, colorLayout.LinearBytes(), copies);
            ReadColorTarget(color, binding.transfer->Bytes());
        }
        // A fast-cleared DCC target holds its clear value wherever the draw does not write.
        if (color.dccAddress != 0) {
            const auto keys = ReadDccKeys(color.dccAddress, colorLayout.Bytes());
            if (IsDccClear(keys)) {
                const auto pixels = binding.gpuTiling ? binding.tiled->Bytes() : binding.transfer->Bytes();
                if (!FillDccClear(color.format, keys, color.dccAlphaOnMsb, pixels)) {
                    static std::set<std::pair<std::uint64_t, int>> reported;
                    if (reported.size() < 32 && reported.insert({color.address, static_cast<int>(keys)}).second) std::fprintf(stderr, "[gpu] color target 0x%llx (VkFormat %d) has %s DCC keys; its stored texels are used\n", static_cast<unsigned long long>(color.address), static_cast<int>(color.format), DccKeysName(keys));
                    if (binding.gpuTiling) std::memcpy(pixels.data(), binding.original.data(), binding.original.size());
                    else ReadColorTarget(color, pixels);
                }
            }
        }
        phase(PhaseReadTarget);
        binding.target = std::make_unique<RenderTarget>(context, color, state.blends[index].blendEnable != 0);
        targetViews.push_back(binding.target->View());
    }
    phase(PhasePrepare);
    // A draw whose targets are all resident is recorded into the device's open batch like a dispatch:
    // the CPU does not wait for it, and the drain before it goes away (queue order and the barriers
    // below keep it behind earlier recorded work); see `recorded` below for the buffers that still
    // need a synchronous draw. Other draws keep their own synchronous batch. Debug aid:
    // APS5_SYNC_DRAWS=1 makes every draw synchronous.
    static const bool recordDraws = std::getenv("APS5_SYNC_DRAWS") == nullptr;
    auto* recorder = Recorder::Active();
    const bool recordable = recordDraws && recorder != nullptr && dumpLimit == 0 && std::all_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.resident != nullptr; });
    APS5_LOG_CHARS_OUT_DEBUG("Creating ShaderResources");
    // A recordable draw whose stages' compiled content repeats an earlier one binds that build's
    // descriptor set when it is still valid (see ResourceCache; the dispatch path does the same).
    // Only recordable draws take part: a synchronous draw writes its resources back, which a shared
    // object must never do. Revalidate proves a hit by texture identity, which needs the texture
    // caches. Push constants still come from this draw's stages. APS5_NO_DRAW_RESOURCE_CACHE=1
    // builds every draw's resources as before.
    static const bool noDrawResourceCache = std::getenv("APS5_NO_DRAW_RESOURCE_CACHE") != nullptr;
    static const bool noTextureCache = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    // The render target and index ranges stay out of the key (see DrawResourceKey); a hit repeats
    // the alias checks instead. Debug aid: APS5_NO_DRAW_KEY_TRIM=1 keys them as before.
    static const bool trimKey = std::getenv("APS5_NO_DRAW_KEY_TRIM") == nullptr;
    const bool cacheable = recordable && !noDrawResourceCache && !noTextureCache && std::all_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->variantId != 0; });
    std::shared_ptr<ShaderResources> resources;
    ResourceCache::Key contentKey;
    if (cacheable) {
        contentKey = DrawResourceKey(context, shaders, state.color, draw.indexAddress, indexBytes, !trimKey);
        if (auto cached = SharedResourceCache().Find(contentKey)) {
            if (cached->Revalidate(shaders)) {
                if (trimKey) CheckBufferAliases(shaders, state.color, draw.indexAddress, indexBytes);
                resources = std::move(cached);
                countCache(&DrawProfile::cacheHits);
            } else {
                SharedResourceCache().Remove(contentKey);
                countCache(&DrawProfile::cacheInvalidated);
            }
        }
    } else {
        countCache(&DrawProfile::uncacheable);
    }
    phase(PhaseLookup);
    if (resources == nullptr) {
        resources = std::make_shared<ShaderResources>(context, shaders, state.color, draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots);
        built = &resources->Timing();
        outcome.addressBased = resources->HoldsLease();
        if (cacheable) countCache(&DrawProfile::cacheMisses);
    }
    phase(PhaseResources);
    APS5_LOG_CHARS_OUT_DEBUG("ShaderResources created");
    APS5_LOG_CHARS_OUT_DEBUG("Creating Pipeline");
    // Vulkan leaves attachments a pixel shader has no output for undefined, so their writes are masked
    // (shaders that only store to images or buffers keep their targets as they were). The state is
    // copied only when a mask must change.
    std::optional<State> masked;
    for (std::size_t index = 0; index < state.blends.size(); ++index) {
        if (fragmentOutputs.contains(static_cast<std::uint32_t>(index)) || state.blends[index].colorWriteMask == 0) continue;
        if (!masked.has_value()) masked = state;
        masked->blends[index].colorWriteMask = 0;
    }
    const State& pipelineState = masked.has_value() ? *masked : state;
    auto pipeline = CachedPipeline(context, pipelineState, vertexInput, *resources, shaders);
    // Resident targets' views are stable while their storage image lives, so the framebuffer is
    // reused with the pipeline; a per-draw RenderTarget gets a framebuffer of its own.
    std::vector<std::shared_ptr<StorageTexture>> owners;
    owners.reserve(targets.size());
    for (const auto& binding : targets) owners.push_back(binding.resident);
    auto framebuffer = pipeline->AcquireFramebuffer(targetViews, owners, state.renderExtent);
    phase(PhasePipeline);
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline created");
    APS5_LOG_CHARS_OUT_DEBUG("Creating CommandBatch");
    // A draw whose only completion work is a BDA fault check (the rect-list stages carry a fault
    // buffer, so every rect-list draw needed one) is recorded too, with the check as a completion
    // action like a dispatch's write-back: its own batch would first wait for everything recorded
    // before it. A draw that writes a copied buffer (at the menu stage: 0x48-byte constant buffers
    // inside an import but not at its storage offset alignment, so GuestBufferMemory copies them
    // and every guest buffer counts as writable) is recorded and then waited for at once: the
    // write-back runs before the next packet, without the separate command batch and fence a
    // synchronous draw takes. The copied write-back is listed in DrawCopiedWriters, which lets such
    // draws run without the wait; both consumers of the device's list in VulkanDevice.cpp
    // (DispatchIndirect's argument check and the fill HLE's range check, see Draw.hpp) consult this
    // list too. A draw holding an address-based build's lease (which pins guest allocations until
    // the write-back releases it) runs free like the dispatch path's: the completion releases the
    // lease, and a guest thread that needs a leased allocation syncs the recorder through the
    // registry's pin waiter (see SyncLeaseWork); APS5_SYNC_LEASE_DISPATCH=1 waits for such draws
    // at once as before. Debug aids:
    // APS5_SYNC_COMPLETION_DRAWS=1 keeps every draw with completion work synchronous;
    // APS5_NO_RECORDER_SYNC_DRAWS=1 gives the waited-for draws their own batch as before;
    // APS5_NO_RECORD_COPIED_DRAWS=1 waits for copied-write draws at once instead of listing them.
    static const bool syncCompletionDraws = std::getenv("APS5_SYNC_COMPLETION_DRAWS") != nullptr;
    static const bool recorderSyncDraws = std::getenv("APS5_NO_RECORDER_SYNC_DRAWS") == nullptr;
    static const bool recordCopiedDraws = std::getenv("APS5_NO_RECORD_COPIED_DRAWS") == nullptr;
    const bool completion = resources->NeedsCompletion();
    const bool copiedWrites = completion && resources->HasCopiedWrites();
    // A lease only forces the wait when deferred release is off.
    const bool lease = resources->HoldsLease() && SyncLeaseWork();
    outcome.completion = completion;
    outcome.recorded = recordable;
    // Whether the copied write-back is listed in DrawCopiedWriters instead of waited for.
    bool listed = false;
    if (!recordable) {
        outcome.reason = !recordDraws || dumpLimit != 0 ? SyncDisabled : recorder == nullptr ? SyncNoRecorder : SyncNotResident;
    } else if (completion && (syncCompletionDraws || copiedWrites || lease)) {
        outcome.reason = syncCompletionDraws ? SyncDisabled : lease ? SyncLease : SyncCopiedWrites;
        if (syncCompletionDraws) outcome.recorded = false;
        else if (copiedWrites && !lease && recordCopiedDraws) listed = true;
        else if (recorderSyncDraws) outcome.waited = true;
        else outcome.recorded = false;
    }
    const bool recorded = outcome.recorded;
    // A build this recorded draw can share with later identical ones goes into the cache (a cache
    // hit is reusable by construction, so `recorded` holds for it; one with completion work is
    // never reusable).
    if (cacheable && built != nullptr && recorded && resources->Reusable()) SharedResourceCache().Insert(contentKey, resources);
    std::optional<CommandBatch> batch;
    if (!recorded) {
        // The color-target detiles below take their descriptor sets from a fresh batch.
        if (context.detiler != nullptr) context.detiler->BeginBatch();
        batch.emplace(context);
    }
    const auto commands = recorded ? recorder->Commands() : batch->Handle();
    APS5_LOG_CHARS_OUT_DEBUG("CommandBatch created");
    VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    upload.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    upload.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | shaderStages, 0, 1, &upload, 0, nullptr, 0, nullptr);
    APS5_LOG_CHARS_OUT_DEBUG("Upload barrier recorded");
    for (auto& binding : targets) {
        if (binding.resident != nullptr) {
            // Earlier recorded work (dispatches, the previous draw) wrote the image in the general layout.
            imageBarrier(context, commands, binding.resident->Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            continue;
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {binding.color.extent.width, binding.color.extent.height, 1};
        const VkBuffer linear = binding.gpuTiling ? binding.linearDevice->Handle() : binding.transfer->Handle();
        if (binding.gpuTiling) {
            CopyBuffer(context, commands, binding.tiled->Handle(), 0, binding.tiledDevice->Handle(), 0, binding.original.size());
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            context.detiler->Dispatch(commands, TextureTileMode::kR64KBX, binding.color.elementBytes, binding.tiledDevice->Handle(), 0, binding.linearDevice->Handle(), 0, binding.mip);
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        }
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    APS5_LOG_OUT_DEBUG("Beginning pipeline renderExtent=%ux%u", state.renderExtent.width, state.renderExtent.height);
    pipeline->Begin(commands, *framebuffer, state.renderExtent, state.viewport, state.scissor);
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline Begin OK");
    resources->Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout());
    APS5_LOG_CHARS_OUT_DEBUG("Resources bound");
    pipeline->PushConstants(commands, shaders);
    APS5_LOG_CHARS_OUT_DEBUG("Push constants recorded");
    if (state.stages.mesh) {
        APS5_LOG_OUT_DEBUG("vkCmdDrawMeshTasksEXT groups=%u instances=%u", meshGroups, draw.instanceCount);
        context.Function<PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT")(commands, meshGroups, draw.instanceCount, 1);
    } else {
        if (!vertexHandles.empty()) context.Function<PFN_vkCmdBindVertexBuffers>("vkCmdBindVertexBuffers")(commands, 0, static_cast<std::uint32_t>(vertexHandles.size()), vertexHandles.data(), vertexOffsets.data());
        if (draw.indexed) {
            context.Function<PFN_vkCmdBindIndexBuffer>("vkCmdBindIndexBuffer")(commands, indices->Handle(), 0, draw.indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            context.Function<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed")(commands, draw.indexCount, draw.instanceCount, 0, 0, 0);
        } else {
            context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, draw.indexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
        }
    }
    APS5_LOG_CHARS_OUT_DEBUG("Draw recorded");
    context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(commands);
    APS5_LOG_CHARS_OUT_DEBUG("Render pass ended");
    for (auto& binding : targets) {
        if (binding.resident != nullptr) {
            imageBarrier(context, commands, binding.resident->Image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
            continue;
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {binding.color.extent.width, binding.color.extent.height, 1};
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        const VkBuffer linear = binding.gpuTiling ? binding.linearDevice->Handle() : binding.transfer->Handle();
        VkBufferMemoryBarrier reuse{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        reuse.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        reuse.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        reuse.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reuse.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reuse.buffer = linear;
        reuse.size = VK_WHOLE_SIZE;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &reuse, 0, nullptr);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear, 1, &copy);
        if (binding.gpuTiling) {
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
            if (dumpLimit > 0) {
                std::lock_guard lock(dumpMutex);
                if (dumped[binding.color.address] < dumpLimit) {
                    binding.dump = std::make_unique<Buffer>(context, binding.linearDevice->Size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                    CopyBuffer(context, commands, binding.linearDevice->Handle(), 0, binding.dump->Handle(), 0, binding.linearDevice->Size());
                }
            }
            context.detiler->Dispatch(commands, TextureTileMode::kR64KBX, binding.color.elementBytes, binding.linearDevice->Handle(), 0, binding.tiledDevice->Handle(), 0, binding.mip, true);
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, binding.tiledDevice->Handle(), 0, binding.tiled->Handle(), 0, binding.original.size());
        }
    }
    VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    download.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    download.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | shaderStages, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
    APS5_LOG_CHARS_OUT_DEBUG("Download barrier recorded");
    APS5_LOG_CHARS_OUT_DEBUG("SubmitAndWait");
    phase(PhaseRecord);
    if (recorded) {
        // Everything the recorded draw uses lives until its batch completes.
        struct Kept {
            std::shared_ptr<ShaderResources> resources;
            std::shared_ptr<Pipeline> pipeline;
            std::shared_ptr<Framebuffer> framebuffer;
            std::unique_ptr<Buffer> indices;
            std::vector<std::unique_ptr<Buffer>> vertexBuffers;
            std::vector<std::shared_ptr<StorageTexture>> targets;
        };
        auto kept = std::make_shared<Kept>();
        kept->resources = resources;
        kept->pipeline = pipeline;
        kept->framebuffer = framebuffer;
        kept->indices = std::move(indices);
        kept->vertexBuffers = std::move(vertexBuffers);
        for (auto& binding : targets) kept->targets.push_back(binding.resident);
        recorder->Keep(std::move(kept));
        // Counted for the [address-sync] line: a lease released by the completion (the pin waiter
        // finishes the recorder up to the open batch, which holds the kept resources and gets the
        // next serial), or waited for below (any wait releases it at once).
        if (resources->HoldsLease()) CountLeaseOutcome(outcome.waited, outcome.waited ? 0 : recorder->Submissions() + 1);
        // Every use of a (possibly shared) resources object registers its GPU writes anew.
        resources->MarkGpuWrites(*recorder);
        // The write-back (fault check, copied buffers) runs when the batch completed; a fault is
        // reported by the recorder ("deferred write-back failed") instead of thrown out of the draw.
        if (listed) {
            // As VulkanDevice::dispatch lists its copied writers: delisted before the write-back
            // (one that fails must not keep indirect dispatches on the CPU), listed after the
            // registration (a throw there leaves nothing behind).
            auto writers = DrawCopiedWriters();
            recorder->OnComplete([resources, writers] {
                writers->erase(std::remove(writers->begin(), writers->end(), resources), writers->end());
                resources->WriteBackBuffers();
            });
            writers->push_back(resources);
        } else if (completion) {
            recorder->OnComplete([resources] { resources->WriteBackBuffers(); });
        }
        for (auto& binding : targets) binding.resident->MarkDirty();
        phase(PhaseKeep);
        if (outcome.waited) {
            // The wait the dispatch path makes for such work (source 3, "address-based"): the
            // write-back runs before the next packet, so the CPU never reads copied results or
            // touches a leased allocation before they landed.
            Recorder::CountSync(3);
            const auto ownBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
            recorder->Sync();
            if (profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
            phase(PhaseSync);
        }
        report(outcome.waited ? (outcome.reason == SyncLease ? " recorded then waited (lease)" : " recorded then waited (copied writes)") : completion ? " recorded with completion" : " recorded");
        return;
    }
    {
        // The recorder sync SubmitAndWait makes is the draw's own wait, not a nested hook wait.
        const auto ownBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
        batch->SubmitAndWait();
        if (profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
    }
    phase(PhaseSync);
    APS5_LOG_CHARS_OUT_DEBUG("SubmitAndWait OK");
    for (const auto& binding : targets) GuestMemory::CheckRange(reinterpret_cast<const void*>(binding.color.address), binding.color.bytes, 256, true);
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
            binding.resident->MarkDirty();
            continue;
        }
        if (binding.gpuTiling) GuestMemory::WriteChanged(binding.color.address, binding.tiled->Bytes(), binding.original);
        else WriteColorTarget(binding.color, binding.transfer->Bytes());
        // The stored texels are the whole target now, so later reads must see them rather than a fast clear.
        MarkDccUncompressed(binding.color.dccAddress, ColorTargetLayout(binding.color.extent.width, binding.color.extent.height, binding.color.tileMode, binding.color.elementBytes).Bytes());
    }
    phase(PhaseWriteBack);
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
        phase(PhaseDescribe);
        char suffix[160];
        std::snprintf(suffix, sizeof(suffix), " synchronous (%s), first 0x%llx format %d (%zu of %zu sampled bytes nonzero)", SyncReasonNames[outcome.reason], static_cast<unsigned long long>(state.color.address), static_cast<int>(state.color.format), nonzero, sampled);
        report(suffix);
    } else {
        report(" synchronous");
    }
    APS5_LOG_CHARS_OUT_DEBUG("Draw finished");
}

}
