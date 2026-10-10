#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_INSTRUCTIONTRANSLATOR_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_INSTRUCTIONTRANSLATOR_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Translation/EmbeddedVertexFetch.hpp"
#include <cstdint>
#include <optional>

namespace ShaderRecompiler {

enum class ShaderStageKind {
    Compute,
    Vertex,
    Pixel,
    Unknown,
    Fetch,
    Mesh,
    Local,
    TessellationControl,
    TessellationEvaluation
};

struct TranslateOptions {
    ShaderStageKind stage = ShaderStageKind::Unknown;
    std::uint32_t waveSize = 64;
    std::uint32_t userDataBaseRegister = 0;
    std::uint32_t userDataCount = 64;
    std::uint32_t scratchDwords = 0;
    std::uint32_t sharedMemoryBytes = 0;
    std::uint64_t shaderHash = 0;
    bool fragmentShaderBarycentricEnabled = false;
    std::optional<ShaderFloatMode> floatMode;
    ShaderStageInputInfo inputInfo;
    const EmbeddedFetchPlan* embeddedFetch = nullptr;
};

class InstructionTranslator {
public:
    [[nodiscard]] IrProgram Translate(const RdnaProgram& decoded, const ControlFlowGraph& cfg, const TranslateOptions& options) const;

private:
    void translateInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const ControlFlowGraph& cfg, const TranslateOptions& options) const;
};

}

#endif
