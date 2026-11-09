#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTDIAGNOSTICS_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTDIAGNOSTICS_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <cstdint>
#include <string>

namespace ShaderRecompiler::Detail {

const char* StageName(IrShaderStage stage);
std::string Diagnostic(const IrResourcePlan& program, std::uint32_t pc, const std::string& message);
[[noreturn]] void Fail(const IrResourcePlan& program, std::uint32_t pc, const std::string& message);

}

#endif
