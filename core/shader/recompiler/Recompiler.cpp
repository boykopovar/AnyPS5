#include "Recompiler.hpp"
#include "StencilReplay.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include "CacheKey.hpp"
#include "CompiledVariant.hpp"
#include "VertexInputSpecialization.hpp"
#include "FragmentOutputSpecialization.hpp"
#include "SpirvBackend/SpirvSpecialization.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "ShaderDiskCache.hpp"
#include <list>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <unordered_map>
#include "ControlFlow/include/ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/include/ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/include/Optimization/BindingAllocator.hpp"
#include "Optimization/include/Optimization/ConstantFolder.hpp"
#include "Optimization/include/Optimization/DeadCodeEliminator.hpp"
#include "Optimization/include/Optimization/DenormalFlushEliminator.hpp"
#include "Optimization/include/Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/include/Optimization/HostInterpolationChecker.hpp"
#include "Optimization/include/Optimization/MaskedSelectEliminator.hpp"
#include "Optimization/include/Optimization/ReadLaneEliminator.hpp"
#include "Optimization/include/Optimization/RedundantValueEliminator.hpp"
#include "Optimization/include/Optimization/RequestMemoryView.hpp"
#include "Optimization/include/Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/include/Optimization/ResourceTracker.hpp"
#include "Optimization/include/Optimization/VectorWindowStoreEliminator.hpp"
#include "Optimization/include/Optimization/ShaderInfoCollector.hpp"
#include "Optimization/include/Optimization/SrtWalker.hpp"
#include "Optimization/include/Optimization/SsaBuilder.hpp"
#include "Optimization/include/Optimization/TessellationLowering.hpp"
#include "SpirvBackend/include/SpirvBackend/SpirvEmitter.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "SpirvBackend/SpirvMemory/SpirvInputOutput.hpp"
#include "Translation/include/Translation/InstructionTranslator.hpp"
#include "Translation/include/Translation/ShaderInputInfoBuilder.hpp"
#include <exception>
#include <stdexcept>
#include <string>
#include <ControlFlow/RequestSerializer.hpp>

namespace ShaderRecompiler {

namespace {

ShaderStageKind toShaderStageKind(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Compute:
        return ShaderStageKind::Compute;
    case ShaderStage::Vertex:
        return ShaderStageKind::Vertex;
    case ShaderStage::TessellationControl:
        return ShaderStageKind::TessellationControl;
    case ShaderStage::TessellationEvaluation:
        return ShaderStageKind::TessellationEvaluation;
    case ShaderStage::Fragment:
        return ShaderStageKind::Pixel;
    case ShaderStage::Local:
        return ShaderStageKind::Local;
    case ShaderStage::Mesh:
        return ShaderStageKind::Mesh;
    case ShaderStage::Geometry:
        break;
    }
    throw std::runtime_error("ShaderRecompiler::Recompile: unsupported shader stage");
}

}

namespace {

bool MergedHullStage(const RecompileRequest& request) {
    const auto stage = request.shader.stage;
    return (stage == ShaderStage::Local || stage == ShaderStage::TessellationControl) && request.graphics && request.graphics->tessellation && request.graphics->tessellation->mergedHull;
}

// The host subgroup width wave64 programs are laid out for. Debug aid: APS5_SINGLE_LANE=<hex code
// addresses, comma separated, or "all"> keeps the listed programs at one guest lane per invocation.
std::uint32_t HostSubgroupSize(const RecompileRequest& request) {
    const bool vertexStage = request.shader.stage == ShaderStage::Vertex || request.shader.stage == ShaderStage::Local;
    if (vertexStage && !request.target.vertexSubgroups) return 1u;
    if (request.shader.stage == ShaderStage::TessellationControl && MergedHullStage(request)) return 1u;
    static const std::string list = [] { const char* text = std::getenv("APS5_SINGLE_LANE"); return text ? std::string(text) : std::string(); }();
    if (!list.empty()) {
        if (list == "all") return 64u;
        char address[32];
        std::snprintf(address, sizeof(address), "%llx", static_cast<unsigned long long>(request.shader.codeAddress));
        if (list.find(address) != std::string::npos) return 64u;
    }
    return request.target.subgroupSize;
}

RdnaProgram DecodeStageProgram(ShaderStageKind stage, std::span<const std::uint32_t> code, bool mergedHull) {
    static constexpr std::array<std::uint32_t, 1> EndProgram{0xbf810000u};
    if (stage == ShaderStageKind::Local && mergedHull) {
        RdnaProgram program;
        program.code = EndProgram;
        program.instructions.push_back(DecodeRdnaInstruction(0u, EndProgram, 0u));
        return program;
    }
    if (stage != ShaderStageKind::Local) return RdnaInstructionDecoder{}.Decode(code);
    auto program = DecodeRdnaFrontProgram(code);
    program.instructions.back() = DecodeRdnaInstruction(program.instructions.back().programCounter, EndProgram, 0u);
    return program;
}

ShaderStageInputInfo RequestInputInfo(const RecompileRequest& request) {
    const auto* mesh = request.graphics && request.graphics->mesh ? &*request.graphics->mesh : nullptr;
    std::optional<TessellationConfiguration> tessellationCopy;
    if (request.graphics && request.graphics->tessellation) {
        tessellationCopy = *request.graphics->tessellation;
        tessellationCopy->instancePatches = TessellationInstancePatches(request);
        const auto& capabilities = request.target.supportedCapabilities;
        tessellationCopy->drawParameters = std::find(capabilities.begin(), capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityDrawParameters)) != capabilities.end();
    }
    const auto* tessellation = tessellationCopy ? &*tessellationCopy : nullptr;
    return BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context, HostSubgroupSize(request), mesh, tessellation);
}

void ValidateRequestInputs(const RecompileRequest& request) {
    const auto stage = toShaderStageKind(request.shader.stage);
    const bool vertexFamily = stage == ShaderStageKind::Vertex || stage == ShaderStageKind::Local || stage == ShaderStageKind::TessellationControl || stage == ShaderStageKind::TessellationEvaluation || stage == ShaderStageKind::Mesh;
    if (!vertexFamily) {
        static_cast<void>(RequestInputInfo(request));
        return;
    }
    const auto* mesh = request.graphics && request.graphics->mesh ? &*request.graphics->mesh : nullptr;
    const auto* tessellation = request.graphics && request.graphics->tessellation ? &*request.graphics->tessellation : nullptr;
    ValidateVertexStageInputs(stage, request.context, mesh, tessellation);
}

std::uint32_t DeviceMemoryLdsBytes(const RecompileRequest& request) {
    if (!request.context.compute.has_value()) return 0u;
    const auto bytes = static_cast<std::uint64_t>(request.context.compute->ldsSizeDwords) * 4u;
    return bytes + 4u > request.target.maxWorkgroupSharedMemoryBytes ? static_cast<std::uint32_t>(bytes) : 0u;
}

std::atomic<std::uint64_t> frontEndBuilds{0};

}

struct PreparedControlFlow {
    PreparedControlFlow(const RecompileRequest& request, const SwappcInfo& swappcInfo)
        : stage(request.shader.stage), mergedHull(MergedHullStage(request)), fallbacks(swappcInfo.linkedCallFallbacks.begin(), swappcInfo.linkedCallFallbacks.end()), swappc(WithFallbacks(swappcInfo, fallbacks)), code(request.shader.code.begin(), request.shader.code.end()),
          decoded(DecodeStageProgram(toShaderStageKind(request.shader.stage), code, mergedHull)), cfg(GraphBuilder{}.Build(decoded, &swappc)) {
        Structurizer{}.Structurize(cfg);
    }

    PreparedControlFlow(const PreparedControlFlow&) = delete;
    PreparedControlFlow& operator=(const PreparedControlFlow&) = delete;

    bool Matches(const RecompileRequest& request, const SwappcInfo& swappcInfo) const {
        return stage == request.shader.stage && mergedHull == MergedHullStage(request) && swappc.fetchCallAllowed == swappcInfo.fetchCallAllowed &&
               swappc.userDataBaseRegister == swappcInfo.userDataBaseRegister && swappc.userDataCount == swappcInfo.userDataCount && swappc.waveSize == swappcInfo.waveSize &&
               std::ranges::equal(fallbacks, swappcInfo.linkedCallFallbacks) && std::ranges::equal(code, request.shader.code);
    }

    static SwappcInfo WithFallbacks(SwappcInfo info, const std::vector<std::uint32_t>& fallbacks) {
        info.linkedCallFallbacks = fallbacks;
        return info;
    }

    ShaderStage stage;
    bool mergedHull;
    std::vector<std::uint32_t> fallbacks;
    SwappcInfo swappc;
    std::vector<std::uint32_t> code;
    RdnaProgram decoded;
    ControlFlowGraph cfg;
};

std::shared_ptr<const PreparedControlFlow> ShaderPreparationContext::AcquireFrontend(const RecompileRequest& request) {
    const auto inputInfo = RequestInputInfo(request);
    const SwappcInfo swappc{inputInfo.vertex != nullptr, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()), request.context.waveSize, request.context.linkedCallFallbacks};
    static const bool reuse = std::getenv("APS5_NO_PERF_FRONTEND_PAIR") == nullptr;
    if (reuse && frontend != nullptr && frontend->Matches(request, swappc)) {
        return frontend;
    }
    auto prepared = std::make_shared<const PreparedControlFlow>(request, swappc);
    if (reuse) {
        frontend = prepared;
    }
    return prepared;
}

std::uint64_t FrontEndBuilds() {
    return frontEndBuilds.load(std::memory_order_relaxed);
}

std::uint32_t ShaderPreparationContext::ResourcePrograms() const {
    return resourcePrograms;
}

IrProgram PrepareResourceProgram(const RecompileRequest& request) {
    return PrepareResourceProgram(request, nullptr);
}

