#ifndef CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVINTERFACE_HPP
#define CORE_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVINTERFACE_HPP

#include "SpirvBackend/SpirvMemory/SpirvEmitterState.hpp"
#include "SpirvBackend/SpirvMemory/SpirvModule.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include <cstdint>

namespace ShaderRecompiler {

void EmitModuleHeader(SpirvModule& module, const IrProgram& program, const CompiledBindingLayout& bindings);
void EmitModuleHeader(SpirvEmitterState& state, const CompiledBindingLayout& bindings);
void DefineModule(SpirvEmitterState& state);
std::uint32_t DefineInterfaceVariable(SpirvEmitterState& state, std::uint32_t type, std::uint32_t storage, const char* name);
std::uint32_t ExecutionModelForStage(IrShaderStage stage);

std::uint32_t GlslStd450(SpirvEmitterState& state);
std::uint32_t InputVariableForKind(const SpirvEmitterState& state, StageInputKind kind);
const SpirvInputBinding* SpirvInputBindingForParameter(const SpirvEmitterState& state, std::uint32_t location);

std::uint32_t PixelParameterLocation(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsFlat(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsCustom(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsDefault(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsLinear(const SpirvEmitterState& state, std::uint32_t attr);

VertexInputScalarKind VertexParameterScalarKind(const SpirvEmitterState& state, std::uint32_t location);
std::uint32_t VertexParameterComponentCount(const SpirvInputBinding& input);
std::uint32_t VertexParameterScalarType(SpirvEmitterState& state, VertexInputScalarKind kind);

std::uint32_t OutputVariableForExport(const SpirvEmitterState& state, const ExportInfo& exp);

}

#endif
