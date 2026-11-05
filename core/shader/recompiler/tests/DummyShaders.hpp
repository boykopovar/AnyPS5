#ifndef CORE_SHADER_RECOMPILER_INCLUDE_SHADER_RECOMPILER_DUMMY_SHADERS_HPP
#define CORE_SHADER_RECOMPILER_INCLUDE_SHADER_RECOMPILER_DUMMY_SHADERS_HPP

#include "Recompiler.hpp"

namespace ShaderRecompiler {

[[nodiscard]] RecompileResult RecompileDummy(const RecompileRequest& request);

}

#endif
