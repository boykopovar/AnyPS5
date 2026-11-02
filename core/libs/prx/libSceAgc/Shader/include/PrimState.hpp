#ifndef CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_PRIMSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_PRIMSTATE_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcCreatePrimState(ShaderRegister* cxRegs, ShaderRegister* ucRegs, const Shader* hs, const Shader* gs, std::uint32_t primType);
int APS5_VABI sceAgcUpdatePrimState(ShaderRegister* cxRegs, ShaderRegister* ucRegs, std::uint32_t primType);

}

#endif
