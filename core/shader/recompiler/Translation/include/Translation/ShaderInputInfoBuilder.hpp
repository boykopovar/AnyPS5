#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SHADERINPUTINFOBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SHADERINPUTINFOBUILDER_HPP

#include "Optimization/ShaderStageInputInfo.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Recompiler.hpp"

namespace ShaderRecompiler {

// hostSubgroupSize is the device subgroup width; wave64 workgroups on 32-wide subgroups run each
// invocation as two guest lanes so a guest wave stays one host subgroup.
ShaderStageInputInfo BuildShaderStageInputInfo(ShaderStageKind stage, const GuestContext& context, std::uint32_t hostSubgroupSize);

}

#endif
