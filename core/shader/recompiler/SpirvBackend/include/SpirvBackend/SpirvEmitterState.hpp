#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTERSTATE_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTERSTATE_HPP

#include "SpirvBackend/SpirvModule.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ShaderRecompiler {

inline constexpr std::uint32_t NoImageComponent = 0xffffffffu;

struct SpirvInputBinding : StageInput {
    std::uint32_t variableId = 0;
};

struct SpirvOutputBinding : StageOutput {
    std::uint32_t variableId = 0;
    std::uint32_t meshDataVariable = 0;
};

struct RdnaImageDimensionInfo {
    RdnaImageDimension dimension;
    std::uint32_t spirvDimension;
    std::uint32_t coordinateComponents;
    std::uint32_t spatialComponents;
    std::uint32_t arrayed;
    std::uint32_t multisampled;
};

enum class VertexInputScalarKind { Float, Sint, Uint };

struct DppTargetLane {
    std::uint32_t lane = 0;
    std::uint32_t valid = 0;
};

struct ImageSampleLayout {
    std::uint32_t offset = NoImageComponent;
    std::uint32_t dref = NoImageComponent;
    std::uint32_t bias = NoImageComponent;
    std::uint32_t coord = 0;
    std::uint32_t lod = NoImageComponent;
    std::uint32_t clamp = NoImageComponent;
    std::uint32_t gradX = NoImageComponent;
    std::uint32_t gradY = NoImageComponent;
};

struct F32Class {
    std::uint32_t bits = 0;
    std::uint32_t nan = 0;
    std::uint32_t zero = 0;
};

struct MemoryResourceAccess {
    ResourceKind kind = ResourceKind::None;
    std::uint32_t objectPointer = 0;
    std::uint32_t length = 0;
    std::uint32_t indexOffset = 0;
    std::uint32_t byteOffset = 0;
    bool addIndexOffset = false;
    std::uint32_t memoryAccess = 0;
    std::uint32_t misalignment = 0;
};

struct SpirvTableImageVariable {
    DescriptorBindingKind kind = DescriptorBindingKind::Buffers;
    std::uint32_t variable = 0;
};

struct SpirvTableAccess {
    std::uint32_t element = 0;
    std::uint32_t dword3 = 0;
    std::uint32_t samplerElement = 0;
    bool active = false;
    bool uniform = false;
};

struct SpirvEmitterState {
    const ImageResource* runtimeImage = nullptr;
    SpirvTableAccess tableAccess;
    SpirvEmitterState(const IrProgram& program, const ShaderStageInputInfo& inputInfo);

    SpirvModule module;
    const IrProgram& program;
    ShaderStageInputInfo inputInfo;
    std::array<std::uint32_t, 10> tessVariables {};
    std::uint32_t tessInnerVariable = 0;

