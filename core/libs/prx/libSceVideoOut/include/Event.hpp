#ifndef CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_EVENT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_EVENT_HPP

#include <cstdint>

#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoOutAddFlipEvent(KernelEqueue eq, int handle, void* udata);
int APS5_VABI sceVideoOutAddOutputModeEvent(KernelEqueue eq, int handle, void* udata);
int APS5_VABI sceVideoOutAddPreVblankStartEvent(KernelEqueue eq, int handle, void* udata);
int APS5_VABI sceVideoOutAddVblankEvent(KernelEqueue eq, int handle, void* udata);

int APS5_VABI sceVideoOutDeleteFlipEvent(KernelEqueue eq, int handle);
int APS5_VABI sceVideoOutDeletePreVblankStartEvent(KernelEqueue eq, int handle);
int APS5_VABI sceVideoOutDeleteVblankEvent(KernelEqueue eq, int handle);

int APS5_VABI sceVideoOutGetEventCount(const KernelEvent* ev);
int APS5_VABI sceVideoOutGetEventData(const KernelEvent* ev, int64_t* data);
int APS5_VABI sceVideoOutGetEventId(const KernelEvent* ev);

#endif

}