IrProgram PrepareResourceProgram(const RecompileRequest& request, ShaderPreparationContext* preparation) {
    frontEndBuilds.fetch_add(1, std::memory_order_relaxed);
    ShaderPreparationContext local;
    auto& context = preparation != nullptr ? *preparation : local;
    ++context.resourcePrograms;
    context.interfaceSource.reset();
    context.interfaceProgram.reset();
    const auto frontend = context.AcquireFrontend(request);
    const auto stageKind = toShaderStageKind(request.shader.stage);
    const auto inputInfo = RequestInputInfo(request);
    const auto& decoded = frontend->decoded;
    const auto& cfg = frontend->cfg;

    TranslateOptions translateOptions {};
    translateOptions.stage = stageKind;
    translateOptions.shaderHash = request.shader.codeAddress;
    translateOptions.waveSize = request.context.waveSize;
    translateOptions.userDataBaseRegister = request.context.userDataBaseRegister;
    translateOptions.gcnLegacyVertexInputs = GcnGuestShaders() && request.context.userDataBaseRegister == 0u;
    translateOptions.userDataCount = static_cast<std::uint32_t>(request.context.userData.size());
    translateOptions.scratchDwords = request.context.compute.has_value() ? request.context.compute->scratchDwords : 0u;
    translateOptions.sharedMemoryBytes = DeviceMemoryLdsBytes(request);
    translateOptions.fragmentShaderBarycentricEnabled = request.target.fragmentShaderBarycentricEnabled;
    const auto supports = [&](spv::Capability capability) { return std::ranges::find(request.target.supportedCapabilities, static_cast<std::uint32_t>(capability)) != request.target.supportedCapabilities.end(); };
    translateOptions.nativeFloat16 = supports(spv::CapabilityFloat16) && supports(spv::CapabilityRoundingModeRTE) && supports(spv::CapabilityDenormPreserve);
    translateOptions.interpolationFunctions = supports(spv::CapabilityInterpolationFunction);
    translateOptions.floatMode = request.context.floatMode;
    translateOptions.float32FlushToZero = request.target.float32FlushToZero;
    translateOptions.inputInfo = inputInfo;

    constexpr InstructionTranslator translator;

    if (frontend->mergedHull && cfg.hasFetchCall) throw std::runtime_error("a merged hull program that calls a fetch subroutine is unsupported");
    EmbeddedFetchPlan embeddedFetch;
    if ((stageKind == ShaderStageKind::Vertex || stageKind == ShaderStageKind::Local) && !frontend->mergedHull && inputInfo.vertex != nullptr && inputInfo.vertex->fetchEmbedded) {
        embeddedFetch = EmbeddedVertexFetchAnalyzer{}.Analyze(decoded, inputInfo.vertex->fetchAttribReg, inputInfo.vertex->fetchBufferReg, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()), request.context.waveSize);
        translateOptions.embeddedFetch = &embeddedFetch;
    }
    auto program = translator.Translate(decoded, cfg, translateOptions);
    // Debug aid: APS5_DUMP_IR=<hex code address> (or "all") prints the program after each front-end pass.
    const auto dumpIr = [&](const char* pass) {
        static const std::string list = [] { const char* text = std::getenv("APS5_DUMP_IR"); return text ? std::string(text) : std::string(); }();
        if (list.empty()) return;
        char address[32];
        std::snprintf(address, sizeof(address), "%llx", static_cast<unsigned long long>(request.shader.codeAddress));
        if (list != "all" && list.find(address) == std::string::npos) return;
        std::fprintf(stderr, "==== IR 0x%s after %s\n%s\n", address, pass, ProgramToString(program).c_str());
    };

    constexpr SsaBuilder ssaBuilder;
    constexpr ConstantFolder constantFolder;
    constexpr DeadCodeEliminator deadCodeEliminator;
    constexpr ReadLaneEliminator readLaneEliminator;
    constexpr MaskedSelectEliminator maskedSelectEliminator;
    constexpr VectorWindowStoreEliminator vectorWindowStoreEliminator;
    const auto simplify = [&] {
        dumpIr("translate");
        ssaBuilder.Rewrite(program);
        dumpIr("ssa");

        constantFolder.Fold(program);
        ResolveControlFlowIdentities(program);
        deadCodeEliminator.RemoveIdentities(program);
        deadCodeEliminator.Eliminate(program);
        dumpIr("fold");

        const auto readLaneStats = readLaneEliminator.Eliminate(program, translateOptions.waveSize);
        if (readLaneStats.rewrittenReads != 0u) {
            constantFolder.Fold(program);
            ResolveControlFlowIdentities(program);
            deadCodeEliminator.RemoveIdentities(program);
            deadCodeEliminator.Eliminate(program);
        }

        if (vectorWindowStoreEliminator.Eliminate(program).removedStores != 0u) {
            deadCodeEliminator.Eliminate(program);
        }

        if (maskedSelectEliminator.Eliminate(program).removedSelects != 0u) {
            const bool liveAncillary = std::ranges::any_of(program.Blocks(), [](const auto& block) {
                return std::ranges::any_of(block->Instructions(), [](const IrValue* value) {
                    return value->Opcode() == IrOpcode::GetBuiltin && value->HasUses() && value->Argument(0)->Resolve()->ImmediateU32() == static_cast<std::uint32_t>(StageInputKind::PackedAncillary);
                });
            });
            if (liveAncillary) {
                constantFolder.Fold(program);
                ResolveControlFlowIdentities(program);
                deadCodeEliminator.RemoveIdentities(program);
            }
            deadCodeEliminator.Eliminate(program);
        }
        if (constantFolder.ComposePackedAncillary(program)) {
            deadCodeEliminator.Eliminate(program);
        }
    };
    simplify();
    if (stageKind == ShaderStageKind::Pixel && !translateOptions.fragmentShaderBarycentricEnabled && !HostInterpolationChecker{}.Lower(program, *inputInfo.pixel)) {
        translateOptions.fragmentShaderBarycentricEnabled = true;
        program = translator.Translate(decoded, cfg, translateOptions);
        program.Metadata().barycentricEmulation = true;
        simplify();
    }

    constexpr DenormalFlushEliminator denormalFlushEliminator;
    if (denormalFlushEliminator.Eliminate(program).removedFlushes != 0u) {
        deadCodeEliminator.Eliminate(program);
    }

    if (inputInfo.vertex != nullptr) {
        constexpr TessellationLowering tessellationLowering;
        tessellationLowering.Lower(program, inputInfo.vertex->tess, request.target.tessellation ? &*request.target.tessellation : nullptr);
    }

    deadCodeEliminator.EliminateUnobservedPhis(program);
    deadCodeEliminator.Eliminate(program);
    constexpr SrtWalker srtWalker;
    srtWalker.BuildPlan(program);
    deadCodeEliminator.Eliminate(program);
    dumpIr("srt");

    constexpr ResourceTracker resourceTracker;
    resourceTracker.Track(program);
    deadCodeEliminator.Eliminate(program);
    dumpIr("resources");
    program.Resources().srgbDecodeFormats = request.target.srgbDecodeFormats;
    if (request.context.convertedSamples) {
        for (auto& image : program.Resources().info.images) image.convertedSamples = true;
    }

    return program;
}

// A materialized result of one variant over one snapshot (Recompile(request, capture)): the
// shared immutable object every later capture that reproduces the snapshot receives, so Populate
// and the per-request copy run once per distinct snapshot.
struct ResultMemoEntry {
    std::uint64_t variantId = 0;
    std::uint64_t hash = 0;
    std::shared_ptr<const RecompileResult> result;
    std::uint64_t used = 0;
    std::uint64_t bytes = 0;
};

struct EmissionFailure {
    std::uint64_t codeAddress;
    BindingLayout layout;
    std::exception_ptr failure;
};

constexpr std::size_t ResultMemoSets = 64;
constexpr std::size_t ResultMemoWays = 4;
using ResultMemoTable = std::array<ResultMemoEntry, ResultMemoSets * ResultMemoWays>;

struct SourceEntry : std::enable_shared_from_this<SourceEntry> {
    std::mutex mutex;
    // The code the entry was built for: the key carries only a hash of it, so a candidate entry is
    // accepted only when its code matches word for word. Owned here because the request's span
    // points into a registration the driver may replace while the entry lives on.
    std::vector<std::uint32_t> code;
    std::vector<std::uint32_t> linkedCallFallbacks;
    std::shared_ptr<const IrResourcePlan> plan;
    bool runtimeCaptured = false;
    // A plan build that threw (an unsupported resource chain or control flow) is remembered and
    // rethrown: the front end ran every pass before failing, ~13 ms per dispatch of a shader the
    // title issues every frame (0x1048947300 at the intro video). APS5_NO_FAILURE_MEMO=1 rebuilds.
    std::exception_ptr planFailure;
    std::unique_ptr<IrProgram> program;
    std::vector<std::shared_ptr<const CompiledVariant>> variants;
    std::vector<EmissionFailure> emissionFailures;
    std::unique_ptr<ResultMemoTable> memo;
};

namespace {

class PrimedSharedMutex : public std::shared_mutex {
public:
    PrimedSharedMutex() {
        lock();
        unlock();
    }
};

struct ResourceProgram {
    explicit ResourceProgram(const RecompileRequest& request, ShaderPreparationContext* preparation = nullptr, bool reclaim = true) : program(std::make_unique<IrProgram>(PrepareResourceProgram(request, preparation))) {
        const auto clearRegisterState = [](const auto& storage) {
            for (const auto& block : storage) {
                block->ssaScalarValues.fill(nullptr);
                block->ssaVectorValues.fill(nullptr);
                block->ssaThreadBitScalarValues.fill(nullptr);
                block->ssaScalarMaskTags.fill(nullptr);
            }
        };
        clearRegisterState(program->Blocks());
        clearRegisterState(program->Resources().blockStorage);
        if (reclaim) program->ReclaimUnusedValues();
        plan = std::make_shared<const IrResourcePlan>(ResourceMaterializer{}.ExtractPlan(*program));
    }

    std::unique_ptr<IrProgram> program;
    std::shared_ptr<const IrResourcePlan> plan;
};

std::shared_ptr<const IrResourcePlan> makeResourcePlan(const RecompileRequest& request) {
    return ResourceProgram(request, nullptr, false).plan;
}

struct SourceKeyHash {
    std::size_t operator()(const std::vector<std::uint64_t>& key) const {
        std::size_t hash = 0;
        for (const auto value : key) {
            hash ^= static_cast<std::size_t>(value) + static_cast<std::size_t>(0x9e3779b97f4a7c15ull) + (hash << 6u) + (hash >> 2u);
            if constexpr (sizeof(std::size_t) < sizeof(value)) hash ^= static_cast<std::size_t>(value >> 32u);
        }
        return hash;
    }
};

bool FailureMemo() {
    static const bool memo = std::getenv("APS5_NO_FAILURE_MEMO") == nullptr;
    return memo;
}

bool diskPlans() {
    static const bool enabled = [] {
        const char* dumpIr = std::getenv("APS5_DUMP_IR");
        return std::getenv("APS5_NO_PERF_DISK_PLANS") == nullptr && (dumpIr == nullptr || *dumpIr == '\0');
    }();
    return enabled && ShaderDiskCache::Enabled() && !DebugProbeActive();
}

bool sameLayout(const BindingLayout& left, const BindingLayout& right);

std::shared_ptr<SourceEntry> getSource(const RecompileRequest& request, ShaderPreparationContext* preparation = nullptr, bool reclaim = true, const SourceHandle* preparedHandle = nullptr) {
    static PrimedSharedMutex mutex;
    // Entries whose code hashes alike share a bucket; the code comparison picks the right one.
    static std::unordered_map<std::vector<std::uint64_t>, std::vector<std::shared_ptr<SourceEntry>>, SourceKeyHash> sources;
    struct SourceKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, SourceKeyStorage>();
    RecompileCacheKey::Build(request, key);
    key.push_back(HostSubgroupSize(request));
    const auto find = [&]() -> std::shared_ptr<SourceEntry> {
        const auto found = sources.find(key);
        if (found == sources.end()) return nullptr;
        for (const auto& entry : found->second) {
            if (std::equal(entry->code.begin(), entry->code.end(), request.shader.code.begin(), request.shader.code.end())) return entry;
        }
        return nullptr;
    };
    std::shared_ptr<SourceEntry> source;
    {
        std::shared_lock lock(mutex);
        source = find();
    }
    if (source == nullptr) {
        std::unique_lock lock(mutex);
        source = find();
        if (source == nullptr) {
            source = std::make_shared<SourceEntry>();
            source->code.assign(request.shader.code.begin(), request.shader.code.end());
            source->linkedCallFallbacks.assign(request.context.linkedCallFallbacks.begin(), request.context.linkedCallFallbacks.end());
            auto& bucket = sources[key];
            if (!bucket.empty()) {
                // A second entry under one key is a code hash collision (or the unhashed key with
                // identical code, which cannot happen); each is reported under the profile switch.
                static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
                static std::uint64_t collisions = 0;
                ++collisions;
                if (profile) std::fprintf(stderr, "[recompile] source key collision %llu: %zu entries share a key (%zu code words)\n", static_cast<unsigned long long>(collisions), bucket.size() + 1, request.shader.code.size());
            }
            bucket.push_back(source);
        }
    }
    {
        std::lock_guard lock(source->mutex);
        if (source->plan == nullptr) {
            const bool hasPreparedArtifact = preparedHandle != nullptr && preparedHandle->artifact != nullptr;
            if (hasPreparedArtifact && (preparedHandle->source != source || !MatchesPreparedShader(request, *preparedHandle))) throw std::runtime_error("ShaderRecompiler: prepared artifact does not match the resource plan source");
            if (FailureMemo() && source->planFailure) std::rethrow_exception(source->planFailure);
            const bool disk = diskPlans();
            std::vector<std::byte> planKey;
            if (disk) {
                ShaderDiskCache::BuildPlanKey(request, HostSubgroupSize(request), planKey);
                IrResourcePlan loaded;
                if (ShaderDiskCache::LoadPlan(planKey, loaded)) {
                    loaded.shaderHash = request.shader.codeAddress;
                    source->plan = std::make_shared<const IrResourcePlan>(std::move(loaded));
                }
            }
            if (source->plan == nullptr) {
                try {
                    ResourceProgram resource(request, preparation, reclaim);
                    source->plan = std::move(resource.plan);
                    if (!hasPreparedArtifact && std::ranges::none_of(source->variants, [&](const auto& variant) { return sameLayout(variant->layout, request.layout); })) {
                        source->program = std::move(resource.program);
                    }
                } catch (...) {
                    if (FailureMemo()) source->planFailure = std::current_exception();
                    throw;
                }
                if (disk) ShaderDiskCache::StorePlan(std::move(planKey), source->plan);
            }
        }
    }
    return source;
}

std::shared_ptr<const IrResourcePlan> acquirePlan(const RecompileRequest& request, const std::shared_ptr<SourceEntry>& source, bool runtimeCapture, const SourceHandle* preparedHandle = nullptr) {
    for (;;) {
        {
            std::lock_guard lock(source->mutex);
            if (source->plan != nullptr) {
                if (runtimeCapture) source->runtimeCaptured = true;
                return source->plan;
            }
        }
        if (getSource(request, nullptr, true, preparedHandle) != source) throw std::runtime_error("ShaderRecompiler: resource plan source changed while reloading");
    }
}

void releaseUndrawnPlan(const SourceHandle& handle) {
    if (handle.source == nullptr || !diskPlans()) return;
    std::lock_guard lock(handle.source->mutex);
    if (!handle.source->runtimeCaptured) handle.source->plan.reset();
}

std::array<std::uint32_t, 3> partialThreads(const RecompileRequest& request) {
    return request.context.compute ? request.context.compute->partialThreads : std::array<std::uint32_t, 3>{};
}

std::uint64_t nextVariantId() {
    static std::atomic<std::uint64_t> variants{0};
    return variants.fetch_add(1, std::memory_order_relaxed) + 1;
}

template<typename TStep>
auto recordEmissionFailure(std::exception_ptr* emissionFailure, TStep&& step) -> decltype(step()) {
    try {
        return step();
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        if (emissionFailure != nullptr) *emissionFailure = std::current_exception();
        throw;
    }
}

void prepareInterface(const RecompileRequest& request, IrProgram& program) {
    const auto inputInfo = RequestInputInfo(request);
    constexpr DeadCodeEliminator deadCodeEliminator;
    constexpr ResourceMaterializer resourceMaterializer;
    const bool nativeSampleOffsets = request.target.nonConstantImageOffsets && std::find(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end(), spv::CapabilityImageGatherExtended) != request.target.supportedCapabilities.end();
    resourceMaterializer.ApplyStaticInterface(program, nativeSampleOffsets);

    constexpr ConstantFolder constantFolder;
    if (request.shader.stage == ShaderStage::Vertex && constantFolder.FoldVertexLaneBounds(program, HostSubgroupSize(request), request.target.spirvVersion)) {
        constantFolder.Fold(program);
        ResolveControlFlowIdentities(program);
    }

    deadCodeEliminator.RemoveIdentities(program);
    constexpr RedundantValueEliminator redundantValueEliminator;
    static_cast<void>(redundantValueEliminator.Eliminate(program));
    deadCodeEliminator.Eliminate(program);

    constexpr ShaderInfoCollector shaderInfoCollector;
    shaderInfoCollector.Collect(program, inputInfo);
}

CompiledVariant emitVariant(const RecompileRequest& request, IrProgram& program) {
    const auto inputInfo = RequestInputInfo(request);
    constexpr BindingAllocator bindingAllocator;
    bool registration = request.target.registrationModules && (request.shader.stage == ShaderStage::Vertex || request.shader.stage == ShaderStage::Fragment);
    program.Metadata().bindingLayoutComplete = false;
    const bool stencilReplay = request.shader.stage == ShaderStage::Fragment && std::ranges::any_of(program.Info().outputs, [](const StageOutput& output) { return output.kind == StageOutputKind::StencilRef; }) && std::ranges::find(request.target.supportedCapabilities, static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT)) == request.target.supportedCapabilities.end();
    auto bindings = bindingAllocator.Allocate(program, request.layout, registration, stencilReplay);

