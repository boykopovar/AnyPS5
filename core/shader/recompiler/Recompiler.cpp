#include "Recompiler.hpp"
#include <algorithm>
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
#include <map>
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
#include "Optimization/include/Optimization/RequestMemoryView.hpp"
#include "Optimization/include/Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/include/Optimization/ResourceTracker.hpp"
#include "Optimization/include/Optimization/ShaderInfoCollector.hpp"
#include "Optimization/include/Optimization/SrtWalker.hpp"
#include "Optimization/include/Optimization/SsaBuilder.hpp"
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

// The host subgroup width wave64 programs are laid out for. Debug aid: APS5_SINGLE_LANE=<hex code
// addresses, comma separated, or "all"> keeps the listed programs at one guest lane per invocation.
std::uint32_t HostSubgroupSize(const RecompileRequest& request) {
    static const std::string list = [] { const char* text = std::getenv("APS5_SINGLE_LANE"); return text ? std::string(text) : std::string(); }();
    if (!list.empty()) {
        if (list == "all") return 64u;
        char address[32];
        std::snprintf(address, sizeof(address), "%llx", static_cast<unsigned long long>(request.shader.codeAddress));
        if (list.find(address) != std::string::npos) return 64u;
    }
    return request.target.subgroupSize;
}

ShaderStageInputInfo RequestInputInfo(const RecompileRequest& request) {
    const auto* mesh = request.graphics && request.graphics->mesh ? &*request.graphics->mesh : nullptr;
    const auto* tessellation = request.graphics && request.graphics->tessellation ? &*request.graphics->tessellation : nullptr;
    return BuildShaderStageInputInfo(toShaderStageKind(request.shader.stage), request.context, HostSubgroupSize(request), mesh, tessellation);
}

std::uint32_t DeviceMemoryLdsBytes(const RecompileRequest& request) {
    if (!request.context.compute.has_value()) return 0u;
    const auto bytes = static_cast<std::uint64_t>(request.context.compute->ldsSizeDwords) * 4u;
    return bytes + 4u > request.target.maxWorkgroupSharedMemoryBytes ? static_cast<std::uint32_t>(bytes) : 0u;
}

}

struct PreparedControlFlow {
    PreparedControlFlow(const RecompileRequest& request, const SwappcInfo& swappcInfo)
        : stage(request.shader.stage), swappc(swappcInfo), code(request.shader.code.begin(), request.shader.code.end()),
          decoded(RdnaInstructionDecoder{}.Decode(code)), cfg(GraphBuilder{}.Build(decoded, &swappc)) {
        Structurizer{}.Structurize(cfg);
    }

    PreparedControlFlow(const PreparedControlFlow&) = delete;
    PreparedControlFlow& operator=(const PreparedControlFlow&) = delete;

    bool Matches(const RecompileRequest& request, const SwappcInfo& swappcInfo) const {
        return stage == request.shader.stage && swappc.fetchCallAllowed == swappcInfo.fetchCallAllowed &&
               swappc.userDataBaseRegister == swappcInfo.userDataBaseRegister && swappc.userDataCount == swappcInfo.userDataCount &&
               std::ranges::equal(code, request.shader.code);
    }

    ShaderStage stage;
    SwappcInfo swappc;
    std::vector<std::uint32_t> code;
    RdnaProgram decoded;
    ControlFlowGraph cfg;
};

std::shared_ptr<const PreparedControlFlow> ShaderPreparationContext::AcquireFrontend(const RecompileRequest& request) {
    const auto inputInfo = RequestInputInfo(request);
    const SwappcInfo swappc{inputInfo.vertex != nullptr, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size())};
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

IrProgram PrepareResourceProgram(const RecompileRequest& request) {
    return PrepareResourceProgram(request, nullptr);
}

