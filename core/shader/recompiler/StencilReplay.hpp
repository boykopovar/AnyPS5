#ifndef CORE_SHADER_RECOMPILER_STENCILREPLAY_HPP
#define CORE_SHADER_RECOMPILER_STENCILREPLAY_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace ShaderRecompiler {

enum class StencilExportMode { None, Native, Replay, Attachment };

inline constexpr std::uint32_t StencilAttachmentLocation = 7u;

[[nodiscard]] StencilExportMode SelectStencilExportMode(const IrProgram& program, std::span<const std::uint32_t> capabilities, bool operationValue, bool earlyTests, bool ordered);

[[nodiscard]] std::vector<std::uint32_t> StencilReferenceValues(const IrProgram& program);
[[nodiscard]] std::vector<std::uint32_t> StencilReferenceValues(const IrProgram& program, std::uint32_t shift);
[[nodiscard]] bool LayerReplaySafe(const IrProgram& program);
[[nodiscard]] bool StencilReplayLanesFixed(std::span<const std::uint32_t> capabilities);
[[nodiscard]] bool StencilReplaySafe(const IrProgram& program, bool lanesFixed = false);
[[nodiscard]] bool StencilReplayNeedsStore(const IrProgram& program, std::span<const std::uint32_t> capabilities);
[[nodiscard]] bool StencilReferenceDefinedForKeptInvocations(const IrProgram& program);

}

#endif
