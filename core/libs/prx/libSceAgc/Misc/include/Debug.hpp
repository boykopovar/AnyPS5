#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_DEBUG_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_DEBUG_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcDebugRaiseException(std::uint32_t exceptionId);
int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags);
std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation();

}

#endif
