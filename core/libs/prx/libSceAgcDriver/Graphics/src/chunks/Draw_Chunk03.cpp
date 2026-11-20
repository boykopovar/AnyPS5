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
void countCache(std::uint64_t DrawProfile::*counter) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++(stats.*counter);
}

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
bool ValidationKey(const Context& context, std::span<const CompiledShader> shaders, const State& state, std::vector<std::uint64_t>& key) {
    using Stage = ShaderRecompiler::ShaderStage;
    static const bool disabled = std::getenv("APS5_NO_VALIDATE_CACHE") != nullptr;
    const auto add = [&](auto value) { key.push_back(static_cast<std::uint64_t>(value)); };
    bool keyed = !disabled;
    if (keyed) {
        key.reserve(40 + shaders.size() * 12);
        add(shaders.size());
        const bool patches = BarycentricPatches(state, shaders);
        add(patches ? static_cast<std::uint64_t>(state.provokingVertexMode) + 1u : 0u);
        for (const auto& shader : shaders) {
            Require(shader.program != nullptr, "missing compiled shader");
            const auto& program = *shader.program;
            const bool generated = ((state.rectList || patches) && (shader.stage == Stage::TessellationControl || shader.stage == Stage::TessellationEvaluation)) || shader.stage == Stage::Geometry;
            if (!generated && program.PipelineVariantId() == 0) {
                keyed = false;
                break;
            }
            add(shader.stage);
            add(generated ? std::uint64_t{0} : program.PipelineVariantId());
            add(KeyedPushOffset(shader));
            add(program.pushConstants.size());
            add(program.bdaAbiVersion);
            add(program.vertexAttributes.size());
            for (const auto& attribute : program.vertexAttributes) {
                add(attribute.location);
                add(attribute.components);
                add(attribute.fetchIndex);
                add(attribute.formatComponents);
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
        add(state.polygonMode);
        add(state.lineWidth);
        add(state.blends.size());
        add(context.subgroup.subgroupSize);
        add(context.subgroup.supportedStages);
        add(context.subgroup.supportedOperations);
        add(context.fragmentShaderBarycentric);
        add(context.shaderClipDistance);
        add(context.shaderCullDistance);
        add(context.viewportIndexLayer);
        add(context.shaderTerminateInvocation);
        add(state.paClVsOutCntl);
        add(state.vertexPoints);
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
            add(mesh.outputPrimitiveSize);
        }
        add(state.stages.tessellation.has_value());
        if (state.stages.tessellation) {
            const auto& tessellation = *state.stages.tessellation;
            add(tessellation.inputControlPoints);
            add(tessellation.outputControlPoints);
            add(tessellation.domain);
            add(tessellation.partitioning);
            add(tessellation.outputTopology);
            add(tessellation.patchesPerGroup);
            add(tessellation.primitiveGroupSize);
            add(tessellation.breakAtInstance ? 1u : 0u);
        }
    }
    return keyed;
}

namespace {

AgcDriver::Mutex& validationMutex() {
    static AgcDriver::Mutex mutex;
    return mutex;
}

std::map<std::vector<std::uint64_t>, std::string>& validationFailures() {
    static std::map<std::vector<std::uint64_t>, std::string> failures;
    return failures;
}

std::vector<std::uint64_t>& validationKeyScratch() {
    thread_local std::vector<std::uint64_t>* keySlot = nullptr;
    auto& key = ShaderRecompiler::ThreadOwned(keySlot);
    key.clear();
    return key;
}

struct LastFragmentOutputs {
    std::vector<std::uint64_t> key;
    std::uint32_t outputs = 0;
    bool valid = false;
    FragmentOutputLookups lookups;
};

struct LastFragmentOutputsTag {};

bool LastFragmentOutputsEnabled() {
    static const bool enabled = std::getenv("APS5_NO_LAST_FRAGMENT_OUTPUTS") == nullptr;
    return enabled;
}

std::uint32_t CachedFragmentOutputs(const Context& context, std::span<const CompiledShader> shaders, const State& state, bool& memoized, bool& hit) {
    memoized = false;
    hit = false;
    auto& key = validationKeyScratch();
    const bool keyed = ValidationKey(context, shaders, state, key);
    static std::map<std::vector<std::uint64_t>, std::uint32_t> memo;
    auto& last = ThreadScratch<LastFragmentOutputs, LastFragmentOutputsTag>();
    const bool remembers = keyed && LastFragmentOutputsEnabled();
    if (remembers && last.valid && key == last.key) {
        memoized = true;
        hit = true;
        ++last.lookups.last;
        return last.outputs;
    }
    const auto remember = [&](std::uint32_t outputs) {
        if (!remembers) return;
        last.key = key;
        last.outputs = outputs;
        last.valid = true;
    };
    if (keyed) {
        memoized = true;
        std::optional<std::uint32_t> found;
        {
            std::lock_guard lock(validationMutex());
            if (const auto entry = memo.find(key); entry != memo.end()) found = entry->second;
        }
        if (found) {
            hit = true;
            ++last.lookups.memo;
            remember(*found);
            return *found;
        }
    }
    ++last.lookups.validated;
    std::uint32_t outputs = 0;
    try {
        for (const auto location : ValidateShaders(shaders, state, context.subgroup, context.fragmentShaderBarycentric, context.descriptorIndexing, context.imageInt64Atomics, context.geometryShader, context.sampleRateShading, context.bufferInt64Atomics, context.shaderClipDistance, context.shaderCullDistance, context.viewportIndexLayer, context.runtimeDescriptorArray, context.demoteToHelperInvocation, context.shaderTerminateInvocation, context.meshShader, context.shaderResourceMinLod, context.fmaFloat16)) {
            Require(location < 32, "fragment output location exceeds the output mask");
            outputs |= 1u << location;
        }
    } catch (const std::exception& error) {
        if (keyed) {
            std::lock_guard lock(validationMutex());
            auto& failures = validationFailures();
            if (failures.size() >= 1024) failures.clear();
            failures.emplace(key, error.what());
        }
        throw;
    }
    if (keyed) {
        std::lock_guard lock(validationMutex());
        // A handful of configurations recur; a runaway key space is dropped wholesale.
        if (memo.size() >= 1024) memo.clear();
        memo.emplace(key, outputs);
    }
    remember(outputs);
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
bool MovableBuffers() {
    static const bool enabled = std::getenv("APS5_NO_MOVED_BUFFER_TEMPLATES") == nullptr;
    return enabled;
}

ResourceCache::Key DrawResourceKey(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint32_t colorAttachments, std::uint64_t indexAddress, std::uint64_t indexBytes, bool ranges) {
    ResourceCache::Key key;
    key.reserve(256);
    key.push_back(0xffffffffu);
    const auto append64 = [&](std::uint64_t value) {
        key.push_back(static_cast<std::uint32_t>(value));
        key.push_back(static_cast<std::uint32_t>(value >> 32u));
    };
    append64(reinterpret_cast<std::uint64_t>(context.device));
    key.push_back(static_cast<std::uint32_t>(shaders.size()));
    for (const auto& shader : shaders) {
        const auto sizeAt = key.size();
        key.push_back(0u);
        auto& memo = shader.program->drawKeyMemo.words;
        if (auto words = memo.load(std::memory_order_acquire); words != nullptr) {
            key.insert(key.end(), words->begin(), words->end());
        } else {
            ShaderResources::AppendContentKey(shader, true, MovableBuffers(), key);
            memo.store(std::make_shared<const std::vector<std::uint32_t>>(key.begin() + static_cast<std::ptrdiff_t>(sizeAt + 1u), key.end()), std::memory_order_release);
        }
        key[sizeAt] = static_cast<std::uint32_t>(key.size() - sizeAt - 1u);
    }
    key.push_back(colorAttachments);
    if (ranges) {
        append64(target.address | target.pipeBankXor);
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
    const auto targetAddress = target.MemoryAddress();
    for (const auto& shader : shaders) {
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
            const auto& words = binding.guestDescriptor;
            for (std::size_t offset = 0; offset + 4 <= words.size(); offset += 4) {
                const ShaderRecompiler::ShaderBufferResource descriptor{{words[offset], words[offset + 1], words[offset + 2], words[offset + 3]}};
                const auto address = descriptor.Base48();
                auto size = descriptor.GetSize();
                if (size == 0 || address == 0) continue;
                const auto element = offset / 4;
                const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                if (!written) size = ShaderResources::BytesUpToTarget(address, size, descriptor.Stride(), &target);
                Require(!overlap(address, size, targetAddress, target.bytes), "shader buffer aliases the render target");
                Require(!written || !overlap(address, size, indexAddress, indexBytes), "writable shader buffer aliases the index buffer");
            }
        }
    }
}

}

std::array<std::uint32_t, 4> MeshIndexBufferDescriptor(const Pm4::DrawParameters& draw) {
    constexpr std::uint32_t RawWord3 = 0x31016facu;
    if (!draw.indexed) return {0u, 0u, 0u, RawWord3};
    const auto address = draw.indexAddress;
    const auto bytes = (static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize + 3u) & ~std::uint64_t{3};
    Require(address != 0 && bytes != 0 && bytes <= 0xffffffffu && (address >> 48u) == 0, "invalid mesh index buffer range");
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, static_cast<std::uint32_t>(bytes), RawWord3};
}

MeshArgumentRules MeshArgumentRulesFor(const Context& context, const ShaderRecompiler::MeshConfiguration& mesh, std::uint32_t indexCount) {
    const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
    const auto step = mesh.inputPrimitive == 5 || mesh.inputPrimitive == 6 ? 1u : inputSize;
    Require(mesh.primitivesPerGroup != 0, "mesh pipeline groups no primitive (zero primitives per group)");
    return {indexCount, inputSize, step, mesh.primitivesPerGroup, context.meshLimits.maxMeshWorkGroupCount[0], context.meshLimits.maxMeshWorkGroupCount[1], context.meshLimits.maxMeshWorkGroupTotalCount};
}

std::optional<MeshGrid> MeshGridFor(const Context& context, std::uint32_t groups, std::uint32_t instances) {
    const auto& limits = context.meshLimits;
    if (groups == 0 || instances > limits.maxMeshWorkGroupCount[1] || limits.maxMeshWorkGroupCount[0] == 0) return std::nullopt;
    const auto across = groups <= limits.maxMeshWorkGroupCount[0] ? groups : std::min(limits.maxMeshWorkGroupCount[0], ShaderRecompiler::MeshLayerStrideLimit);
    const auto layers = (groups - 1u) / across + 1u;
    const auto columns = (groups - 1u) / layers + 1u;
    if (layers > limits.maxMeshWorkGroupCount[2] || (instances != 0 && columns > limits.maxMeshWorkGroupTotalCount / instances / layers)) return std::nullopt;
    return MeshGrid{columns, instances, layers, layers > 1u ? columns : 0u};
}

MeshArguments ResolveMeshArguments(const Pm4::DrawArguments& record, const MeshArgumentRules& rules) {
    const auto first = record.firstVertexOrIndex;
    const auto effective = first < rules.indexCount ? std::min(record.count, rules.indexCount - first) : 0u;
    std::uint32_t groups = 0;
    if (effective >= rules.inputSize && record.instances != 0) {
        groups = (effective - rules.inputSize) / rules.step / rules.primitivesPerGroup + 1u;
        if (groups > rules.maxGroups || record.instances > rules.maxInstances || groups > rules.maxTotal / record.instances) groups = 0;
    }
    return {groups, groups != 0 ? record.instances : 0u, groups != 0 ? 1u : 0u, effective, first};
}

std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>> DrawCopiedWriters() {
    // Never destroyed: a completion action of a batch still in flight at static teardown may erase from it.
    static auto* const writers = new std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>>(std::make_shared<std::vector<std::shared_ptr<ShaderResources>>>());
    return *writers;
}

const char* IndirectDrawPathName(IndirectDrawPath path) {
    return IndirectDrawPathNames[static_cast<std::size_t>(path)];
}

void CountIndirectDraw(IndirectDrawPath path, double readMs, bool rewritten) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++stats.indirect[static_cast<std::size_t>(path)];
    if (rewritten) ++stats.indirectRewritten;
    stats.indirectReadUs += readMs * 1000.0;
}