    SpirvTargetOptions targetOptions {};
    targetOptions.vulkanVersion = request.target.vulkanVersion;
    targetOptions.spirvVersion = request.target.spirvVersion;
    targetOptions.subgroupSize = HostSubgroupSize(request) == 1u ? 1u : request.target.subgroupSize;
    targetOptions.bdaAbiVersion = request.target.bdaAbiVersion;
    targetOptions.supportedCapabilities = request.target.supportedCapabilities;
    targetOptions.supportedExtensions = request.target.supportedExtensions;
    targetOptions.nonConstantImageOffsets = request.target.nonConstantImageOffsets;
    targetOptions.narrowSubgroupClock = request.target.narrowSubgroupClock;
    targetOptions.float64DenormPreserve = request.target.float64DenormPreserve;
    targetOptions.depthRangeRemap = request.target.depthRangeRemap;
    targetOptions.registrationModules = registration;
    targetOptions.fragmentDescriptorSet = request.target.registrationModules;
    targetOptions.float16DenormPreserve = request.target.float16DenormPreserve;
    targetOptions.spannedWaves = request.target.spannedWaves;
    targetOptions.robustBufferAccess2 = request.target.robustBufferAccess2;
    targetOptions.float32FlushToZero = request.target.float32FlushToZero;
    targetOptions.float32DenormPreserve = request.target.float32DenormPreserve;
    targetOptions.fusedExactArithmetic = request.target.fusedExactArithmetic;
    targetOptions.largePixelFunctionLdsBuffer = request.target.largePixelFunctionLdsBuffer;
    targetOptions.pixelFunctionStorageDwords = request.target.pixelFunctionStorageDwords;
    targetOptions.meshBarycentricPixel = request.shader.stage == ShaderStage::Fragment && request.graphics && request.graphics->mesh;
    if (request.shader.stage == ShaderStage::Mesh && request.graphics && request.graphics->meshBarycentrics) {
        const auto& paired = *request.graphics->meshBarycentrics;
        const auto emulated = DescribeEmulatedInterface(paired.parameters, paired.emulation, true);
        const auto& layout = emulated.layout;
        targetOptions.meshBarycentricVertex = true;
        targetOptions.barycentricSmoothLocation = layout.smoothLocation;
        targetOptions.barycentricLinearLocation = layout.linearLocation;
        targetOptions.barycentricPositionsLocation = layout.positionsLocation;
        targetOptions.meshExportedLocations = emulated.passed;
        for (const auto& parameter : emulated.perVertex) targetOptions.meshPerVertexLocations.emplace_back(parameter.sourceLocation, parameter.location);
        if (!request.graphics->mesh || !request.target.mesh) throw std::runtime_error("barycentric emulation on the mesh path needs a mesh configuration and a device with mesh shaders");
        const auto& config = *request.graphics->mesh;
        const auto& limits = *request.target.mesh;
        if (config.provokingVertex != 0u) throw std::runtime_error("barycentric emulation on the mesh path is only measured with the first provoking vertex");
        const std::uint64_t perVertexParameters = emulated.perVertex.size();
        const std::uint64_t sharedBytes = 16ull * config.maxVertices * (1u + perVertexParameters) + DeviceMemoryLdsBytes(request);
        if (sharedBytes > limits.maxSharedMemoryBytes) throw std::runtime_error("barycentric emulation on the mesh path needs " + std::to_string(sharedBytes) + " bytes of mesh shared memory for " + std::to_string(config.maxVertices) + " vertices, the device allows " + std::to_string(limits.maxSharedMemoryBytes));
        MeshOutputUse use;
        std::set<std::uint32_t> flatLocations;
        for (const auto& output : program.Info().outputs) {
            if (output.kind == StageOutputKind::Parameter && std::ranges::find(targetOptions.meshExportedLocations, output.location) != targetOptions.meshExportedLocations.end()) flatLocations.insert(output.location);
            if (output.kind == StageOutputKind::ClipDistance) use.clipDistances = std::max(use.clipDistances, output.index + 1u);
            if (output.kind == StageOutputKind::CullDistance) use.cullDistances = std::max(use.cullDistances, output.index + 1u);
            if (output.kind == StageOutputKind::Layer) use.layer = true;
        }
        use.flatParameters = static_cast<std::uint32_t>(flatLocations.size());
        use.perVertexParameters = static_cast<std::uint32_t>(perVertexParameters);
        use.clipVaryings = emulated.barycentrics;
        const auto budget = MeshBarycentricOutputBudget(config, limits, use);
        if (budget.components > limits.maxOutputComponents) throw std::runtime_error("barycentric emulation on the mesh path needs " + std::to_string(budget.components) + " mesh output components (" + std::to_string(budget.vertexLocations) + " per-vertex and " + std::to_string(budget.primitiveLocations) + " per-primitive locations), the device allows " + std::to_string(limits.maxOutputComponents));
        if (budget.bytes > limits.maxOutputMemoryBytes) throw std::runtime_error("barycentric emulation on the mesh path needs " + std::to_string(budget.bytes) + " bytes of mesh output memory, the device allows " + std::to_string(limits.maxOutputMemoryBytes));
    }
    targetOptions.waveLdsMemoryBarriers = request.target.waveLdsMemoryBarriers;
    targetOptions.deviceScopeLdsMemoryBarriers = request.target.deviceScopeLdsMemoryBarriers;

    constexpr SpirvEmitter spirvEmitter;
    CompiledShaderArtifact result;
    result.variantId = nextVariantId();
    SpirvEmitInfo emitted;
    try {
        result.spirv = spirvEmitter.Emit(program, inputInfo, bindings, targetOptions, &emitted);
    } catch (const SpannedWaveUnsupported& error) {
        std::fprintf(stderr, "[spanned-wave] 0x%llx falls back to two lanes per invocation: %s\n", static_cast<unsigned long long>(request.shader.codeAddress), error.what());
        targetOptions.spannedWaves = false;
        result.spirv = spirvEmitter.Emit(program, inputInfo, bindings, targetOptions, &emitted);
    }
    if (registration && result.spirv.size() > request.target.registrationWordLimit) {
        registration = false;
        program.Metadata().bindingLayoutComplete = false;
        bindings = bindingAllocator.Allocate(program, request.layout, false, stencilReplay);
        targetOptions.registrationModules = false;
        result.spirv = spirvEmitter.Emit(program, inputInfo, bindings, targetOptions, &emitted);
    }
    result.registrationModule = registration;
    if (request.shader.stage == ShaderStage::Mesh && request.graphics) result.meshBarycentrics = request.graphics->meshBarycentrics;
    result.stencilReplaySafe = (request.shader.stage == ShaderStage::Vertex || stencilReplay) && StencilReplaySafe(program);
    result.bdaWrites = program.Info().bdaWrites;
    if (stencilReplay) {
        result.stencilReferences = StencilReferenceValues(program, request.context.pixel.has_value() && request.context.pixel->stencilOperationValueExport ? 8u : 0u);
        result.stencilReferenceDword = bindings.layout.stencilReferenceDword;
    }

    result.bdaAbiVersion = program.Info().usesDma || program.Info().usesFaultBuffer ? request.target.bdaAbiVersion : 0u;
    result.windowLaneGuards = 0;
    for (const auto& block : program.Blocks()) result.windowLaneGuards += static_cast<std::uint32_t>(std::ranges::count_if(block->Instructions(), [](const IrValue* value) { return value->Opcode() == IrOpcode::WindowLaneGuard; }));
    result.memoryOffsetDword = bindings.layout.memoryOffsetDword;
    result.shaderDataDwords = bindings.layout.ShaderDataDwords();
    result.imageMetadataDword = bindings.layout.ImageMetadataDword();
    result.runtimeImageCount = bindings.layout.runtimeImageCount;
    result.hostSubgroupSize = HostSubgroupSize(request);
    result.fullSubgroups = emitted.fullSubgroups;
    result.functionLdsBuffer = emitted.functionLdsBuffer;
    result.vertexOffsetSgpr = program.Info().vertexOffsetSgpr;
    result.instanceOffsetSgpr = program.Info().instanceOffsetSgpr;
    result.vertexOffsetShared = program.Info().vertexOffsetShared;
    result.instanceOffsetShared = program.Info().instanceOffsetShared;
    result.vertexOffsetConflict = program.Info().vertexOffsetConflict;
    result.instanceOffsetConflict = program.Info().instanceOffsetConflict;
    for (const auto& output : program.Info().outputs) {
        if (output.kind == StageOutputKind::Parameter) result.parameterExports.push_back(output.location);
    }
    if (request.shader.stage == ShaderStage::Fragment) {
        result.fragmentParameters = DescribeFragmentParameters(program, inputInfo);
        const auto& inputs = program.Info().inputs;
        const auto reads = [&](StageInputKind kind) { return std::any_of(inputs.begin(), inputs.end(), [&](const auto& input) { return input.kind == kind; }); };
        if (program.Metadata().barycentricEmulation) result.barycentricEmulation = {true, reads(StageInputKind::BaryCoordSmooth), reads(StageInputKind::BaryCoordNoPerspective), program.Info().smoothOffsetBarycentrics};
    }
    if (request.shader.stage == ShaderStage::Vertex || request.shader.stage == ShaderStage::Local) {
        if (inputInfo.vertex == nullptr) throw std::runtime_error("vertex input metadata is missing");
        for (const auto& input : program.Info().inputs) {
            if (input.kind != StageInputKind::Parameter) continue;
            if (input.location >= static_cast<std::uint32_t>(inputInfo.vertex->resourcesNum)) throw std::runtime_error("vertex attribute location exceeds resource count");
            result.vertexInputs.push_back({input.location, input.componentCount, inputInfo.vertex->resourcesDst[input.location].fetchIndex});
            if (inputInfo.vertex->fetchEmbedded) {
                for (const auto& block : program.Blocks()) {
                    for (const auto* instruction : block->Instructions()) {
                        if (instruction->Opcode() == IrOpcode::GetAttribute && instruction->Argument(0)->Resolve()->ImmediateU32() == input.location) result.vertexInputs.back().outputMask |= 1u << instruction->Argument(1)->Resolve()->ImmediateU32();
                    }
                }
            }
        }
    }

