#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVFLOWEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVFLOWEMITTER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "SpirvBackend/SpirvModule.hpp"

#include "SpirvBackend/SpirvEmitterState.hpp"

namespace ShaderRecompiler {

void EmitControlFlow(SpirvValueEmitContext& context, StructuredFunctionState& functionState, const IrProgram& program);

}

#endif
