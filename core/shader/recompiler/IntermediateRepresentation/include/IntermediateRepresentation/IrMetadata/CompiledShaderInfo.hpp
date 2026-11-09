#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_COMPILEDSHADERINFO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_COMPILEDSHADERINFO_HPP

#include "IntermediateRepresentation/IrMetadata/DescriptorBinding.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct CompiledShaderInfo {
    IrShaderStage stage = IrShaderStage::Unknown;
    std::uint64_t shaderHash = 0;
    std::uint32_t waveSize = 64;
    std::uint32_t userDataBase = 0;
    std::uint32_t userDataCount = 64;
    std::uint32_t scratchDwords = 0;
    std::uint32_t paramExportMask = 0;
    ShaderInfo info;
    IrBindingLayout bindings;
};

}

#endif
