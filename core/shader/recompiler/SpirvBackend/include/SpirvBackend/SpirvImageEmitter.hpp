#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVIMAGEEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVIMAGEEMITTER_HPP

#include "IntermediateRepresentation/IrValue.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#include <cstdint>

#include "SpirvBackend/SpirvEmitterState.hpp"

namespace ShaderRecompiler {

void EmitImageOperation(SpirvModule& module, const IrValue& value, std::uint32_t resultId);

void EmitImageOperation(SpirvValueEmitContext& context, const IrValue& value, std::uint32_t resultId);

}

#endif
