#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_PROGRAMMETADATA_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_PROGRAMMETADATA_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include "IntermediateRepresentation/IrMetadata/ControlFlowInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/DescriptorBinding.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourceInfo.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ShaderRecompiler {

enum NanCheckLanes : std::uint8_t {
    NanCheckLowHalf = 1u,
    NanCheckHighHalf = 2u,
    NanCheckWord = 4u,
};

struct NanSpeculationCheck {
    IrValue* condition = nullptr;
    IrValue* value = nullptr;
    std::uint8_t lanes = 0;
    std::uint32_t region = 0;
    std::uint32_t execEpoch = 0;
};

struct IrProgramMetadata {
    FailureKind cfgFailureKind = FailureKind::None;
    std::string failureReason;
    std::vector<BlockInfo> blockInfo;
    std::vector<ExportInfo> exportInfo;
    std::uint32_t pixelLinearInputs = 0;
    std::uint32_t pixelPerspectiveInputs = 0;
    std::uint32_t pixelVertexOrderInputs = 0;
    std::uint32_t pixelFirstVertexInputs = 0;
    bool pixelIteratesSamples = false;
    bool barycentricEmulation = false;
    bool barycentricBuiltinsUnnormalized = false;
    bool invalidatesCaches = false;
    std::vector<IrValue*> dynamicReads;
    std::vector<NanSpeculationCheck> nanSpeculationChecks;
    std::vector<IrValue*> nanSpeculationLaneWrites;
    std::uint32_t execWrites = 0u;
    std::uint32_t nanSpeculationCandidates = 0u;
    std::uint32_t nanSpeculationKept = 0u;
    std::uint32_t nanSpeculationClean = 0u;
    std::uint32_t nanSpeculationDeclines = 0u;
    std::uint32_t nanSpeculationPrunedChecks = 0u;
    std::uint32_t nanSpeculationFastOps = 0u;
    std::uint32_t nanSpeculationRepairedOps = 0u;
    bool shaderInfoComplete = false;
    IrBindingLayout bindings;
    bool bindingLayoutComplete = false;
    std::uint32_t tessellationLocalStride = 0;
    std::uint32_t vectorWindowBase = 0;
    std::uint32_t vectorWindowSize = 0;
    bool vectorWindowIndexedWrites = false;
    bool splitState = false;
    std::uint32_t tessellationControlStride = 0;
    std::uint32_t tessellationPatchStride = 0;
    std::uint32_t tessellationGeometryItemDwords = 0;
    std::uint32_t tessellationSharedStride = 0;
};

}

#endif
