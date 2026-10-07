#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVMODULESETUP_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVMODULESETUP_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/BindingAllocator.hpp"
#include <cstdint>

namespace ShaderRecompiler {

void EmitModuleHeader(SpirvModule& module, const IrProgram& program, const BindingAllocationResult& bindings);
void EmitModuleHeader(SpirvEmitterState& state, const BindingAllocationResult& bindings);
void DefineModule(SpirvEmitterState& state);
std::uint32_t ExecutionModelForStage(IrShaderStage stage);
std::uint32_t DefineInterfaceVariable(SpirvEmitterState& state, std::uint32_t type, std::uint32_t storage,
                                      const char* name);

}

#endif
