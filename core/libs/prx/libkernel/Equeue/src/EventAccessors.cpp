#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static const KernelEvent& requireEvent(const KernelEvent* ev, const char* caller) {
    if (ev == nullptr) {
        throw std::runtime_error(std::string(caller) + ": null event");
    }
    return *ev;
}

extern "C" {

intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent* ev) {
    return requireEvent(ev, __func__).data;
}

int APS5_VABI sceKernelGetEventError(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

intptr_t APS5_VABI sceKernelGetEventFflags(const KernelEvent* ev) {
    return static_cast<intptr_t>(requireEvent(ev, __func__).fflags);
}

int APS5_VABI sceKernelGetEventFilter(const KernelEvent* ev) {
    return requireEvent(ev, __func__).filter;
}

uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev) {
    return requireEvent(ev, __func__).ident;
}

void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev) {
    return requireEvent(ev, __func__).udata;
}

}