    const bool embedded = !result.vertexInputs.empty() && inputInfo.vertex != nullptr && inputInfo.vertex->fetchEmbedded;
    if (embedded) PrepareVertexInputSpecialization(result);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, request.target.vulkanVersion, request.target.spirvVersion, request.target.nonConstantImageOffsets, !embedded);
    if (embedded) {
        std::array<std::uint32_t, ShaderVertexStageInfo::MaxResources> classes{};
        for (std::uint32_t kind = 1u; kind < 3u; ++kind) {
            classes.fill(kind);
            const auto specialized = SpecializeVertexInputTypes(result, classes);
            static_cast<void>(ValidateAndOptimizeSpirv(specialized, request.target.vulkanVersion, request.target.spirvVersion, request.target.nonConstantImageOffsets, false));
        }
    }
#endif

    return {request.layout, {}, std::move(static_cast<CompiledBindingLayout&>(bindings)), std::move(result)};
}

CompiledVariant compileVariant(const RecompileRequest& request, IrProgram program, std::exception_ptr* emissionFailure = nullptr) {
    return recordEmissionFailure(emissionFailure, [&] {
        prepareInterface(request, program);
        auto variant = emitVariant(request, program);
        variant.info = std::move(program).TakeCompiledInfo();
        return variant;
    });
}

struct SpecializedModule {
    std::uint64_t specializationId = 0;
    SharedSpirv spirv;
    std::vector<std::uint32_t> bindings;
    bool pushData = false;
};

std::shared_ptr<const SpecializedModule> buildSpecializedModule(const CompiledShaderArtifact& artifact, std::span<const std::uint32_t> classes, std::span<const PipelineSpecializationConstant> constants, const SpirvTarget& target) {
    auto source = SpecializeVertexInputTypes(artifact, classes);
    std::map<std::uint32_t, std::uint32_t> supplied;
    for (const auto& constant : constants) {
        if (!supplied.emplace(constant.id, constant.value).second) throw std::runtime_error("duplicate prepared specialization ID");
    }
    std::map<std::uint32_t, std::uint32_t> values;
    std::set<std::uint32_t> retained;
    std::set<std::uint32_t> specializedIds;
    std::set<std::uint32_t> pushTypes;
    std::optional<std::uint32_t> replaySlotBytes;
    const auto& words = source.Words();
    if (words.size() < 5u || words[0] != spv::MagicNumber) throw std::runtime_error("invalid prepared specialization module");
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        if (count == 0u || count > words.size() - cursor) throw std::runtime_error("truncated prepared specialization instruction");
        if (!artifact.stencilReferences.empty() && op == spv::OpTypePointer && count == 4u && words[cursor + 2u] == spv::StorageClassPushConstant) pushTypes.insert(words[cursor + 3u]);
        if (op == spv::OpDecorate && count == 4u && words[cursor + 2u] == spv::DecorationSpecId) {
            if (PipelineSpecialization::IsWorkgroupOffset(words[cursor + 3u])) {
                if (supplied.contains(words[cursor + 3u])) throw std::runtime_error("a workgroup offset is supplied at dispatch, not prepared");
                if (!retained.insert(words[cursor + 1u]).second) throw std::runtime_error("duplicate workgroup offset specialization");
                cursor += count;
                continue;
            }
            const auto found = supplied.find(words[cursor + 3u]);
            if (found == supplied.end()) throw std::runtime_error("missing prepared specialization value");
            auto value = found->second;
            if (!artifact.stencilReferences.empty()) {
                if (found->first == PipelineSpecialization::StencilReferenceOffset) replaySlotBytes = value / PushData::DwordCount * NativePushSlotSize;
                if (found->first == PipelineSpecialization::StencilReferenceOffset || found->first == PipelineSpecialization::PushDataOffset) value %= PushData::DwordCount;
            }
            if (!values.emplace(words[cursor + 1u], value).second) throw std::runtime_error("duplicate prepared specialization value");
            specializedIds.insert(found->first);
        }
        cursor += count;
    }
    std::vector<std::uint32_t> materialized(words.begin(), words.begin() + 5);
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        if (op == spv::OpSpecConstant && count == 4u && retained.contains(words[cursor + 2u])) {
            materialized.insert(materialized.end(), words.begin() + cursor, words.begin() + cursor + count);
        } else if (op == spv::OpDecorate && count == 4u && words[cursor + 2u] == spv::DecorationSpecId && retained.contains(words[cursor + 1u])) {
            materialized.insert(materialized.end(), words.begin() + cursor, words.begin() + cursor + count);
        } else if (op == spv::OpSpecConstant) {
            if (count != 4u || !values.contains(words[cursor + 2u])) throw std::runtime_error("invalid prepared specialization constant");
            materialized.insert(materialized.end(), {(4u << 16u) | spv::OpConstant, words[cursor + 1u], words[cursor + 2u], values.at(words[cursor + 2u])});
        } else if (op == spv::OpMemberDecorate && count == 5u && words[cursor + 3u] == spv::DecorationOffset && pushTypes.contains(words[cursor + 1u])) {
            if (!replaySlotBytes || words[cursor + 2u] != 0u || *replaySlotBytes >= NativePushConstantSize) throw std::runtime_error("invalid stencil replay push constant slot");
            materialized.insert(materialized.end(), words.begin() + cursor, words.begin() + cursor + count - 1u);
            materialized.push_back(*replaySlotBytes);
        } else if (!(op == spv::OpDecorate && count == 4u && words[cursor + 2u] == spv::DecorationSpecId)) {
            materialized.insert(materialized.end(), words.begin() + cursor, words.begin() + cursor + count);
        }
        cursor += count;
    }
    materialized = SpecializeFragmentOutputs(SpecializeSpirv(materialized), constants, specializedIds);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    materialized = ValidateAndOptimizeSpirv(materialized, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, true, true);
