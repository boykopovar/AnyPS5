#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_CONFIGURATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_CONFIGURATION_HPP

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint16_t granularity, uint16_t buffering);
int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size);
int APS5_VABI sceAgcDriverGetTFRing(uintptr_t* base, uint32_t* size);

}

#endif
