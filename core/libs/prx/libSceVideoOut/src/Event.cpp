#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Event.hpp"

extern "C" {

int APS5_VABI sceVideoOutAddFlipEvent(KernelEqueue eq, int handle, void* udata) {
    (void)eq;
    (void)handle;
    (void)udata;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutAddOutputModeEvent(KernelEqueue eq, int handle, void* udata) {
    (void)eq;
    (void)handle;
    (void)udata;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutAddPreVblankStartEvent(KernelEqueue eq, int handle, void* udata) {
    (void)eq;
    (void)handle;
    (void)udata;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutAddVblankEvent(KernelEqueue eq, int handle, void* udata) {
    (void)eq;
    (void)handle;
    (void)udata;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutDeleteFlipEvent(KernelEqueue eq, int handle) {
    (void)eq;
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutDeletePreVblankStartEvent(KernelEqueue eq, int handle) {
    (void)eq;
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutDeleteVblankEvent(KernelEqueue eq, int handle) {
    (void)eq;
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutGetEventCount(const KernelEvent* ev) {
    (void)ev;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutGetEventData(const KernelEvent* ev, int64_t* data) {
    (void)ev;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutGetEventId(const KernelEvent* ev) {
    (void)ev;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