#endif
    std::map<std::uint32_t, std::uint32_t> descriptorVariables;
    std::set<std::uint32_t> usedDescriptors;
    for (std::size_t cursor = 5; cursor < materialized.size();) {
        const auto count = materialized[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(materialized[cursor] & 0xffffu);
        if (op == spv::OpDecorate && count == 4u && materialized[cursor + 2u] == spv::DecorationBinding) descriptorVariables.emplace(materialized[cursor + 1u], materialized[cursor + 3u]);
        if ((op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain || op == spv::OpPtrAccessChain || op == spv::OpInBoundsPtrAccessChain || op == spv::OpLoad || op == spv::OpCopyObject) && count >= 4u) usedDescriptors.insert(materialized[cursor + 3u]);
        cursor += count;
    }
    const auto unused = [&](std::uint32_t id) { return descriptorVariables.contains(id) && !usedDescriptors.contains(id); };
    std::vector<std::uint32_t> compact(materialized.begin(), materialized.begin() + 5);
    for (std::size_t cursor = 5; cursor < materialized.size();) {
        const auto count = materialized[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(materialized[cursor] & 0xffffu);
        if (op == spv::OpEntryPoint) {
            const auto start = compact.size();
            std::size_t interfaceIndex = 3u;
            for (; interfaceIndex < count; ++interfaceIndex) {
                const auto word = materialized[cursor + interfaceIndex];
                if ((word & 0xffu) == 0u || (word & 0xff00u) == 0u || (word & 0xff0000u) == 0u || (word & 0xff000000u) == 0u) { ++interfaceIndex; break; }
            }
            compact.insert(compact.end(), materialized.begin() + cursor, materialized.begin() + cursor + interfaceIndex);
            for (auto index = interfaceIndex; index < count; ++index) if (!unused(materialized[cursor + index])) compact.push_back(materialized[cursor + index]);
            compact[start] = (static_cast<std::uint32_t>(compact.size() - start) << 16u) | spv::OpEntryPoint;
        } else if (!((op == spv::OpVariable && unused(materialized[cursor + 2u])) || ((op == spv::OpDecorate || op == spv::OpName) && unused(materialized[cursor + 1u])))) {
            compact.insert(compact.end(), materialized.begin() + cursor, materialized.begin() + cursor + count);
        }
        cursor += count;
    }
    materialized = std::move(compact);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    materialized = ValidateAndOptimizeSpirv(materialized, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, false);
#endif
    auto module = std::make_shared<SpecializedModule>();
    std::map<std::uint32_t, std::uint32_t> bindingNumbers;
    std::unordered_set<std::uint32_t> functionLdsBuffer;
    for (std::size_t cursor = 5; cursor < materialized.size();) {
        const auto count = materialized[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(materialized[cursor] & 0xffffu);
        if (op == spv::OpDecorate && count == 4u && materialized[cursor + 2u] == spv::DecorationBinding) bindingNumbers.emplace(materialized[cursor + 1u], materialized[cursor + 3u]);
        if (op == spv::OpDecorate && count == 4u && materialized[cursor + 2u] == spv::DecorationDescriptorSet && materialized[cursor + 3u] == PixelFunctionLdsDescriptorSet) functionLdsBuffer.insert(materialized[cursor + 1u]);
        if (op == spv::OpVariable && count >= 4u && !functionLdsBuffer.contains(materialized[cursor + 2u])) {
            if (const auto found = bindingNumbers.find(materialized[cursor + 2u]); found != bindingNumbers.end()) module->bindings.push_back(found->second);
            module->pushData |= materialized[cursor + 3u] == spv::StorageClassPushConstant;
        }
        cursor += count;
    }
    module->spirv = InternSpirv(std::move(materialized));
    module->specializationId = constants.empty() ? 0u : nextVariantId();
    return module;
}

struct SpecializedModuleEntry {
    std::once_flag ready;
    std::exception_ptr failure;
    std::shared_ptr<const SpecializedModule> module;
};

struct PreparedModuleEntry {
    std::once_flag ready;
    std::exception_ptr failure;
    std::shared_ptr<const SpecializedModule> module;
    DescriptorBindingPlan bindings;
};

struct PreparedBindingPlan {
    std::once_flag ready;
    std::exception_ptr failure;
    DescriptorBindingPlan bindings;
    PrimedSharedMutex mutex;
    std::map<std::vector<std::uint32_t>, std::shared_ptr<PreparedModuleEntry>> modules;
};

struct RegistrationModuleEntry {
    std::once_flag ready;
    std::exception_ptr failure;
    std::shared_ptr<const SpecializedModule> module;
};

std::shared_ptr<const SpecializedModule> registrationModule(std::uint64_t variantId, std::span<const std::uint32_t> moduleKey, const std::function<std::shared_ptr<const SpecializedModule>()>& specialize) {
    std::vector<std::uint64_t> key{variantId};
    key.insert(key.end(), moduleKey.begin(), moduleKey.end());
    static PrimedSharedMutex mutex;
    static std::map<std::vector<std::uint64_t>, std::shared_ptr<RegistrationModuleEntry>> modules;
    std::shared_ptr<RegistrationModuleEntry> entry;
    {
        std::shared_lock lock(mutex);
        if (const auto found = modules.find(key); found != modules.end()) entry = found->second;
    }
    if (entry == nullptr) {
        std::unique_lock lock(mutex);
        const auto found = modules.find(key);
        entry = found != modules.end() ? found->second : modules.emplace(key, std::make_shared<RegistrationModuleEntry>()).first->second;
    }
    std::call_once(entry->ready, [&] {
        try {
            entry->module = specialize();
        } catch (...) {
            entry->failure = std::current_exception();
        }
    });
    if (entry->failure) std::rethrow_exception(entry->failure);
    return entry->module;
}

std::shared_ptr<PreparedBindingPlan> preparedBindingPlan(const CompiledVariant& variant, const ResourceSnapshot& snapshot, std::span<const std::uint8_t> exports, bool fragmentDescriptorSet) {
    struct BindingPlanKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, BindingPlanKeyStorage>();
    key.clear();
    key.push_back(variant.artifact.variantId);
    for (std::size_t index = 0; index < variant.info.info.buffers.size(); ++index) {
        const auto& descriptor = snapshot.buffers.at(index);
        key.push_back(descriptor.dwordCount);
        key.push_back(descriptor.dwords[1] & 0xffff0000u);
        key.push_back(descriptor.dwords[3]);
        const bool present = descriptor.dwords[2] != 0u && (descriptor.dwords[0] != 0u || (descriptor.dwords[1] & 0xffffu) != 0u);
        key.push_back(present ? 4u | (descriptor.dwords[0] & 3u) : 0u);
    }
    for (std::size_t index = 0; index < variant.info.info.images.size(); ++index) {
        const auto& descriptor = snapshot.images.at(index);
        key.push_back(descriptor.dwordCount);
        key.push_back(descriptor.dwords[0] != 0u || (descriptor.dwords[1] & 0xffu) != 0u);
        key.push_back(descriptor.dwords[1] & ~0xffu);
        for (std::size_t word = 2; word < 8; ++word) key.push_back(descriptor.dwords[word]);
    }
    for (std::size_t index = 0; index < variant.info.info.samplers.size(); ++index) {
        const auto& descriptor = snapshot.samplers.at(index);
        key.push_back(descriptor.dwordCount);
        for (std::size_t word = 0; word < 4; ++word) key.push_back(descriptor.dwords[word]);
    }
    key.push_back(exports.size());
    for (const auto mapping : exports) key.push_back(mapping);
    static PrimedSharedMutex mutex;
    static std::map<std::vector<std::uint64_t>, std::shared_ptr<PreparedBindingPlan>> plans;
    std::shared_ptr<PreparedBindingPlan> plan;
    {
        std::shared_lock lock(mutex);
        if (const auto found = plans.find(key); found != plans.end()) plan = found->second;
    }
    if (plan == nullptr) {
        std::unique_lock lock(mutex);
        const auto found = plans.find(key);
        plan = found != plans.end() ? found->second : plans.emplace(key, std::make_shared<PreparedBindingPlan>()).first->second;
    }
    std::call_once(plan->ready, [&] {
        try {
            plan->bindings = DescriptorBindingBuilder{}.Prepare(variant.bindings.layout, variant.info.info, variant.info.stage, snapshot, exports, variant.artifact.registrationModule, fragmentDescriptorSet);
        } catch (...) {
            plan->failure = std::current_exception();
        }
    });
    if (plan->failure) std::rethrow_exception(plan->failure);
    return plan;
}

std::shared_ptr<const SpecializedModule> specializeModule(const CompiledShaderArtifact& artifact, std::span<const std::uint32_t> classes, std::span<const PipelineSpecializationConstant> constants, const SpirvTarget& target) {
    struct ModuleKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, ModuleKeyStorage>();
    key.clear();
    key.push_back(artifact.variantId);
    for (const auto& constant : constants) key.push_back((static_cast<std::uint64_t>(constant.id) << 32u) | constant.value);
    static PrimedSharedMutex mutex;
    static std::map<std::vector<std::uint64_t>, std::shared_ptr<SpecializedModuleEntry>> modules;
    std::shared_ptr<SpecializedModuleEntry> entry;
    {
        std::shared_lock lock(mutex);
        if (const auto found = modules.find(key); found != modules.end()) entry = found->second;
    }
    if (entry == nullptr) {
        std::unique_lock lock(mutex);
        const auto found = modules.find(key);
        entry = found != modules.end() ? found->second : modules.emplace(key, std::make_shared<SpecializedModuleEntry>()).first->second;
    }
    std::call_once(entry->ready, [&] {
        try {
            entry->module = buildSpecializedModule(artifact, classes, constants, target);
        } catch (...) {
            entry->failure = std::current_exception();
        }
    });
    if (entry->failure) std::rethrow_exception(entry->failure);
    return entry->module;
}

const ResourceSnapshot& mergeTargetImage(const CompiledVariant& variant, const RecompileRequest& request, const ResourceSnapshot& snapshot, bool registrationDefaults, ResourceSnapshot& merged) {
    const auto& images = variant.info.info.images;
    const auto image = std::ranges::find_if(images, [](const ImageResource& candidate) { return candidate.targetMerge; });
    if (image == images.end() || registrationDefaults) return snapshot;
    if (!request.context.pixel || request.context.pixel->targetMerge.mask == 0u) throw std::runtime_error("ShaderRecompiler: a color target merge variant was materialized without the merged target");
    if (image + 1 != images.end() || snapshot.images.size() + 1u != images.size()) throw std::runtime_error("ShaderRecompiler: the captured images do not precede the color target merge image");
    merged = snapshot;
    DescriptorValue descriptor;
    std::ranges::copy(request.context.pixel->targetMerge.descriptor, descriptor.dwords.begin());
    descriptor.dwordCount = 8u;
    descriptor.availableDwordMask = 0xffu;
    merged.images.push_back(descriptor);
    return merged;
}

RecompileResult materializeResult(const CompiledVariant& variant, const RecompileRequest& request, const ResourceSnapshot& captured, bool registrationDefaults = false) {
    ResourceSnapshot merged;
    const auto& snapshot = mergeTargetImage(variant, request, captured, registrationDefaults, merged);
    RecompileResult result;
    const auto& artifact = variant.artifact;
    static_cast<CompiledShaderArtifact&>(result) = {
        .memoryOffsetDword = artifact.memoryOffsetDword,
        .shaderDataDwords = artifact.shaderDataDwords,
        .imageMetadataDword = artifact.imageMetadataDword,
        .runtimeImageCount = artifact.runtimeImageCount,
        .runtimeImageResources = artifact.runtimeImageResources,
        .registrationModule = artifact.registrationModule,
        .bdaAbiVersion = artifact.bdaAbiVersion,
        .runtimeAbiVersion = artifact.runtimeAbiVersion,
        .vertexInputs = artifact.vertexInputs,
        .vertexOffsetSgpr = artifact.vertexOffsetSgpr,
        .instanceOffsetSgpr = artifact.instanceOffsetSgpr,
        .vertexOffsetShared = artifact.vertexOffsetShared,
        .instanceOffsetShared = artifact.instanceOffsetShared,
        .vertexOffsetConflict = artifact.vertexOffsetConflict,
        .instanceOffsetConflict = artifact.instanceOffsetConflict,
        .hostSubgroupSize = artifact.hostSubgroupSize,
        .fullSubgroups = artifact.fullSubgroups,
        .functionLdsBuffer = artifact.functionLdsBuffer,
        .parameterExports = artifact.parameterExports,
        .fragmentParameters = artifact.fragmentParameters,
        .barycentricEmulation = artifact.barycentricEmulation,
        .meshBarycentrics = artifact.meshBarycentrics,
        .stencilReferences = artifact.stencilReferences,
        .stencilReplaySafe = artifact.stencilReplaySafe,
        .bdaWrites = artifact.bdaWrites,
        .stencilReferenceDword = artifact.stencilReferenceDword,
        .variantId = artifact.variantId
    };
    for (const auto reg : variant.bindings.layout.userDataRegisters) {
        const auto index = reg - request.context.userDataBaseRegister;
        result.userDataRegisterReads |= reg >= request.context.userDataBaseRegister && index < 64u ? std::uint64_t{1} << index : ~std::uint64_t{0};
    }
    const auto plan = preparedBindingPlan(variant, snapshot, request.context.pixel ? std::span<const std::uint8_t>(request.context.pixel->targetExportMapping) : std::span<const std::uint8_t>{}, request.target.registrationModules);
    const DescriptorBindingPlan* bindingPlan = &plan->bindings;
    std::shared_ptr<PreparedModuleEntry> entry;
    struct LocalModuleKeyStorage {};
    auto& moduleKey = HostThreadLocal<std::vector<std::uint32_t>, LocalModuleKeyStorage>();
    moduleKey.clear();
    if (variant.bindings.layout.UsesPushData()) moduleKey.push_back(request.layout.pushConstantOffsetBytes / 4u);
    if (!artifact.stencilReferences.empty()) moduleKey.push_back(request.layout.pushConstantOffsetBytes / 4u + artifact.stencilReferenceDword);
    if (request.context.pixel) {
        for (const auto packing : request.context.pixel->targetExportPacking) moduleKey.push_back(static_cast<std::uint32_t>(packing));
        moduleKey.push_back(request.context.pixel->dualSourceBlend ? 1u : 0u);
        moduleKey.push_back(request.context.pixel->reversedBlend);
        if (variant.info.info.rasterPrimitive) moduleKey.push_back(static_cast<std::uint32_t>(request.context.pixel->rasterPrimitive));
        if (variant.info.info.layerExported) moduleKey.push_back(request.context.pixel->layerExported ? 1u : 0u);
    }
    const bool depthRemap = request.target.depthRangeRemap && request.shader.stage == ShaderStage::Vertex && request.context.vertex.has_value();
    if (depthRemap) {
        if (request.context.vertex->depthRemapMode != 0u && request.layout.pushConstantOffsetBytes % (RuntimeAbi::PushConstantDwords * 4u) + request.layout.pushConstantSizeBytes > DepthRemapPushOffsetBytes) {
            throw std::runtime_error("a remapped vertex shader's push data must end before the depth remap words");
        }
        moduleKey.push_back(request.context.vertex->depthRemapMode);
    }
    result.vertexAttributes.reserve(result.vertexInputs.size());
    std::array<std::uint32_t, ShaderVertexStageInfo::MaxResources> vertexClasses{};
    std::array<std::uint32_t, ShaderVertexStageInfo::MaxResources / RuntimeAbi::VertexSelectorsPerDword> runtimeSelectors{};
    for (const auto& input : result.vertexInputs) {
        if (!request.context.vertex || input.location >= request.context.vertex->resourcesNum || input.location >= request.context.vertex->resources.size()) throw std::runtime_error("Shader cache: invalid vertex attribute metadata");
        const auto& vertex = *request.context.vertex;
        const auto& resource = vertex.resources[input.location];
        const auto fetchIndex = vertex.fetchEmbedded ? vertex.resourcesDst[input.location].fetchIndex : input.fetchIndex;
        result.vertexAttributes.push_back({input.location, input.components, resource, fetchIndex});
        if (!artifact.vertexInputPatches.empty()) {
            const auto format = static_cast<IrBufferFormat>((resource.fields[3] >> 12u) & 0x7fu);
            const auto numeric = registrationDefaults && format == IrBufferFormat::Invalid ? IrTextureNumericClass::Float : VertexInputNumericClass(format);
            if (numeric == IrTextureNumericClass::Unsupported) throw std::runtime_error("unsupported prepared vertex format");
            const auto kind = numeric == IrTextureNumericClass::Float ? 0u : numeric == IrTextureNumericClass::Sint ? 1u : 2u;
            vertexClasses.at(input.location) = kind;
            std::uint32_t selectors = kind << 12u;
            auto& formatComponents = result.vertexAttributes.back().formatComponents;
            formatComponents = 1u;
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto selector = (input.outputMask & (1u << component)) != 0u ? (resource.fields[3] >> (component * 3u)) & 7u : 0u;
                if (selector == 2u || selector == 3u) throw std::runtime_error("reserved prepared vertex component selector");
                if (selector >= 4u) formatComponents = std::max(formatComponents, selector - 3u);
                selectors |= selector << (component * 3u);
            }
            moduleKey.push_back(artifact.registrationModule ? selectors & ~0xfffu : selectors);
            if (artifact.registrationModule) runtimeSelectors.at(input.location / RuntimeAbi::VertexSelectorsPerDword) |= (selectors & 0xfffu) << (input.location % RuntimeAbi::VertexSelectorsPerDword * 16u);
        }
    }
    if (!plan->bindings.specialization.empty() || variant.bindings.layout.UsesPushData() || !artifact.vertexInputPatches.empty() || request.context.pixel || depthRemap) {
        {
            std::shared_lock lock(plan->mutex);
            if (const auto found = plan->modules.find(moduleKey); found != plan->modules.end()) entry = found->second;
        }
        if (entry == nullptr) {
            std::unique_lock lock(plan->mutex);
            const auto found = plan->modules.find(moduleKey);
            entry = found != plan->modules.end() ? found->second : plan->modules.emplace(moduleKey, std::make_shared<PreparedModuleEntry>()).first->second;
        }
        const auto prepare = [&] {
            auto constants = plan->bindings.specialization;
            std::size_t index = 0;
            if (variant.bindings.layout.UsesPushData()) constants.push_back({PipelineSpecialization::PushDataOffset, moduleKey[index++]});
            if (!artifact.stencilReferences.empty()) constants.push_back({PipelineSpecialization::StencilReferenceOffset, moduleKey[index++]});
            if (request.context.pixel) {
                for (std::uint32_t target = 0; target < request.context.pixel->targetExportPacking.size(); ++target) constants.push_back({PipelineSpecialization::ExportPackingBase + target, moduleKey[index++]});
                constants.push_back({PipelineSpecialization::DualSourceBlend, moduleKey[index++]});
                constants.push_back({PipelineSpecialization::ReversedBlend, moduleKey[index++]});
                if (variant.info.info.rasterPrimitive) constants.push_back({PipelineSpecialization::RasterPrimitive, moduleKey[index++]});
                if (variant.info.info.layerExported) constants.push_back({PipelineSpecialization::LayerExported, moduleKey[index++]});
            }
            if (depthRemap) {
                for (std::uint32_t word = 0; word < PipelineSpecialization::DepthRemapWords; ++word) constants.push_back({PipelineSpecialization::DepthRemapBase + word, moduleKey[index++]});
            }
            if (!artifact.vertexInputPatches.empty()) {
                for (const auto& input : result.vertexInputs) {
                    const auto selectors = moduleKey[index++];
                    const auto first = PipelineSpecialization::VertexBase + input.location * PipelineSpecialization::VertexWords;
                    for (std::uint32_t component = 0; component < 4u; ++component) constants.push_back({first + component, (selectors >> (component * 3u)) & 7u});
                    const auto kind = selectors >> 12u;
                    constants.push_back({first + 4u, kind == 0u ? 0x3f800000u : 1u});
                    constants.push_back({first + 5u, kind});
                }
            }
            const auto module = artifact.registrationModule && plan->bindings.specialization.empty() ? registrationModule(artifact.variantId, moduleKey, [&] { return specializeModule(variant.artifact, vertexClasses, constants, request.target); }) : specializeModule(variant.artifact, vertexClasses, constants, request.target);
            if (variant.bindings.layout.UsesPushData() && !module->pushData) throw std::runtime_error("specialization removed the prepared push constant interface");
            auto selected = DescriptorBindingBuilder{}.Select(plan->bindings, module->bindings);
            entry->bindings = std::move(selected);
            entry->module = module;
        };
        std::call_once(entry->ready, [&] {
            try {
                prepare();
            } catch (...) {
                entry->failure = std::current_exception();
            }
        });
        if (entry->failure) std::rethrow_exception(entry->failure);
        const auto& module = entry->module;
        result.specializationId = module->specializationId;
        result.spirv = module->spirv;
        bindingPlan = &entry->bindings;
    } else {
        result.spirv = artifact.spirv;
    }
    const auto resolved = ResourceMaterializer::ResolveImageTablesShared(variant.info.info, snapshot, artifact.variantId);
    const auto& tables = *resolved;
    BindingAllocationResult bindings;
    DescriptorBindingBuilder{}.Populate(bindings, variant.bindings, *bindingPlan, variant.info.userDataBase, snapshot, partialThreads(request), &tables);
    result.workgroupMemoryDwords = WorkgroupMemoryStrideDwords(variant.info.info);
    result.bindings = std::move(bindings.bindings);
    if (std::ranges::none_of(result.bindings, [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::BdaPagetable || binding.role == DescriptorRole::FaultBuffer; })) result.bdaAbiVersion = 0u;
    result.pushConstants = std::move(bindings.pushConstants);
    result.readsDepthRemapPush = depthRemap;
    if (request.shader.stage == ShaderStage::Compute) {
        for (std::uint32_t component = 0; component < PipelineSpecialization::WorkgroupOffsetWords; ++component) result.specialization.push_back({PipelineSpecialization::WorkgroupOffsetBase + component, 0u});
    }
    if (!artifact.stencilReferences.empty()) {
        if (artifact.stencilReferenceDword * sizeof(std::uint32_t) != result.pushConstants.size() || result.pushConstants.size() + sizeof(std::uint32_t) > request.layout.pushConstantSizeBytes) throw std::runtime_error("stencil replay push constant lies outside the stage range");
        result.pushConstants.resize(result.pushConstants.size() + sizeof(std::uint32_t));
    }
    const auto& layout = variant.bindings.layout;
    if (artifact.registrationModule && request.shader.stage != ShaderStage::Fragment && layout.runtimeStateDwords != 0u) {
        const auto first = layout.RuntimeStateDword() + RuntimeAbi::StateVertexSelectors;
        if (layout.runtimeStateDwords > runtimeSelectors.size()) throw std::runtime_error("vertex selector state exceeds its capacity");
        if (layout.UsesPushData()) {
            if ((first + layout.runtimeStateDwords) * sizeof(std::uint32_t) > result.pushConstants.size()) throw std::runtime_error("vertex selector state lies outside the push data");
            std::memcpy(result.pushConstants.data() + first * sizeof(std::uint32_t), runtimeSelectors.data(), layout.runtimeStateDwords * sizeof(std::uint32_t));
        } else {
            const auto data = std::ranges::find_if(result.bindings, [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::ShaderData; });
            if (data == result.bindings.end() || first + layout.runtimeStateDwords > data->guestDescriptor.size()) throw std::runtime_error("vertex selector state lies outside the shader data");
            std::copy_n(runtimeSelectors.begin(), layout.runtimeStateDwords, data->guestDescriptor.begin() + first);
        }
    }
    result.shaderDataUserWords = std::move(bindings.shaderDataUserWords);
    result.poisonedSrtReads = static_cast<std::uint32_t>(snapshot.srtPoison.size()) + snapshot.nullRootReads;
    if (!tables.map.empty()) {
        result.imageTablePoison.reserve(tables.poison.size());
        for (const auto& poison : tables.poison) result.imageTablePoison.push_back({poison.words, poison.dwordCount, poison.resource, static_cast<std::uint32_t>(poison.reason)});
        result.imageTableRanges = snapshot.tables.ranges;
        result.imageTableReadRanges.reserve(snapshot.tables.tables.size());
        for (const auto& table : snapshot.tables.tables) result.imageTableReadRanges.push_back(table.readRanges.Copy());
        result.imageTableShader = snapshot.tables.shader;
        result.imageTableFaults = tables.faults;
    }
    return result;
}

bool sameLayout(const BindingLayout& left, const BindingLayout& right) {
    return left.descriptorSet == right.descriptorSet && left.firstBinding == right.firstBinding && left.pushConstantOffsetBytes == right.pushConstantOffsetBytes && left.pushConstantSizeBytes == right.pushConstantSizeBytes;
}

}

