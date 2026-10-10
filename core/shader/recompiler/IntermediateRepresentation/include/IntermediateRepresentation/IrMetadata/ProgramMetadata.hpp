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

struct IrProgramMetadata {
    FailureKind cfgFailureKind = FailureKind::None;
    std::string failureReason;
    std::vector<BlockInfo> blockInfo;
    std::vector<ExportInfo> exportInfo;
    std::uint32_t pixelLinearInputs = 0;
    std::uint32_t pixelPerspectiveInputs = 0;
    std::vector<IrValue*> dynamicReads;
    bool shaderInfoComplete = false;
    IrBindingLayout bindings;
    bool bindingLayoutComplete = false;
    std::uint32_t tessellationLocalStride = 0;
    std::uint32_t tessellationControlStride = 0;
};

}

#endif