    SpirvRequirements requirements;
    std::uint32_t laneCount = 1;
    std::uint32_t hostSubgroupSize = 0;
    bool splitSubgroup = false;
    bool spannedWave = false;
    std::uint32_t spannedExchangeVariable = 0;
    bool narrowSubgroupClock = false;
    bool float64DenormPreserve = false;
    bool depthRangeRemap = false;
    std::uint32_t depthRemapClipIndex = 0;
    bool registrationModules = false;
    bool fragmentDescriptorSet = false;
    bool float16DenormPreserve = false;
    bool robustBufferAccess2 = false;
    bool float32FlushToZero = false;
    bool float32DenormPreserve = false;
    bool fusedExactArithmetic = false;
    bool meshBarycentricPixel = false;
    bool meshBarycentricVertex = false;
    std::uint32_t barycentricSmoothLocation = 0xffffffffu;
    std::uint32_t barycentricLinearLocation = 0xffffffffu;
    std::uint32_t barycentricPositionsLocation = 0xffffffffu;
    std::vector<std::uint32_t> meshExportedLocations;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> meshPerVertexLocations;
    std::vector<std::array<std::uint32_t, 3>> meshVertexParameters;
    std::uint32_t meshPositionStaging = 0;
    std::uint32_t meshClipSmooth = 0;
    std::uint32_t meshClipLinear = 0;
    std::uint32_t meshFlatPositions = 0;
    std::uint32_t pixelFlatPositions = 0;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> solvedBarycentrics;
    bool waveLdsMemoryBarriers = false;
    bool deviceScopeLdsMemoryBarriers = false;
    std::unordered_set<const IrValue*> sharedLaneValues;
    std::uint32_t laneHalf = 0;
    std::uint32_t spirvVersion = 0x00010300u;
    std::span<const std::uint32_t> supportedCapabilities;
    std::span<const std::string_view> supportedExtensions;
    bool nonConstantImageOffsets = false;
    bool tableIndexNonUniform = true;
    std::uint32_t storageBufferVariable = 0;
    std::uint32_t storageBufferU64Variable = 0;
    std::uint32_t storageBufferCoherentVariable = 0;
    std::uint32_t storageBufferU8Variable = 0;
    std::vector<std::uint32_t> memoryByteOffsets;
    std::map<std::array<std::uint32_t, 6>, std::uint32_t> formattedBufferFunctions;
    std::map<std::array<std::uint32_t, 6>, std::uint32_t> formattedGpuBufferFunctions;
    struct BufferLengthCacheEntry {
        std::array<const IrValue*, 4> descriptor{};
        std::uint32_t variable = 0;
        std::uint32_t element = 0;
        std::uint32_t label = 0;
        std::uint32_t length = 0;
    };
    std::vector<BufferLengthCacheEntry> bufferLengths;
    std::uint32_t bufferFormatTable = 0;
    std::uint32_t bdaPagetableVariable = 0;
    std::uint32_t faultBufferVariable = 0;
    std::uint32_t bdaPointerFunction = 0;
    // The lookup without fault recording that wide reads try first (see EmitBdaDwordReads); 0 when
    // every read takes the byte path.
    std::uint32_t bdaProbeFunction = 0;
    std::uint32_t bdaWritePointerFunction = 0;
    std::uint32_t bdaWriteProbeFunction = 0;
    std::uint32_t bdaAtomicPointerFunction = 0;
    std::uint32_t bdaNoteWriteFunction = 0;
    std::array<std::uint32_t, 2> bdaByteWriteFunctions {};
    std::uint32_t bdaFaultFunction = 0;
    bool nativeF16ModesEmitted = false;
    bool f16DenormPreserveEmitted = false;
    std::uint32_t dot2F32F16Function = 0;
    std::map<std::pair<bool, std::uint32_t>, std::uint32_t> interpolateFunctions;
    bool integerInterpolation = false;
    std::array<std::array<std::uint32_t, 2>, 2> bdaDwordReadFunctions {};
    std::array<std::array<std::uint32_t, 2>, 2> bdaSpanReadFunctions {};
    std::unordered_map<const IrValue*, std::pair<std::uint32_t, std::uint32_t>> bdaGroupedLoads;
    struct BdaAnchor {
        std::uint32_t lowRegister = 0;
        std::uint32_t highRegister = 0;
        std::uint32_t size = 0;
        std::uint32_t device = 0;
        std::uint32_t offset = 0;
    };
    std::vector<BdaAnchor> bdaAnchors;
    std::uint32_t bdaAnchorFunction = 0;
    std::uint32_t bdaStopValue = 0;
    std::uint32_t bdaPcOverride = 0;
    // False for programs with workgroup barriers: faulting BDA accesses then continue (see BdaInvocationsMayStop).
    bool bdaStopsInvocations = true;
    bool continueTarget = false;
    // Execution scope of the barriers that keep one guest wave's LDS accesses in program order across
    // host invocations (see WaveLdsScope); 0 when none are emitted.
    std::uint32_t waveLdsScope = 0;
    bool insideLdsLock = false;
    // Debug aid (APS5_LOOP_GUARD=<exits>): after that many evaluated loop exits an invocation leaves its
    // loops, and the first loop that ran out is reported as a LoopLimit fault instead of hanging the GPU.
    std::uint32_t loopGuardLimit = 0;
    std::uint32_t loopGuardVisits = 0;
    std::uint32_t loopGuardPc = 0;
    std::uint32_t srgbTableVariable = 0;
    std::uint32_t gcnBufferFormatVariable = 0;
    std::uint32_t gdsVariable = 0;
    std::uint32_t gdsLength = 0;
    std::uint32_t pushConstantVariable = 0;
    std::uint32_t shaderDataStorageVariable = 0;
    std::uint32_t flattenedSrtVariable = 0;
    std::uint32_t ldsVariable = 0;
    std::uint32_t ldsBufferVariable = 0;
    std::uint32_t ldsBufferU64Variable = 0;
    std::uint32_t ldsBufferBase = 0;
    bool pixelFunctionLdsBuffer = false;
    std::uint32_t numWorkgroupsVariable = 0;
    std::array<std::uint32_t, 2> scratchVariable {};
    std::array<std::uint32_t, 2> vectorWindowVariable {};
    std::uint32_t vectorWindowIndexed = 0;
    std::array<std::vector<std::uint32_t>, 2> vectorWindowSlots {};
    std::array<std::uint32_t, ImageBindingCount> imageVariables {};
    std::uint32_t samplerVariable = 0;
    std::vector<SpirvTableImageVariable> tableImageVariables;
    std::uint32_t tableSamplerVariable = 0;
    std::uint32_t tableMapVariable = 0;
    std::uint32_t mainFunc = 0;
    std::uint32_t meshGuestFunc = 0;
    std::uint32_t pixelGuestFunc = 0;
    std::uint32_t pixelStopFlag = 0;
    bool pixelStopStored = false;
    std::uint32_t meshAllocation = 0;
    std::uint32_t meshPrimitiveData = 0;
    std::uint32_t meshPrimitives = 0;
    std::uint32_t meshCull = 0;
    std::uint32_t entryLabel = 0;
    std::uint32_t currentLabel = 0;
    const IrBlock* currentBlock = nullptr;
    std::uint32_t pixelValidMaskVariable = 0;
    std::uint32_t mergeWordVariable = 0;
    std::uint32_t mergePendingVariable = 0;
    std::uint32_t quadPixelUncovered = 0;
    std::uint32_t sampleMaskInputVariable = 0;
    std::uint32_t quadSampleCoverage = 0;
    std::uint32_t sampleOrderWord = 0;
    std::uint32_t subgroupLocalInvocationIdVariable = 0;
    std::uint32_t subgroupIdVariable = 0;
    std::uint32_t perVertexVariable = 0;
    std::uint32_t barycentricReciprocalWVariable = 0;
    std::uint32_t pointSizeVariable = 0;
    std::uint32_t clipDistanceVariable = 0;
    std::optional<std::uint32_t> zeroPositionClipIndex;
    std::uint32_t cullDistanceVariable = 0;
    std::uint32_t layerVariable = 0;
    std::uint32_t viewportIndexVariable = 0;
    std::uint32_t depthVariable = 0;
    std::uint32_t sampleMaskVariable = 0;
    std::uint32_t stencilRefVariable = 0;
    bool stencilReplay = false;
    std::vector<SpirvInputBinding> inputs;
    std::vector<SpirvOutputBinding> outputs;
    std::vector<std::uint32_t> interfaceVariables;
    std::unordered_map<const IrBlock*, std::uint32_t> labels;
};