SharedSpirv InternSpirv(std::vector<std::uint32_t> words) {
    SharedSpirv spirv(std::move(words));
    const auto hash = spirv.ContentHash();
    static std::mutex mutex;
    static std::unordered_map<std::uint64_t, std::vector<SharedSpirv>> interned;
    std::lock_guard lock(mutex);
    auto& candidates = interned[hash.low ^ (hash.high * 0x9e3779b97f4a7c15ull)];
    for (const auto& candidate : candidates) {
        if (candidate.ContentHash() == hash && candidate == spirv) return candidate;
    }
    candidates.push_back(spirv);
    return spirv;
}

MeshOutputBudget MeshBarycentricOutputBudget(const MeshConfiguration& mesh, const MeshTargetLimits& limits, const MeshOutputUse& use) {
    const auto alignUp = [](std::uint64_t value, std::uint64_t granularity) { return granularity <= 1u ? value : (value + granularity - 1u) / granularity * granularity; };
    MeshOutputBudget budget;
    budget.vertexLocations = 1u + use.flatParameters + use.clipVaryings + (use.clipDistances + 3u) / 4u + (use.cullDistances + 3u) / 4u;
    budget.primitiveLocations = 3u + 3u * use.perVertexParameters + 1u + (use.layer ? 1u : 0u);
    budget.components = 4ull * (budget.vertexLocations + budget.primitiveLocations);
    budget.bytes = 16ull * (budget.vertexLocations * alignUp(mesh.maxVertices, limits.outputPerVertexGranularity) + budget.primitiveLocations * alignUp(mesh.maxPrimitives, limits.outputPerPrimitiveGranularity));
    return budget;
}

std::optional<MeshBarycentricLayout> MeshBarycentricLayoutOf(const RecompileResult& fragment) {
    if (!fragment.barycentricEmulation.active) return std::nullopt;
    MeshBarycentricLayout layout;
    layout.emulation = fragment.barycentricEmulation;
    for (const auto& parameter : fragment.fragmentParameters) {
        layout.parameters.push_back({parameter.location, parameter.sourceLocation, false, parameter.perVertex});
    }
    return layout;
}

struct PreparedInterface {
    static CompiledVariant Compile(SourceEntry& source, const RecompileRequest& request, ShaderPreparationContext& preparation, std::exception_ptr* emissionFailure) {
        if (preparation.interfaceSource.get() != &source || preparation.interfaceProgram->Resources().shaderHash != request.shader.codeAddress) {
            preparation.interfaceSource.reset();
            preparation.interfaceProgram.reset();
            auto program = source.program != nullptr ? std::move(source.program) : std::make_unique<IrProgram>(PrepareResourceProgram(request, &preparation));
            recordEmissionFailure(emissionFailure, [&] { prepareInterface(request, *program); });
            preparation.interfaceSource = source.shared_from_this();
            preparation.interfaceProgram = std::move(program);
        }
        auto& program = *preparation.interfaceProgram;
        return recordEmissionFailure(emissionFailure, [&] {
            auto variant = emitVariant(request, program);
            variant.info = program.CompiledInfo();
            return variant;
        });
    }
};

