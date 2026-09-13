#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDriverGetEqContextId(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverGetEqEventType(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
