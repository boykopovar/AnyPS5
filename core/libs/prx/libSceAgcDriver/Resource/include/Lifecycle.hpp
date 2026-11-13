#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_RESOURCE_INCLUDE_LIFECYCLE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_RESOURCE_INCLUDE_LIFECYCLE_HPP

#include "SceTypes.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcDriverInitResourceRegistration(void);
uint32_t APS5_VABI sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* sizeInBytes);

}

#endif
