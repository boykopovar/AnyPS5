#ifndef CORE_SHADER_RECOMPILER_STENCILREPLAY_HPP
#define CORE_SHADER_RECOMPILER_STENCILREPLAY_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <vector>

namespace ShaderRecompiler {

[[nodiscard]] std::vector<std::uint32_t> StencilReferenceValues(const IrProgram& program);
[[nodiscard]] bool LayerReplaySafe(const IrProgram& program);
[[nodiscard]] bool StencilReplaySafe(const IrProgram& program);
[[nodiscard]] bool StencilReferenceDefinedForKeptInvocations(const IrProgram& program);

}

#endif
