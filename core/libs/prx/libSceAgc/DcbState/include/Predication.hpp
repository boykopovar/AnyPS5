#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_PREDICATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_PREDICATION_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetPredication(CommandBuffer* buf, uint8_t condition, uint8_t op, uint8_t waitOp, const volatile void* address, uint32_t countInDwords);
std::uint32_t APS5_VABI sceAgcDcbSetZPassPredicationEnableGetSize();
std::uint32_t APS5_VABI sceAgcDcbSetPredicationDisableGetSize();
std::uint32_t APS5_VABI sceAgcDcbSetBoolPredicationEnableGetSize();

}

#endif
