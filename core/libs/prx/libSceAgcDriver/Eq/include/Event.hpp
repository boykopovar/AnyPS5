#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP

#include <cstdint>
#include "SceTypes.hpp"

bool AgcDriverDeliverEopInterrupt(std::uint32_t queue);

extern "C" int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata);
extern "C" int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id);
extern "C" int AgcDriverAddQueueEqEvent_nid_postfix(KernelEqueue eq, std::uint32_t queue, std::uintptr_t ident, void* udata);
extern "C" int AgcDriverDeleteQueueEqEvent_nid_postfix(KernelEqueue eq, std::uintptr_t ident);

#endif
