#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_INSTANCING_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_INSTANCING_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetNumInstances(CommandBuffer* buf, std::uint32_t numInstances);
std::uint32_t APS5_VABI sceAgcDcbSetNumInstancesGetSize();
uint32_t* APS5_VABI sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shaderType, const volatile void* indirectBaseAddr);

}

#endif
