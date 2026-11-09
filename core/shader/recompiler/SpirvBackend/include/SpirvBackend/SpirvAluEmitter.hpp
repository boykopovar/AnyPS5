#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVALUEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVALUEMITTER_HPP

#include "IntermediateRepresentation/IrValue.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#include <cstdint>

#include "SpirvBackend/SpirvEmitterState.hpp"

namespace ShaderRecompiler {

void EmitAluValue(SpirvModule& module, const IrValue& value, std::uint32_t resultId);

void EmitAluValue(SpirvValueEmitContext& context, const IrValue& value, std::uint32_t resultId);

}

#endif
