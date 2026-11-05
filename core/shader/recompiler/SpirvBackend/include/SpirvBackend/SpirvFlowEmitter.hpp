#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVFLOWEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVFLOWEMITTER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "SpirvBackend/SpirvModule.hpp"

#include "SpirvBackend/SpirvEmitterState.hpp"

namespace ShaderRecompiler {

void EmitControlFlow(SpirvModule& module, const IrProgram& program);

void EmitControlFlow(SpirvValueEmitContext& context, StructuredFunctionState& functionState, const IrProgram& program);

}

#endif
