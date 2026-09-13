#include "shader/recompilier/Recompiler.hpp"

#include <stdexcept>

namespace ShaderRecompiler {

RecompileResult Recompile(const RecompileRequest& request) {
    (void)request;
    throw std::runtime_error("ShaderRecompiler::Recompile not implemented");
}

}
