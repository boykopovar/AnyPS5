#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMODULEEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMODULEEMITTER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/BindingAllocator.hpp"
#include "SpirvBackend/SpirvModule.hpp"

#include "SpirvBackend/SpirvEmitterState.hpp"

namespace ShaderRecompiler {

void EmitModuleHeader(SpirvModule& module, const IrProgram& program, const CompiledBindingLayout& bindings);

void EmitModuleHeader(SpirvEmitterState& state, const CompiledBindingLayout& bindings);

}

#endif