namespace {

std::shared_ptr<const CompiledVariant> findOrCompileVariant(SourceEntry& source, const RecompileRequest& request, bool& cacheHit, ShaderPreparationContext* preparation = nullptr) {
    for (const auto& candidate : source.variants) {
        if (sameLayout(candidate->layout, request.layout)) {
            cacheHit = true;
            return candidate;
        }
    }
    for (const auto& failure : source.emissionFailures) {
        if (failure.codeAddress == request.shader.codeAddress && sameLayout(failure.layout, request.layout)) std::rethrow_exception(failure.failure);
    }
    cacheHit = false;
    const bool disk = ShaderDiskCache::Enabled() && !DebugProbeActive();
    std::vector<std::byte> diskKey;
    std::shared_ptr<const CompiledVariant> variant;
    if (disk) {
        ShaderDiskCache::BuildKey(request, HostSubgroupSize(request), diskKey);
        CompiledVariant loaded;
        if (ShaderDiskCache::Load(diskKey, loaded)) {
            loaded.layout = request.layout;
            loaded.artifact.variantId = nextVariantId();
            variant = std::make_shared<const CompiledVariant>(std::move(loaded));
        }
    }
    if (variant == nullptr) {
        static const bool layoutPair = std::getenv("APS5_NO_PERF_LAYOUT_PAIR") == nullptr;
        std::exception_ptr emissionFailure;
        try {
            if (layoutPair && preparation != nullptr) {
                variant = std::make_shared<const CompiledVariant>(PreparedInterface::Compile(source, request, *preparation, &emissionFailure));
            } else {
                auto program = source.program != nullptr ? std::move(*source.program) : PrepareResourceProgram(request, preparation);
                source.program.reset();
                variant = std::make_shared<const CompiledVariant>(compileVariant(request, std::move(program), &emissionFailure));
            }
        } catch (...) {
            if (emissionFailure != nullptr && FailureMemo()) source.emissionFailures.push_back({request.shader.codeAddress, request.layout, emissionFailure});
            throw;
        }
        if (disk) {
            if (auto persisted = ShaderDiskCache::Persist(diskKey, *variant)) variant = std::move(persisted);
            else ShaderDiskCache::Store(std::move(diskKey), variant);
        }
    }
    source.program.reset();
    source.variants.push_back(variant);
    return variant;
}

RecompileResult materializeVariant(SourceEntry& source, const RecompileRequest& request, const ResourceSnapshot& snapshot) {
    std::shared_ptr<const CompiledVariant> variant;
    bool cacheHit = false;
    {
        std::lock_guard lock(source.mutex);
        variant = findOrCompileVariant(source, request, cacheHit);
    }
    auto result = materializeResult(*variant, request, snapshot);
    result.cacheHit = cacheHit;
    return result;
}

RecompileResult recompileOnce(const RecompileRequest& request) {
    static_cast<void>(RequestInputInfo(request));
    RequestMemoryView memory(request.context.memory);
    const auto runtime = memory.MakeRuntime(request.context.userData, request.shader.codeAddress);
    ResourceSnapshot snapshot;
    constexpr ResourceMaterializer materializer;
    if (!request.useCache) {
        auto program = PrepareResourceProgram(request);
        const auto plan = materializer.ExtractPlan(program);
        materializer.Materialize(plan, runtime, snapshot);
        const auto variant = compileVariant(request, std::move(program));
        return materializeResult(variant, request, snapshot);
    }
    const auto source = getSource(request);
    materializer.Materialize(*acquirePlan(request, source, true), runtime, snapshot);
    return materializeVariant(*source, request, snapshot);
}

RecompileResult RecompileImpl(const RecompileRequest& request) {
    try {
        return recompileOnce(request);
    } catch (const ConvertedSampleRequired&) {
        if (request.context.convertedSamples) throw;
    }
    auto converted = request;
    converted.context.convertedSamples = true;
    return recompileOnce(converted);
}

// APS5_NO_RESULT_MEMO=1: every Recompile(request, capture) materializes its own result as before.
bool ResultMemo() {
    static const bool resultMemo = std::getenv("APS5_NO_RESULT_MEMO") == nullptr;
    return resultMemo;
}

ResultMemoEntry* memoSet(SourceEntry& source, std::uint64_t index) {
    if (source.memo == nullptr) return nullptr;
    return source.memo->data() + static_cast<std::size_t>((index ^ (index >> 32u)) % ResultMemoSets) * ResultMemoWays;
}

struct ResultMemoCounters {
    std::atomic<std::uint64_t> hits{0}, misses{0}, evictions{0}, trimmed{0}, populateNanoseconds{0};
    std::atomic<std::int64_t> lastReport{0};
    std::atomic<std::uint64_t> clock{0}, heldBytes{0}, heldEntries{0};
};

ResultMemoCounters& resultMemoCounters() {
    static ResultMemoCounters counters;
    return counters;
}

template <typename T>
std::uint64_t vectorBytes(const std::vector<T>& values) {
    return static_cast<std::uint64_t>(values.capacity()) * sizeof(T);
}

std::uint64_t vectorBytes(const std::vector<bool>& values) {
    return static_cast<std::uint64_t>(values.capacity() + 7u) / 8u;
}

std::uint64_t descriptorBindingBytes(const DescriptorBinding& binding) {
    return vectorBytes(binding.guestDescriptor) + vectorBytes(binding.imageShapes) + vectorBytes(binding.imageSamplers) + vectorBytes(binding.bufferExtent) + vectorBytes(binding.samplerDepthCompare) + vectorBytes(binding.imageWritten) + vectorBytes(binding.imageDepthCompare) + vectorBytes(binding.imageAtomic) + vectorBytes(binding.imageAtomic64) + vectorBytes(binding.bufferAtomic) + vectorBytes(binding.bufferWritten) + vectorBytes(binding.samplerUnnormalized) + vectorBytes(binding.samplerIntegerBorder) + vectorBytes(binding.samplerCoordinatesIgnored) + vectorBytes(binding.imageUnnormalized) + vectorBytes(binding.imageUnnormalizedBaseLevel) + vectorBytes(binding.bufferRead);
}

std::uint64_t resultBytes(const RecompileResult& result) {
    const auto bytes = [](const auto& values) { return vectorBytes(values); };
    auto total = static_cast<std::uint64_t>(sizeof(RecompileResult)) + bytes(result.vertexInputPatches) + bytes(result.runtimeImageResources) + bytes(result.vertexInputs) + bytes(result.parameterExports) + bytes(result.fragmentParameters) + bytes(result.stencilReferences) + bytes(result.shaderDataUserWords) + bytes(result.specialization) + bytes(result.bindings) + bytes(result.pushConstants) + bytes(result.vertexAttributes) + bytes(result.imageTablePoison) + bytes(result.imageTableRanges) + bytes(result.imageTableReadRanges);
    for (const auto& ranges : result.imageTableReadRanges) total += bytes(ranges);
    for (const auto& binding : result.bindings) total += descriptorBindingBytes(binding);
    return total;
}

std::atomic<std::uint64_t>& resultMemoBudgetBytes() {
    static std::atomic<std::uint64_t> budget{[] {
        const char* value = std::getenv("APS5_RESULT_MEMO_MB");
        const std::uint64_t megabytes = value == nullptr || *value == '\0' ? 256u : std::strtoull(value, nullptr, 10);
        return megabytes << 20u;
    }()};
    return budget;
}

std::uint64_t resultMemoBudget() {
    return resultMemoBudgetBytes().load(std::memory_order_relaxed);
}

struct ResultMemoRegistry {
    std::mutex mutex;
    std::vector<std::weak_ptr<SourceEntry>> sources;
};

ResultMemoRegistry& resultMemoRegistry() {
    static ResultMemoRegistry registry;
    return registry;
}

std::mutex& resultMemoTrimMutex() {
    static std::mutex mutex;
    return mutex;
}

std::function<void()>& resultMemoTrimHook() {
    static std::function<void()> hook;
    return hook;
}

void trimResultMemo(std::uint64_t budget) {
    std::lock_guard trim(resultMemoTrimMutex());
    auto& counters = resultMemoCounters();
    for (auto held = counters.heldBytes.load(std::memory_order_relaxed); held > budget; held = counters.heldBytes.load(std::memory_order_relaxed)) {
        if (auto hook = std::exchange(resultMemoTrimHook(), nullptr)) hook();
        std::vector<std::shared_ptr<SourceEntry>> sources;
        {
            auto& registry = resultMemoRegistry();
            std::lock_guard lock(registry.mutex);
            sources.reserve(registry.sources.size());
            for (const auto& weak : registry.sources) {
                if (auto source = weak.lock()) sources.push_back(std::move(source));
            }
        }
        std::vector<std::pair<std::uint64_t, std::uint64_t>> stamps;
        for (const auto& source : sources) {
            std::lock_guard lock(source->mutex);
            for (const auto& entry : *source->memo) {
                if (entry.result != nullptr) stamps.emplace_back(entry.used, entry.bytes);
            }
        }
        if (stamps.empty()) return;
        std::sort(stamps.begin(), stamps.end());
        const auto excess = held - budget / 4u * 3u;
        std::uint64_t freed = 0;
        std::uint64_t threshold = 0;
        for (const auto& [used, bytes] : stamps) {
            threshold = used;
            freed += bytes;
            if (freed >= excess) break;
        }
        std::uint64_t dropped = 0;
        for (const auto& source : sources) {
            std::lock_guard lock(source->mutex);
            for (auto& entry : *source->memo) {
                if (entry.result == nullptr || entry.used > threshold) continue;
                counters.heldBytes.fetch_sub(entry.bytes, std::memory_order_relaxed);
                counters.heldEntries.fetch_sub(1, std::memory_order_relaxed);
                counters.trimmed.fetch_add(1, std::memory_order_relaxed);
                entry = {};
                ++dropped;
            }
        }
        if (dropped == 0) return;
    }
}

void reportResultMemo() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& counters = resultMemoCounters();
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load(std::memory_order_relaxed);
    if (last == 0) {
        counters.lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed);
        return;
    }
    if (now - last < 10'000'000'000ll || !counters.lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;
    const auto hits = counters.hits.exchange(0, std::memory_order_relaxed);
    const auto misses = counters.misses.exchange(0, std::memory_order_relaxed);
    const auto evictions = counters.evictions.exchange(0, std::memory_order_relaxed);
    const auto trimmed = counters.trimmed.exchange(0, std::memory_order_relaxed);
    const auto populate = counters.populateNanoseconds.exchange(0, std::memory_order_relaxed);
    std::fprintf(stderr, "[recompile] result memo (10 s): %llu hits, %llu misses (%.1f%% hits), Populate %.1f us per miss / %.1f ms in total, %llu evictions, %llu trimmed; %llu results held, %.1f MiB\n", static_cast<unsigned long long>(hits), static_cast<unsigned long long>(misses), hits + misses != 0 ? 100.0 * static_cast<double>(hits) / static_cast<double>(hits + misses) : 0.0, misses != 0 ? static_cast<double>(populate) / 1000.0 / static_cast<double>(misses) : 0.0, static_cast<double>(populate) / 1e6, static_cast<unsigned long long>(evictions), static_cast<unsigned long long>(trimmed), static_cast<unsigned long long>(counters.heldEntries.load(std::memory_order_relaxed)), static_cast<double>(counters.heldBytes.load(std::memory_order_relaxed)) / 1048576.0);
}

// Everything materializeResult reads besides the variant: the snapshot (the descriptor words, the
// flattened SRT, the user data, the uniform fill) and, for the vertex family, the V# table the
// attributes are resolved from.
std::uint64_t snapshotHash(const RecompileRequest& request, const ResourceSnapshot& snapshot) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    const auto mix = [&](std::uint64_t value) {
        hash ^= value;
        hash *= 0x100000001b3ull;
    };
    const auto mixWords = [&](std::span<const std::uint32_t> words) {
        mix(words.size());
        for (const auto word : words) mix(word);
    };
    const auto mixDescriptors = [&](const std::vector<DescriptorValue>& values) {
        mix(values.size());
        for (const auto& value : values) {
            mix(value.dwordCount);
            mix(value.availableDwordMask);
            for (std::uint32_t i = 0; i < value.dwordCount && i < value.dwords.size(); ++i) mix(value.dwords[i]);
        }
    };
    mixDescriptors(snapshot.buffers);
    mixDescriptors(snapshot.images);
    mixDescriptors(snapshot.samplers);
    mixWords(snapshot.flattenedSrt);
    mixWords(snapshot.userData);
    mix(static_cast<std::uint64_t>(snapshot.uniformFill.kind));
    mix(snapshot.uniformFill.resource);
    for (const auto stride : snapshot.uniformFill.groupStride) mix(stride);
    mix(snapshot.uniformFill.words);
    mix(snapshot.uniformFill.value);
    const auto& tables = snapshot.tables;
    mix(tables.shader);
    mix(tables.tables.size());
    for (const auto& table : tables.tables) {
        mix(table.base);
        mix(table.size);
        mix(table.records);
        mix(table.keys);
        mix(table.fault);
        mix(table.outside ? 1u : 0u);
        mix(table.codes.Hash());
        mix(table.words.Hash());
        mix(table.readRanges.Hash());
    }
    mix(tables.ranges.size());
    for (const auto& [base, size] : tables.ranges) {
        mix(base);
        mix(size);
    }
    for (const auto threads : partialThreads(request)) mix(threads);
    mix(request.layout.pushConstantOffsetBytes);
    if (request.context.pixel) {
        for (const auto mapping : request.context.pixel->targetExportMapping) mix(mapping);
        for (const auto packing : request.context.pixel->targetExportPacking) mix(static_cast<std::uint64_t>(packing));
        mix(request.context.pixel->dualSourceBlend);
        mix(request.context.pixel->reversedBlend);
        mix(static_cast<std::uint64_t>(request.context.pixel->rasterPrimitive));
        mix(request.context.pixel->layerExported);
        const auto& merge = request.context.pixel->targetMerge;
        mix(merge.target);
        mix(merge.mask);
        mix(merge.samples);
        for (const auto word : merge.descriptor) mix(word);
    }
    if (request.context.vertex) {
        const auto& vertex = *request.context.vertex;
        const auto count = std::min<std::uint32_t>(vertex.resourcesNum, ShaderVertexStageInfo::MaxResources);
        mix(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            for (const auto field : vertex.resources[i].fields) mix(field);
            mix(vertex.resourcesDst[i].fetchIndex);
        }
    } else {
        mix(1ull << 32u);
    }
    return hash;
}

// The memo'd result of `source`'s variant for the snapshot (design13 R5): a hit returns the shared
// object, a miss materializes outside the source mutex and inserts (a concurrent miss's object is
// as good). `memoHit` reports the hit.
std::shared_ptr<const RecompileResult> materializePreparedMemoized(SourceEntry& source, const std::shared_ptr<const CompiledVariant>& variant, const RecompileRequest& request, const ResourceSnapshot& snapshot, bool cacheHit, bool* memoHit) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto hash = snapshotHash(request, snapshot);
    std::uint64_t index = 0;
    auto& counters = resultMemoCounters();
    {
        std::lock_guard lock(source.mutex);
        index = (variant->artifact.variantId * 0x9e3779b97f4a7c15ull) ^ hash;
        if (auto* set = memoSet(source, index)) {
            for (std::size_t way = 0; way < ResultMemoWays; ++way) {
                auto& entry = set[way];
                if (entry.result == nullptr || entry.variantId != variant->artifact.variantId || entry.hash != hash) continue;
                entry.used = counters.clock.fetch_add(1, std::memory_order_relaxed) + 1u;
                counters.hits.fetch_add(1, std::memory_order_relaxed);
                if (memoHit != nullptr) *memoHit = true;
                reportResultMemo();
                return entry.result;
            }
        }
    }
    const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto result = std::make_shared<RecompileResult>(materializeResult(*variant, request, snapshot));
    result->cacheHit = cacheHit;
    if (profile) counters.populateNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
    counters.misses.fetch_add(1, std::memory_order_relaxed);
    const auto bytes = resultBytes(*result);
    std::shared_ptr<const RecompileResult> shared = std::move(result);
    {
        std::lock_guard lock(source.mutex);
        if (source.memo == nullptr) {
            source.memo = std::make_unique<ResultMemoTable>();
            auto& registry = resultMemoRegistry();
            std::lock_guard registryLock(registry.mutex);
            registry.sources.push_back(source.weak_from_this());
        }
        auto* set = memoSet(source, index);
        auto* victim = set;
        bool present = false;
        for (std::size_t way = 0; way < ResultMemoWays && !present; ++way) {
            auto& entry = set[way];
            if (entry.result != nullptr && entry.variantId == variant->artifact.variantId && entry.hash == hash) {
                entry.used = counters.clock.fetch_add(1, std::memory_order_relaxed) + 1u;
                shared = entry.result;
                present = true;
            } else if (entry.used < victim->used) {
                victim = &entry;
            }
        }
        if (!present) {
            if (victim->result != nullptr) {
                counters.evictions.fetch_add(1, std::memory_order_relaxed);
                counters.heldBytes.fetch_sub(victim->bytes, std::memory_order_relaxed);
                counters.heldEntries.fetch_sub(1, std::memory_order_relaxed);
            }
            *victim = {variant->artifact.variantId, hash, shared, counters.clock.fetch_add(1, std::memory_order_relaxed) + 1u, bytes};
            counters.heldBytes.fetch_add(bytes, std::memory_order_relaxed);
            counters.heldEntries.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (const auto budget = resultMemoBudget(); budget != 0 && counters.heldBytes.load(std::memory_order_relaxed) > budget) trimResultMemo(budget);
    reportResultMemo();
    return shared;
}

std::shared_ptr<const RecompileResult> materializeMemoized(SourceEntry& source, const RecompileRequest& request, const ResourceSnapshot& snapshot, bool* memoHit) {
    std::shared_ptr<const CompiledVariant> variant;
    bool cacheHit = false;
    {
        std::lock_guard lock(source.mutex);
        variant = findOrCompileVariant(source, request, cacheHit);
    }
    return materializePreparedMemoized(source, variant, request, snapshot, cacheHit, memoHit);
}

// The capture already resolved the source entry (stage input validation included) and materialized
// the request over exactly the words the driver captured, so neither is repeated here.
std::shared_ptr<const RecompileResult> RecompileImpl(const RecompileRequest& request, const ResourceCapture& capture, bool* memoHit) {
    if (!request.useCache || capture.source == nullptr) {
        auto program = PrepareResourceProgram(request);
        const auto variant = compileVariant(request, std::move(program));
        return std::make_shared<const RecompileResult>(materializeResult(variant, request, capture.snapshot));
    }
    if (!ResultMemo()) return std::make_shared<const RecompileResult>(materializeVariant(*capture.source, request, capture.snapshot));
    return materializeMemoized(*capture.source, request, capture.snapshot, memoHit);
}

template <typename Impl>
auto recompileReporting(const RecompileRequest& request, Impl&& impl) -> decltype(impl()) {
    try {
        return impl();
    } catch (const std::exception& e) {
        constexpr auto requestSerializer = RequestSerializer{};
        const auto inputInfo = "\nRecompileRequest:\n" + requestSerializer.Serialize(request);
        throw std::runtime_error(std::string("ShaderRecompiler::Recompile: ") + e.what() + inputInfo);
    } catch (...) {
        throw std::runtime_error("ShaderRecompiler::Recompile: unknown exception");
    }
}

}

