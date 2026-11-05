#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVINPUTOUTPUT_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVMEMORY_SPIRVINPUTOUTPUT_HPP

#include "SpirvBackend/SpirvEmitterState.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include "Recompiler.hpp"

namespace ShaderRecompiler {

std::vector<FragmentParameter> DescribeFragmentParameters(const IrProgram& program, const ShaderStageInputInfo& inputInfo);

std::uint32_t PixelParameterLocation(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsFlat(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsCustom(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsDefault(const SpirvEmitterState& state, std::uint32_t attr);
bool PixelParameterIsLinear(const SpirvEmitterState& state, std::uint32_t attr);
VertexInputScalarKind VertexParameterScalarKind(const SpirvEmitterState& state, std::uint32_t location);
std::uint32_t VertexParameterComponentCount(const SpirvInputBinding& input);
std::uint32_t VertexParameterScalarType(SpirvEmitterState& state, VertexInputScalarKind kind);
std::uint32_t OutputVariableForExport(const SpirvEmitterState& state, const ExportInfo& exp);
std::uint32_t InputVariableForKind(const SpirvEmitterState& state, StageInputKind kind);
const SpirvInputBinding* SpirvInputBindingForParameter(const SpirvEmitterState& state, std::uint32_t location);
std::uint32_t EmitVertexParameterComponentU32(SpirvEmitterState& state, const SpirvInputBinding& input, std::uint32_t component);
std::uint32_t EmitInputComponentU32(SpirvEmitterState& state, StageInputKind kind, std::uint32_t component);
std::uint32_t EmitLocalInvocationIndex(SpirvEmitterState& state);

}

#endif
