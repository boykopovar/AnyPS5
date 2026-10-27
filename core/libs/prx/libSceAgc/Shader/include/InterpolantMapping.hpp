#ifndef CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_INTERPOLANTMAPPING_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_INTERPOLANTMAPPING_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcCreateInterpolantMapping_0100(ShaderRegister* regs, const Shader* gs, const Shader* ps);
int APS5_VABI sceAgcCreateInterpolantMapping_10000(ShaderRegister* regs, const Shader* gs, const Shader* ps);
int APS5_VABI sceAgcUpdateInterpolantMapping_10000(ShaderRegister* regs, const Shader* gs, const Shader* ps);
int APS5_VABI sceAgcCreateInterpolantMapping_nid_postfix(ShaderRegister* regs, const Shader* gs, const Shader* ps);
int APS5_VABI sceAgcUpdateInterpolantMapping_nid_postfix(ShaderRegister* regs, const Shader* gs, const Shader* ps);

}

#endif
