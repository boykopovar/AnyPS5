#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTERSTATE_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTERSTATE_HPP

#include "SpirvBackend/SpirvModule.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
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
};

struct SpirvEmitterState {
    SpirvEmitterState(const IrProgram& program, const ShaderStageInputInfo& inputInfo);

    SpirvModule module;
    const IrProgram& program;
    ShaderStageInputInfo inputInfo;
    std::array<std::uint32_t, 6> tessVariables {};
    std::uint32_t tessInnerVariable = 0;
    std::uint32_t tessPatchBase = 0;

    SpirvRequirements requirements;
    std::uint32_t laneCount = 1;
    std::uint32_t laneHalf = 0;
    std::uint32_t storageBufferVariable = 0;
    std::uint32_t storageBufferU64Variable = 0;
    std::array<std::uint32_t, ShaderInfo::MaxBuffers> memoryByteOffsets {};
    std::uint32_t bdaPagetableVariable = 0;
    std::uint32_t faultBufferVariable = 0;
    std::uint32_t bdaPointerFunction = 0;
    // False for programs with workgroup barriers: faulting BDA accesses then continue (see BdaInvocationsMayStop).
    bool bdaStopsInvocations = true;
    // Execution scope of the barriers that keep one guest wave's LDS accesses in program order across
    // host invocations (see WaveLdsScope); 0 when none are emitted.
    std::uint32_t waveLdsScope = 0;
    // Debug aid (APS5_LOOP_GUARD=<exits>): after that many evaluated loop exits an invocation leaves its
    // loops, and the first loop that ran out is reported as a LoopLimit fault instead of hanging the GPU.
    std::uint32_t loopGuardLimit = 0;
    std::uint32_t loopGuardVisits = 0;
    std::uint32_t loopGuardPc = 0;
    std::uint32_t gdsVariable = 0;
    std::uint32_t gdsLength = 0;
    std::uint32_t pushConstantVariable = 0;
    std::uint32_t shaderDataStorageVariable = 0;
    std::uint32_t flattenedSrtVariable = 0;
    std::uint32_t ldsVariable = 0;
    std::array<std::uint32_t, 2> scratchVariable {};
    std::array<std::uint32_t, ImageBindingCount> imageVariables {};
    std::uint32_t samplerVariable = 0;
    std::uint32_t mainFunc = 0;
    std::uint32_t meshGuestFunc = 0;
    std::uint32_t meshAllocation = 0;
    std::uint32_t meshPrimitiveData = 0;
    std::uint32_t meshPrimitives = 0;
    std::uint32_t meshCull = 0;
    std::uint32_t entryLabel = 0;
    std::uint32_t currentLabel = 0;
    const IrBlock* currentBlock = nullptr;
    std::uint32_t pixelValidMaskVariable = 0;
    std::uint32_t subgroupLocalInvocationIdVariable = 0;
    std::uint32_t perVertexVariable = 0;
    std::uint32_t pointSizeVariable = 0;
    std::uint32_t clipDistanceVariable = 0;
    std::uint32_t cullDistanceVariable = 0;
    std::uint32_t layerVariable = 0;
    std::uint32_t viewportIndexVariable = 0;
    std::uint32_t depthVariable = 0;
    std::uint32_t sampleMaskVariable = 0;
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