IrProgram PrepareResourceProgram(const RecompileRequest& request, ShaderPreparationContext* preparation) {
    ShaderPreparationContext local;
    const auto frontend = (preparation != nullptr ? *preparation : local).AcquireFrontend(request);
    const auto stageKind = toShaderStageKind(request.shader.stage);
    const auto inputInfo = RequestInputInfo(request);
    const auto& decoded = frontend->decoded;
    const auto& cfg = frontend->cfg;

    TranslateOptions translateOptions {};
    translateOptions.stage = stageKind;
    translateOptions.shaderHash = request.shader.codeAddress;
    translateOptions.waveSize = request.context.waveSize;
    translateOptions.userDataBaseRegister = request.context.userDataBaseRegister;
    translateOptions.userDataCount = static_cast<std::uint32_t>(request.context.userData.size());
    translateOptions.scratchDwords = request.context.compute.has_value() ? request.context.compute->scratchDwords : 0u;
    translateOptions.sharedMemoryBytes = DeviceMemoryLdsBytes(request);
    translateOptions.fragmentShaderBarycentricEnabled = request.target.fragmentShaderBarycentricEnabled;
    translateOptions.floatMode = request.context.floatMode;
    translateOptions.inputInfo = inputInfo;

    constexpr InstructionTranslator translator;

    EmbeddedFetchPlan embeddedFetch;
    if ((stageKind == ShaderStageKind::Vertex || stageKind == ShaderStageKind::Local) && inputInfo.vertex != nullptr && inputInfo.vertex->fetchEmbedded) {
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

        if (maskedSelectEliminator.Eliminate(program).removedSelects != 0u) {
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

    constexpr SrtWalker srtWalker;
    srtWalker.BuildPlan(program);
    deadCodeEliminator.Eliminate(program);
    dumpIr("srt");

    constexpr ResourceTracker resourceTracker;
    resourceTracker.Track(program);
    deadCodeEliminator.Eliminate(program);
    dumpIr("resources");
    program.Resources().srgbDecodeFormats = request.target.srgbDecodeFormats;

    return program;
}

// A materialized result of one variant over one snapshot (Recompile(request, capture)): the
// shared immutable object every later capture that reproduces the snapshot receives, so Populate
// and the per-request copy run once per distinct snapshot.
struct ResultMemoEntry {
    std::uint64_t variantId;
    std::uint64_t hash;
    std::shared_ptr<const RecompileResult> result;
};

struct EmissionFailure {
    std::uint64_t codeAddress;
    BindingLayout layout;
    std::exception_ptr failure;
};

struct SourceEntry {
    std::mutex mutex;
    // The code the entry was built for: the key carries only a hash of it, so a candidate entry is
    // accepted only when its code matches word for word. Owned here because the request's span
    // points into a registration the driver may replace while the entry lives on.
    std::vector<std::uint32_t> code;
    std::shared_ptr<const IrResourcePlan> plan;
    // A plan build that threw (an unsupported resource chain or control flow) is remembered and
    // rethrown: the front end ran every pass before failing, ~13 ms per dispatch of a shader the
    // title issues every frame (0x1048947300 at the intro video). APS5_NO_FAILURE_MEMO=1 rebuilds.
    std::exception_ptr planFailure;
    std::unique_ptr<IrProgram> program;
    std::vector<std::shared_ptr<const CompiledVariant>> variants;
    std::vector<EmissionFailure> emissionFailures;
    // The result memo, most recently used first, at most ResultMemoEntries (under mutex).
    std::list<ResultMemoEntry> memo;
    std::unordered_map<std::uint64_t, std::list<ResultMemoEntry>::iterator> memoIndex;
};

namespace {

struct ResourceProgram {
    explicit ResourceProgram(const RecompileRequest& request, ShaderPreparationContext* preparation = nullptr) : program(std::make_unique<IrProgram>(PrepareResourceProgram(request, preparation))), plan(std::make_shared<const IrResourcePlan>(ResourceMaterializer{}.ExtractPlan(*program))) {}

    std::unique_ptr<IrProgram> program;
    std::shared_ptr<const IrResourcePlan> plan;
};

std::shared_ptr<const IrResourcePlan> makeResourcePlan(const RecompileRequest& request) {
    return ResourceProgram(request).plan;
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

std::shared_ptr<SourceEntry> getSource(const RecompileRequest& request, ShaderPreparationContext* preparation = nullptr) {
    static std::shared_mutex mutex;
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
            if (FailureMemo() && source->planFailure) std::rethrow_exception(source->planFailure);
            try {
                ResourceProgram resource(request, preparation);
                source->plan = std::move(resource.plan);
                source->program = std::move(resource.program);
            } catch (...) {
                if (FailureMemo()) source->planFailure = std::current_exception();
                throw;
            }
        }
    }
    return source;
}

std::array<std::uint32_t, 3> partialThreads(const RecompileRequest& request) {
    return request.context.compute ? request.context.compute->partialThreads : std::array<std::uint32_t, 3>{};
}

std::uint64_t nextVariantId() {
    static std::atomic<std::uint64_t> variants{0};
    return variants.fetch_add(1, std::memory_order_relaxed) + 1;
}

CompiledVariant compileVariant(const RecompileRequest& request, IrProgram program, std::exception_ptr* emissionFailure = nullptr) try {
    const auto inputInfo = RequestInputInfo(request);
    constexpr DeadCodeEliminator deadCodeEliminator;
    constexpr ResourceMaterializer resourceMaterializer;
    const bool nativeSampleOffsets = request.target.nonConstantImageOffsets && std::find(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end(), spv::CapabilityImageGatherExtended) != request.target.supportedCapabilities.end();
    resourceMaterializer.ApplyStaticInterface(program, nativeSampleOffsets);

    deadCodeEliminator.RemoveIdentities(program);
    deadCodeEliminator.Eliminate(program);

    constexpr ShaderInfoCollector shaderInfoCollector;
    shaderInfoCollector.Collect(program, inputInfo);

    constexpr BindingAllocator bindingAllocator;
    auto bindings = bindingAllocator.Allocate(program, request.layout);

    SpirvTargetOptions targetOptions {};
    targetOptions.vulkanVersion = request.target.vulkanVersion;
    targetOptions.spirvVersion = request.target.spirvVersion;
    targetOptions.subgroupSize = request.target.subgroupSize;
    targetOptions.bdaAbiVersion = request.target.bdaAbiVersion;
    targetOptions.supportedCapabilities = request.target.supportedCapabilities;
    targetOptions.supportedExtensions = request.target.supportedExtensions;
    targetOptions.nonConstantImageOffsets = request.target.nonConstantImageOffsets;
    targetOptions.narrowSubgroupClock = request.target.narrowSubgroupClock;

    constexpr SpirvEmitter spirvEmitter;
    CompiledShaderArtifact result;
    result.variantId = nextVariantId();
    result.spirv = spirvEmitter.Emit(program, inputInfo, bindings, targetOptions);

    result.bdaAbiVersion = program.Info().usesDma || program.Info().usesFaultBuffer ? request.target.bdaAbiVersion : 0u;
    result.memoryOffsetDword = bindings.layout.memoryOffsetDword;
    result.shaderDataDwords = bindings.layout.ShaderDataDwords();
    result.imageMetadataDword = bindings.layout.ImageMetadataDword();
    result.runtimeImageCount = bindings.layout.runtimeImageCount;
    result.hostSubgroupSize = HostSubgroupSize(request);
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
        if (program.Metadata().barycentricEmulation) result.barycentricEmulation = {true, reads(StageInputKind::BaryCoordSmooth), reads(StageInputKind::BaryCoordNoPerspective)};
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

    return {request.layout, std::move(program).TakeCompiledInfo(), std::move(static_cast<CompiledBindingLayout&>(bindings)), std::move(result)};
} catch (const std::bad_alloc&) {
    throw;
} catch (...) {
    if (emissionFailure != nullptr) *emissionFailure = std::current_exception();
    throw;
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
    std::set<std::uint32_t> specializedIds;
    const auto& words = source.Words();
    if (words.size() < 5u || words[0] != spv::MagicNumber) throw std::runtime_error("invalid prepared specialization module");
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        if (count == 0u || count > words.size() - cursor) throw std::runtime_error("truncated prepared specialization instruction");
        if (op == spv::OpDecorate && count == 4u && words[cursor + 2u] == spv::DecorationSpecId) {
            const auto found = supplied.find(words[cursor + 3u]);
            if (found == supplied.end() || !values.emplace(words[cursor + 1u], found->second).second) throw std::runtime_error("missing or duplicate prepared specialization value");
            specializedIds.insert(found->first);
        }
        cursor += count;
    }
    std::vector<std::uint32_t> materialized(words.begin(), words.begin() + 5);
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        if (op == spv::OpSpecConstant) {
            if (count != 4u || !values.contains(words[cursor + 2u])) throw std::runtime_error("invalid prepared specialization constant");
            materialized.insert(materialized.end(), {(4u << 16u) | spv::OpConstant, words[cursor + 1u], words[cursor + 2u], values.at(words[cursor + 2u])});
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
    for (std::size_t cursor = 5; cursor < materialized.size();) {
        const auto count = materialized[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(materialized[cursor] & 0xffffu);
        if (op == spv::OpDecorate && count == 4u && materialized[cursor + 2u] == spv::DecorationBinding) bindingNumbers.emplace(materialized[cursor + 1u], materialized[cursor + 3u]);
        if (op == spv::OpVariable && count >= 4u) {
            if (const auto found = bindingNumbers.find(materialized[cursor + 2u]); found != bindingNumbers.end()) module->bindings.push_back(found->second);
            module->pushData |= materialized[cursor + 3u] == spv::StorageClassPushConstant;
        }
        cursor += count;
    }
    module->spirv = std::move(materialized);
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
    std::shared_mutex mutex;
    std::map<std::vector<std::uint32_t>, std::shared_ptr<PreparedModuleEntry>> modules;
};

std::shared_ptr<PreparedBindingPlan> preparedBindingPlan(const CompiledVariant& variant, const ResourceSnapshot& snapshot, std::span<const std::uint8_t> exports) {
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
    static std::shared_mutex mutex;
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
            plan->bindings = DescriptorBindingBuilder{}.Prepare(variant.bindings.layout, variant.info.info, variant.info.stage, snapshot, exports);
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
    static std::shared_mutex mutex;
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

RecompileResult materializeResult(const CompiledVariant& variant, const RecompileRequest& request, const ResourceSnapshot& snapshot) {
    RecompileResult result;
    const auto& artifact = variant.artifact;
    static_cast<CompiledShaderArtifact&>(result) = {
        .memoryOffsetDword = artifact.memoryOffsetDword,
        .shaderDataDwords = artifact.shaderDataDwords,
        .imageMetadataDword = artifact.imageMetadataDword,
        .runtimeImageCount = artifact.runtimeImageCount,
        .runtimeImageResources = artifact.runtimeImageResources,
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
        .parameterExports = artifact.parameterExports,
        .fragmentParameters = artifact.fragmentParameters,
        .barycentricEmulation = artifact.barycentricEmulation,
        .variantId = artifact.variantId
    };
    const auto plan = preparedBindingPlan(variant, snapshot, request.context.pixel ? std::span<const std::uint8_t>(request.context.pixel->targetExportMapping) : std::span<const std::uint8_t>{});
    const DescriptorBindingPlan* bindingPlan = &plan->bindings;
    std::shared_ptr<PreparedModuleEntry> entry;
    struct LocalModuleKeyStorage {};
    auto& moduleKey = HostThreadLocal<std::vector<std::uint32_t>, LocalModuleKeyStorage>();
    moduleKey.clear();
    if (variant.bindings.layout.UsesPushData()) moduleKey.push_back(request.layout.pushConstantOffsetBytes / 4u);
    if (request.context.pixel) {
        for (const auto packing : request.context.pixel->targetExportPacking) moduleKey.push_back(static_cast<std::uint32_t>(packing));
        moduleKey.push_back(request.context.pixel->dualSourceBlend ? 1u : 0u);
    }
    result.vertexAttributes.reserve(result.vertexInputs.size());
    std::array<std::uint32_t, ShaderVertexStageInfo::MaxResources> vertexClasses{};
    for (const auto& input : result.vertexInputs) {
        if (!request.context.vertex || input.location >= request.context.vertex->resourcesNum || input.location >= request.context.vertex->resources.size()) throw std::runtime_error("Shader cache: invalid vertex attribute metadata");
        const auto& vertex = *request.context.vertex;
        const auto& resource = vertex.resources[input.location];
        const auto fetchIndex = vertex.fetchEmbedded ? vertex.resourcesDst[input.location].fetchIndex : input.fetchIndex;
        result.vertexAttributes.push_back({input.location, input.components, resource, fetchIndex});
        if (!artifact.vertexInputPatches.empty()) {
            const auto numeric = VertexInputNumericClass(static_cast<IrBufferFormat>((resource.fields[3] >> 12u) & 0x7fu));
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
            moduleKey.push_back(selectors);
        }
    }
    if (!plan->bindings.specialization.empty() || variant.bindings.layout.UsesPushData() || !artifact.vertexInputPatches.empty() || request.context.pixel) {
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
            if (request.context.pixel) {
                for (std::uint32_t target = 0; target < request.context.pixel->targetExportPacking.size(); ++target) constants.push_back({PipelineSpecialization::ExportPackingBase + target, moduleKey[index++]});
                constants.push_back({PipelineSpecialization::DualSourceBlend, moduleKey[index++]});
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
            const auto module = specializeModule(variant.artifact, vertexClasses, constants, request.target);
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
    const auto tables = ResourceMaterializer::ResolveImageTables(variant.info.info, snapshot, artifact.variantId);
    BindingAllocationResult bindings;
    DescriptorBindingBuilder{}.Populate(bindings, variant.bindings, *bindingPlan, variant.info.userDataBase, snapshot, partialThreads(request), &tables);
    result.workgroupMemoryDwords = WorkgroupMemoryStrideDwords(variant.info.info);
    result.bindings = std::move(bindings.bindings);
    result.pushConstants = std::move(bindings.pushConstants);
    result.poisonedSrtReads = static_cast<std::uint32_t>(snapshot.srtPoison.size());
    if (!tables.map.empty()) {
        result.imageTablePoison.reserve(tables.poison.size());
        for (const auto& poison : tables.poison) result.imageTablePoison.push_back({poison.words, poison.dwordCount, poison.resource, static_cast<std::uint32_t>(poison.reason)});
        result.imageTableRanges = snapshot.tables.ranges;
        result.imageTableShader = snapshot.tables.shader;
        result.imageTableFaults = tables.faults;
    }
    return result;
}

bool sameLayout(const BindingLayout& left, const BindingLayout& right) {
    return left.descriptorSet == right.descriptorSet && left.firstBinding == right.firstBinding && left.pushConstantOffsetBytes == right.pushConstantOffsetBytes && left.pushConstantSizeBytes == right.pushConstantSizeBytes;
}

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
        auto program = source.program != nullptr ? std::move(*source.program) : PrepareResourceProgram(request, preparation);
        source.program.reset();
        std::exception_ptr emissionFailure;
        try {
            variant = std::make_shared<const CompiledVariant>(compileVariant(request, std::move(program), &emissionFailure));
        } catch (...) {
            if (emissionFailure != nullptr && FailureMemo()) source.emissionFailures.push_back({request.shader.codeAddress, request.layout, emissionFailure});
            throw;
        }
        if (disk) ShaderDiskCache::Store(std::move(diskKey), variant);
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

RecompileResult RecompileImpl(const RecompileRequest& request) {
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
    materializer.Materialize(*source->plan, runtime, snapshot);
    return materializeVariant(*source, request, snapshot);
}

// APS5_NO_RESULT_MEMO=1: every Recompile(request, capture) materializes its own result as before.
bool ResultMemo() {
    static const bool resultMemo = std::getenv("APS5_NO_RESULT_MEMO") == nullptr;
    return resultMemo;
}

constexpr std::size_t ResultMemoEntries = 256;

struct ResultMemoCounters {
    std::atomic<std::uint64_t> hits{0}, misses{0}, evictions{0}, populateNanoseconds{0};
    std::atomic<std::int64_t> lastReport{0};
};

ResultMemoCounters& resultMemoCounters() {
    static ResultMemoCounters counters;
    return counters;
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
    const auto populate = counters.populateNanoseconds.exchange(0, std::memory_order_relaxed);
    std::fprintf(stderr, "[recompile] result memo (10 s): %llu hits, %llu misses (%.1f%% hits), Populate %.1f us per miss / %.1f ms in total, %llu evictions\n", static_cast<unsigned long long>(hits), static_cast<unsigned long long>(misses), hits + misses != 0 ? 100.0 * static_cast<double>(hits) / static_cast<double>(hits + misses) : 0.0, misses != 0 ? static_cast<double>(populate) / 1000.0 / static_cast<double>(misses) : 0.0, static_cast<double>(populate) / 1e6, static_cast<unsigned long long>(evictions));
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
        mixWords(table.codes);
    }
    mixDescriptors(tables.words);
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
        const auto found = source.memoIndex.find(index);
        if (found != source.memoIndex.end() && found->second->variantId == variant->artifact.variantId && found->second->hash == hash) {
            source.memo.splice(source.memo.begin(), source.memo, found->second);
            counters.hits.fetch_add(1, std::memory_order_relaxed);
            if (memoHit != nullptr) *memoHit = true;
            reportResultMemo();
            return found->second->result;
        }
    }
    const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto result = std::make_shared<RecompileResult>(materializeResult(*variant, request, snapshot));
    result->cacheHit = cacheHit;
    if (profile) counters.populateNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
    counters.misses.fetch_add(1, std::memory_order_relaxed);
    std::shared_ptr<const RecompileResult> shared = std::move(result);
    {
        std::lock_guard lock(source.mutex);
        const auto found = source.memoIndex.find(index);
        if (found != source.memoIndex.end()) {
            if (found->second->variantId == variant->artifact.variantId && found->second->hash == hash) {
                source.memo.splice(source.memo.begin(), source.memo, found->second);
                shared = found->second->result;
            } else {
                source.memo.erase(found->second);
                source.memoIndex.erase(found);
            }
        }
        if (source.memoIndex.find(index) == source.memoIndex.end()) {
            source.memo.push_front({variant->artifact.variantId, hash, shared});
            source.memoIndex.emplace(index, source.memo.begin());
            while (source.memo.size() > ResultMemoEntries) {
                const auto& last = source.memo.back();
                source.memoIndex.erase((last.variantId * 0x9e3779b97f4a7c15ull) ^ last.hash);
                source.memo.pop_back();
                counters.evictions.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
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
    if (request.useCache) return getSource(request)->plan;
    return makeResourcePlan(request);
}

namespace {

// The capture's materialization; with pure flat slots in the plan the walk's read addresses are
// traced into the capture (ResourceCapture::readTrace).
void materializeCapture(ResourceCapture& capture, const SrtRuntime& runtime) {
    const auto& plan = *capture.plan;
    if (std::none_of(plan.pureFlatSlots.begin(), plan.pureFlatSlots.end(), [](std::uint8_t pure) { return pure != 0u; })) {
        ResourceMaterializer{}.Materialize(plan, runtime, capture.snapshot);
        return;
    }
    SrtRuntime traced = runtime;
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
        capture->plan = capture->source->plan;
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
        handle->source = getSource(request, preparation);
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
    if (!std::ranges::equal(key, handle.staticKey) || handle.source->code.size() != request.shader.code.size()) return false;
    return handle.source->code.data() == request.shader.code.data() || std::ranges::equal(handle.source->code, request.shader.code);
}

std::span<const std::uint32_t> GetPreparedCode(const SourceHandle& handle) {
    if (handle.source == nullptr || handle.artifact == nullptr) throw std::runtime_error("ShaderRecompiler: prepared artifact is missing");
    return handle.source->code;
}

PreparedShaderInvocation::PreparedShaderInvocation(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle) : request(request), handle(handle) {}

std::optional<PreparedShaderInvocation> PreparedShaderInvocation::TryCreate(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle) {
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    BuildPreparedShaderKey(request, key);
    return TryCreate(request, handle, key);
}

std::optional<PreparedShaderInvocation> PreparedShaderInvocation::TryCreate(const RecompileRequest& request, const std::shared_ptr<const SourceHandle>& handle, std::span<const std::uint64_t> key) {
    if (handle == nullptr || !MatchesPreparedShader(request, *handle, key)) return std::nullopt;
    if (request.shader.stage != ShaderStage::Compute && request.shader.stage != ShaderStage::Fragment) static_cast<void>(RequestInputInfo(request));
    return PreparedShaderInvocation(request, handle);
}

std::shared_ptr<const ResourceCapture> PreparedShaderInvocation::Capture(const SrtRuntime& runtime) const {
    auto capture = std::make_shared<ResourceCapture>();
    capture->source = handle->source;
    capture->plan = handle->source->plan;
    materializeCapture(*capture, runtime);
    return capture;
}

std::shared_ptr<const RecompileResult> PreparedShaderInvocation::Materialize(const ResourceCapture& capture) const {
    if (capture.source != handle->source || capture.plan != handle->source->plan) throw std::runtime_error("ShaderRecompiler: resource capture belongs to another prepared shader");
    if (request.useCache && ResultMemo()) return materializePreparedMemoized(*handle->source, handle->artifact, request, capture.snapshot, true, nullptr);
    auto result = std::make_shared<RecompileResult>(materializeResult(*handle->artifact, request, capture.snapshot));
    result->cacheHit = true;
    return result;
}

const CompiledShaderArtifact& GetPreparedArtifact(const SourceHandle& handle) {
    if (handle.artifact == nullptr) throw std::runtime_error("ShaderRecompiler: prepared artifact is missing");
    return handle.artifact->artifact;
}

std::shared_ptr<const RecompileResult> MaterializeShader(const RecompileRequest& request, const ResourceCapture& capture, const SourceHandle& handle) {
    if (!MatchesPreparedShader(request, handle)) throw std::runtime_error("ShaderRecompiler: prepared artifact does not match the static ABI");
    if (capture.source != handle.source || capture.plan != handle.source->plan) throw std::runtime_error("ShaderRecompiler: resource capture belongs to another prepared shader");
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
    if (request.shader.stage != ShaderStage::Compute && request.shader.stage != ShaderStage::Fragment) static_cast<void>(RequestInputInfo(request));
    auto capture = std::make_shared<ResourceCapture>();
    capture->source = handle.source;
    capture->plan = handle.source->plan;
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

}
