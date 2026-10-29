#ifndef CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_CREATESHADER_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_CREATESHADER_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcCreateShader(Shader** dst, void* header, const volatile void* code);

}

#endif
