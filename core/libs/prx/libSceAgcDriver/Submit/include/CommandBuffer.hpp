#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_COMMANDBUFFER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_COMMANDBUFFER_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceAgcDriverSubmitCommandBuffer(void* queueContext, const Packet* packet);
int APS5_VABI sceAgcDriverSubmitMultiCommandBuffers(void* queueContext, uint32_t* const* commandBuffers, const uint32_t* sizesInDwords, uint32_t count);

}

#endif