std::array<std::uint64_t, IndirectPathCount> IndirectDrawCounts() {
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    return stats.indirect;
}

void CountEmptyIndirectDraw(double readMs) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++stats.indirectEmpty;
    stats.indirectReadUs += readMs * 1000.0;
}

std::uint64_t EmptyIndirectDrawCount() {
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    return stats.indirectEmpty;
}

std::array<DrawPassCounts, 2> DrawPassTotals() {
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    return stats.passTotals;
}

std::array<std::uint64_t, PassBreakCount> DrawPassBreaks() {
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    return stats.passBreaks;
}

void CountDrawSkip(DrawSkip kind, double us) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++stats.skips[static_cast<std::size_t>(kind)];
    stats.skipUs[static_cast<std::size_t>(kind)] += us;
}


bool DrawRecipes() {
    static const bool noDrawRecipe = std::getenv("APS5_NO_DRAW_RECIPE") != nullptr;
    return !noDrawRecipe;
}

const char* DrawRecipeMissName(DrawRecipeMiss miss) {
    constexpr std::array<const char*, static_cast<std::size_t>(DrawRecipeMiss::Count)> names{"none", "not recordable", "target gone", "template gone", "objects gone", "proof", "buffers"};
    return names[static_cast<std::size_t>(miss)];
}

