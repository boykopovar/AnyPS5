#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_QUERY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_QUERY_HPP

#include "SceTypes.hpp"

extern "C" int APS5_VABI sceAgcDriverGetEqEventType(const KernelEvent* ev);
extern "C" uint32_t APS5_VABI sceAgcDriverGetEqContextId(const KernelEvent* ev);

#endif
