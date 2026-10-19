#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP

#include <cstdint>
#include "SceTypes.hpp"

void AgcDriverDeliverEopInterrupt(std::uint32_t queue);

extern "C" int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata);
extern "C" int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id);

#endif