namespace {

// APS5_PROFILE_DRAW: the per-phase timers of one draw (DrawPhase), shared by Draw's parts and
// DrawWithRecipe.
struct DrawTimer {
    bool profile;
    std::chrono::steady_clock::time_point phaseStart;
    std::array<double, PhaseCount> us{};
    explicit DrawTimer(bool profile) : profile(profile), phaseStart(std::chrono::steady_clock::now()) {}
    void phase(DrawPhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        us[which] += std::chrono::duration<double, std::micro>(now - phaseStart).count();
        phaseStart = now;
    }
    void charge(DrawPhase which, std::chrono::steady_clock::time_point start) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        us[which] += std::chrono::duration<double, std::micro>(now - start).count();
        phaseStart += now - start;
    }
};

// One line per draw with the phases that took time (APS5_TRACE_DRAWS), and the 10 s totals.
// `waitedBefore` is the thread's GPU waits at the draw's start and `ownWaitedMs` the draw's own
// syncs (see DrawOutcome::hookWaitUs).
void reportDrawEnd(const State& state, const DrawTimer& timer, const ShaderResources::BuildTiming* built, DrawOutcome& outcome, double waitedBefore, double ownWaitedMs, const char* suffix) {
    if (!timer.profile) return;
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    outcome.hookWaitUs = std::max(0.0, Recorder::ThreadWaitedMs() - waitedBefore - ownWaitedMs) * 1000.0;
    outcome.ownSyncUs = ownWaitedMs * 1000.0;
    if (traceDraws) {
        char line[512];
        int n = std::snprintf(line, sizeof(line), "[draw] %ux%u %zu targets%s:", state.renderExtent.width, state.renderExtent.height, state.colors.size(), suffix);
        for (std::size_t i = 0; i < PhaseCount && n > 0 && static_cast<std::size_t>(n) < sizeof(line); ++i) {
            if (timer.us[i] <= 0) continue;
            n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s=%.0fus", DrawPhaseNames[i], timer.us[i]);
        }
        std::fprintf(stderr, "%s\n", line);
    }
    reportDraw(timer.us, built, outcome);
}

}

