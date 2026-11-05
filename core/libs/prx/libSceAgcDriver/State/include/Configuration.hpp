#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_CONFIGURATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_CONFIGURATION_HPP

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2);
int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size);
int APS5_VABI sceAgcDriverGetTFRing(uintptr_t* base, uint32_t* size);

}

#endif
