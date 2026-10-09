#ifndef CORE_SHADER_RECOMPILER_CONTROLFLOW_USERDATACALLS_HPP
#define CORE_SHADER_RECOMPILER_CONTROLFLOW_USERDATACALLS_HPP

#include "Recompiler.hpp"
#include "Optimization/SrtWalker.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"

namespace ShaderRecompiler {

struct CapturedCallProgram {
    std::vector<std::uint32_t> code;
    std::vector<CapturedShaderCall> calls;
};

[[nodiscard]] CapturedCallProgram ResolveUserDataCalls(const RecompileRequest& request, SrtMemoryReader reader, void* context);
[[nodiscard]] RdnaProgram DecodeShaderProgram(const ShaderBinary& shader);

}

#endif