std::shared_ptr<const IrResourcePlan> GetResourcePlan(const RecompileRequest& request) {
    static_cast<void>(RequestInputInfo(request));
    if (request.useCache) {
        const auto source = getSource(request);
        return acquirePlan(request, source, false);
    }
    return makeResourcePlan(request);
}

void ReleaseUndrawnResourcePlan(const SourceHandle& handle) {
    releaseUndrawnPlan(handle);
}

namespace {

// The capture's materialization; with pure flat slots in the plan the walk's read addresses are
// traced into the capture (ResourceCapture::readTrace).
void materializeCapture(ResourceCapture& capture, const SrtRuntime& runtime) {
    const auto& plan = *capture.plan;
    SrtRuntime traced = runtime;
    traced.userDataUse = &capture.userDataUse;
    if (std::none_of(plan.pureFlatSlots.begin(), plan.pureFlatSlots.end(), [](std::uint8_t pure) { return pure != 0u; })) {
        ResourceMaterializer{}.Materialize(plan, traced, capture.snapshot);
        return;
    }
    traced.readTrace = &capture.readTrace;
    ResourceMaterializer{}.Materialize(plan, traced, capture.snapshot);
    auto& other = capture.readTrace.otherReads;
    std::sort(other.begin(), other.end());
    other.erase(std::unique(other.begin(), other.end()), other.end());
}

}

std::shared_ptr<const ResourceCapture> CaptureResources(const RecompileRequest& request, const SrtRuntime& runtime) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    // Validates the stage inputs once per request, as GetResourcePlan and Recompile(request) do.
    static_cast<void>(RequestInputInfo(request));
    auto capture = std::make_shared<ResourceCapture>();
    if (request.useCache) {
        capture->source = getSource(request);
        capture->plan = acquirePlan(request, capture->source, true);
    } else {
        capture->plan = makeResourcePlan(request);
    }
    if (profile) capture->sourceNanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
    materializeCapture(*capture, runtime);
    return capture;
}

std::shared_ptr<const SourceHandle> ResolveSource(const RecompileRequest& request) {
    if (!request.useCache) return nullptr;
    static_cast<void>(RequestInputInfo(request));
    return std::make_shared<const SourceHandle>(SourceHandle{getSource(request)});
}

std::shared_ptr<const SourceHandle> PrepareShader(const RecompileRequest& request) {
    return PrepareShader(request, nullptr);
}

std::shared_ptr<const SourceHandle> PrepareShader(const RecompileRequest& request, ShaderPreparationContext* preparation) {
    return recompileReporting(request, [&]() -> std::shared_ptr<const SourceHandle> {
        static_cast<void>(RequestInputInfo(request));
        auto handle = std::make_shared<SourceHandle>();
        handle->source = getSource(request, preparation, false);
        RecompileCacheKey::BuildInterface(request, handle->staticKey);
        handle->staticKey.push_back(HostSubgroupSize(request));
        bool cacheHit = false;
        std::lock_guard lock(handle->source->mutex);
        handle->artifact = findOrCompileVariant(*handle->source, request, cacheHit, preparation);
        return handle;
    });
}

bool MatchesPreparedShader(const RecompileRequest& request, const SourceHandle& handle) {
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    BuildPreparedShaderKey(request, key);
    return MatchesPreparedShader(request, handle, key);
}

void BuildPreparedShaderKey(const RecompileRequest& request, std::vector<std::uint64_t>& key) {
    RecompileCacheKey::BuildInterface(request, key);
    key.push_back(HostSubgroupSize(request));
}

bool MatchesPreparedShader(const RecompileRequest& request, const SourceHandle& handle, std::span<const std::uint64_t> key) {
    if (handle.source == nullptr || handle.artifact == nullptr) return false;
    const auto& layout = request.layout;
    const auto& prepared = handle.artifact->bindings.layout;
    if (layout.descriptorSet != handle.artifact->layout.descriptorSet || layout.firstBinding != handle.artifact->layout.firstBinding) return false;
    if (layout.pushConstantOffsetBytes % 4u != 0u || layout.pushConstantSizeBytes % 4u != 0u || layout.pushConstantOffsetBytes > NativePushConstantSize || layout.pushConstantSizeBytes > NativePushConstantSize - layout.pushConstantOffsetBytes) return false;
    const auto words = prepared.ShaderDataDwords();
    const bool usesPush = prepared.runtimeImageCount == 0u && words != 0u && words <= layout.pushConstantSizeBytes / 4u;
    if (prepared.UsesPushData() != usesPush) return false;
    if (!handle.artifact->artifact.stencilReferences.empty() && (prepared.stencilReferenceDword >= layout.pushConstantSizeBytes / 4u || prepared.stencilReferenceDword != (usesPush ? words : 0u))) return false;
    if (!std::ranges::equal(key, handle.staticKey) || handle.source->code.size() != request.shader.code.size()) return false;
    return handle.source->code.data() == request.shader.code.data() || std::ranges::equal(handle.source->code, request.shader.code);
}

std::span<const std::uint32_t> GetPreparedCode(const SourceHandle& handle) {
    if (handle.source == nullptr || handle.artifact == nullptr) throw std::runtime_error("ShaderRecompiler: prepared artifact is missing");
    return handle.source->code;
}

PreparedShaderInvocation::PreparedShaderInvocation(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle) : request(request), handle(handle) {
    this->request.context.linkedCallFallbacks = handle->source->linkedCallFallbacks;
}

std::optional<PreparedShaderInvocation> PreparedShaderInvocation::TryCreate(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle) {
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    BuildPreparedShaderKey(request, key);
    return TryCreate(request, handle, key);
}

std::optional<PreparedShaderInvocation> PreparedShaderInvocation::TryCreate(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle, std::span<const std::uint64_t> key) {
    if (handle == nullptr || !MatchesPreparedShader(request, *handle, key)) return std::nullopt;
    if (request.shader.stage != ShaderStage::Compute && request.shader.stage != ShaderStage::Fragment) ValidateRequestInputs(request);
    return PreparedShaderInvocation(request, handle);
}

std::shared_ptr<const ResourceCapture> PreparedShaderInvocation::Capture(const SrtRuntime& runtime) const {
    auto capture = std::make_shared<ResourceCapture>();
    capture->source = handle->source;
    capture->plan = acquirePlan(request, handle->source, true, handle.get());
    materializeCapture(*capture, runtime);
    return capture;
}

std::shared_ptr<const RecompileResult> PreparedShaderInvocation::Materialize(const ResourceCapture& capture) const {
    if (capture.source != handle->source) throw std::runtime_error("ShaderRecompiler: resource capture belongs to another prepared shader");
    if (request.useCache && ResultMemo()) return materializePreparedMemoized(*handle->source, handle->artifact, request, capture.snapshot, true, nullptr);
    auto result = std::make_shared<RecompileResult>(materializeResult(*handle->artifact, request, capture.snapshot));
    result->cacheHit = true;
    return result;
}

const CompiledShaderArtifact& GetPreparedArtifact(const SourceHandle& handle) {
    if (handle.artifact == nullptr) throw std::runtime_error("ShaderRecompiler: prepared artifact is missing");
    return handle.artifact->artifact;
}

std::shared_ptr<const RecompileResult> RegistrationResult(const RecompileRequest& request, const SourceHandle& handle) {
    if (handle.artifact == nullptr || !MatchesPreparedShader(request, handle)) throw std::runtime_error("ShaderRecompiler: prepared artifact does not match the static ABI");
    if (!request.target.registrationModules || !handle.artifact->artifact.registrationModule) return nullptr;
    const auto& info = handle.artifact->info.info;
    if (std::ranges::any_of(info.buffers, [](const BufferResource& buffer) { return buffer.descriptorFormatted; }) || std::ranges::any_of(info.samplers, [](const SamplerResource& sampler) { return FoldsTexelOffsets(sampler); })) return nullptr;
    ResourceSnapshot snapshot;
    snapshot.buffers.assign(info.buffers.size(), {{}, 4u});
    snapshot.images.assign(info.images.size(), {{}, 8u});
    snapshot.samplers.assign(info.samplers.size(), {{}, 4u});
    snapshot.userData.assign(RuntimeAbi::UserDataCapacity, 0u);
    snapshot.flattenedSrt.assign(1u, 0u);
    return std::make_shared<RecompileResult>(materializeResult(*handle.artifact, request, snapshot, true));
}

std::shared_ptr<const RecompileResult> MaterializeShader(const RecompileRequest& request, const ResourceCapture& capture, const SourceHandle& handle) {
    if (!MatchesPreparedShader(request, handle)) throw std::runtime_error("ShaderRecompiler: prepared artifact does not match the static ABI");
    if (capture.source != handle.source) throw std::runtime_error("ShaderRecompiler: resource capture belongs to another prepared shader");
    if (request.useCache && ResultMemo()) return materializePreparedMemoized(*handle.source, handle.artifact, request, capture.snapshot, true, nullptr);
    auto result = std::make_shared<RecompileResult>(materializeResult(*handle.artifact, request, capture.snapshot));
    result->cacheHit = true;
    return result;
}

std::shared_ptr<const ResourceCapture> CaptureResources(const RecompileRequest& request, const SrtRuntime& runtime, const SourceHandle& handle) {
    if (handle.source == nullptr) throw std::runtime_error("ShaderRecompiler: source handle is missing");
    if (handle.artifact != nullptr && !MatchesPreparedShader(request, handle)) throw std::runtime_error("ShaderRecompiler: prepared artifact does not match the static ABI");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    // The whole vertex family (Vertex, Local, TC, TE, Mesh) validates V# fields the memo key does not cover.
    if (request.shader.stage != ShaderStage::Compute && request.shader.stage != ShaderStage::Fragment) ValidateRequestInputs(request);
    auto capture = std::make_shared<ResourceCapture>();
    capture->source = handle.source;
    capture->plan = acquirePlan(request, handle.source, true, &handle);
    if (profile) capture->sourceNanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
    materializeCapture(*capture, runtime);
    return capture;
}

RecompileResult Recompile(const RecompileRequest& request) {
    return recompileReporting(request, [&] { return RecompileImpl(request); });
}

std::shared_ptr<const RecompileResult> Recompile(const RecompileRequest& request, const ResourceCapture& capture, bool* memoHit) {
    if (memoHit != nullptr) *memoHit = false;
    return recompileReporting(request, [&] { return RecompileImpl(request, capture, memoHit); });
}

void SetResultMemoBudget(std::uint64_t bytes) {
    resultMemoBudgetBytes().store(bytes, std::memory_order_relaxed);
    if (bytes != 0 && resultMemoCounters().heldBytes.load(std::memory_order_relaxed) > bytes) trimResultMemo(bytes);
}

void SetResultMemoTrimHook(std::function<void()> hook) {
    std::lock_guard lock(resultMemoTrimMutex());
    resultMemoTrimHook() = std::move(hook);
}

std::uint64_t ResultMemoBytes() {
    return resultMemoCounters().heldBytes.load(std::memory_order_relaxed);
}

}
