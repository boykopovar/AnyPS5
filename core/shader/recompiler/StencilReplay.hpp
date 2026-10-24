#ifndef CORE_SHADER_RECOMPILER_STENCILREPLAY_HPP
#define CORE_SHADER_RECOMPILER_STENCILREPLAY_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <vector>

namespace ShaderRecompiler {

enum class StencilExportMode { None, Native, Replay, Attachment };

inline constexpr std::uint32_t StencilAttachmentLocation = 7u;

[[nodiscard]] StencilExportMode SelectStencilExportMode(const IrProgram& program, bool nativeExport, bool operationValue, bool earlyTests, bool ordered);

[[nodiscard]] std::vector<std::uint32_t> StencilReferenceValues(const IrProgram& program);
[[nodiscard]] std::vector<std::uint32_t> StencilReferenceValues(const IrProgram& program, std::uint32_t shift);
[[nodiscard]] bool StencilReplaySafe(const IrProgram& program);
[[nodiscard]] bool StencilReferenceDefinedForKeptInvocations(const IrProgram& program);

}

#endif
