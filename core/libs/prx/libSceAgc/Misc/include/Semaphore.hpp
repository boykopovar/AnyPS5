#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SEMAPHORE_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_SEMAPHORE_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcSetAmmSemaphoreMemory(void* memory, std::uint64_t sizeInBytes);
int APS5_VABI sceAgcGetSemaphoreLabel(std::uint32_t index, void** label);

}

#endif