bool BindsIndexCopy(const State& state) {
    return !state.stages.mesh;
}

DrawInputCopy CopyDrawInput(const Context& context, Recorder* recorder, std::uint64_t address, std::size_t bytes, std::size_t alignment, Recorder::SnapshotUse use, std::size_t from, bool hostOnly) {
    Require(use != Recorder::SnapshotUse::Storage, "a draw input is a vertex or index buffer");
    Require(from == 0 || (use == Recorder::SnapshotUse::Vertex && from < bytes), "only a vertex input starts past its address");
    DrawInputCopy copy;
    copy.from = from;
    if (recorder != nullptr && bytes != 0) {
        GuestMemory::FlushGpuWrites(address, bytes);
#ifdef _WIN32
        const bool owned = !RuntimeSwitches::Set(RuntimeSwitch::NoOwnedDrawInputs);
#else
        constexpr bool owned = false;
#endif
        if (owned && recorder->OwnedDrawInputEntries() != 0) copy.buffer = recorder->ReusableOwnedDrawInput(address, bytes, use, &copy.derived, from, &copy.lowest);
        if (copy.buffer != nullptr) {
            copy.reused = true;
            return copy;
        }
        copy.registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        copy.generation = GuestMemory::CollectWrites(address, bytes);
        if (copy.generation != 0) copy.buffer = recorder->ReusableDrawSnapshot(address, bytes, use, &copy.derived, 0, nullptr, from, &copy.lowest, !hostOnly);
        else if (owned) copy.owned = recorder->ArmOwnedDrawInput(address, bytes);
        if (copy.buffer != nullptr) {
            copy.reused = true;
            return copy;
        }
    }
    copy.buffer = std::make_shared<Buffer>(context, bytes, (use == Recorder::SnapshotUse::Vertex ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    static const bool poisonPrefix = std::getenv("APS5_POISON_VERTEX_PREFIX") != nullptr;
    const auto held = copy.buffer->Bytes();
    if (poisonPrefix) std::fill(held.begin(), held.begin() + static_cast<std::ptrdiff_t>(from), std::byte{0x7f});
    GuestMemory::Read(address + from, held.subspan(from), alignment);
    return copy;
}

}
}