struct SpirvValueEmitContext {
    explicit SpirvValueEmitContext(SpirvEmitterState& state);

    std::uint32_t Def(const IrValue* value);
    std::uint32_t Arg(const IrValue& inst, std::size_t index);
    std::uint32_t HalfArg(const IrValue& inst, std::size_t index, std::uint32_t half);
    std::uint32_t Ballot(const IrValue* predicate);
    std::uint32_t FirstLane(std::uint32_t ballot);
    std::uint32_t Shuffle(const IrValue& inst, std::size_t index, std::uint32_t lane);
    std::uint32_t ShuffleInHalf(const IrValue& inst, std::size_t index, std::uint32_t lane);
    std::uint32_t Result(const IrValue& inst);
    std::uint32_t Define(const IrValue& inst, std::uint32_t value);
    std::uint32_t ResourceIndex(const IrValue* value, IrOpcode opcode);
    const IrValue* ImageAddress(const IrValue* value);
    const MemoryInfo& Memory(const IrValue& inst) const;
    const ExportInfo& Export(const IrValue& inst) const;
    std::uint32_t Label(const IrBlock* block) const;
    [[noreturn]] void Fail(const char* reason) const;
    [[noreturn]] void Fail(const IrValue& inst, const char* reason) const;

    SpirvEmitterState& state;
    std::unordered_map<const IrValue*, std::uint32_t> definitions;
    std::uint32_t scratchU32Variable = 0;
    SpirvValueEmitContext* otherHalf = nullptr;
    std::uint32_t half = 0;
};

struct SpirvDeferredPhiPatch {
    SpirvDeferredPhi phi;
    const IrValue* instruction = nullptr;
    std::uint32_t half = 0;
};

struct StructuredFunctionState {
    std::unordered_map<const IrBlock*, std::uint32_t> blockExitLabels;
    std::vector<SpirvDeferredPhiPatch> deferredPhis;
};

enum class SpirvFormatComponentType {
    Unknown,
    Uint,
    Sint,
    Unorm,
    Snorm,
    Uscaled,
    Sscaled,
    Float,
};

struct SpirvBufferFormatInfo {
    IrBufferFormat format = IrBufferFormat::Invalid;
    SpirvFormatComponentType type = SpirvFormatComponentType::Unknown;
    std::uint32_t componentCount = 0;
    std::uint32_t byteSize = 0;
    std::uint32_t componentBits[4] = {};
    std::uint32_t componentBitOffset[4] = {};
    bool packedBitfield = false;
};

}

#endif
