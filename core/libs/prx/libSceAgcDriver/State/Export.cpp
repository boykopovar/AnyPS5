#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2) {
 (void)value0;
 (void)value1;
 (void)value2;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size) {
 (void)base;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

bool APS5_VABI sceAgcDriverIsCaptureInProgress(void) {
 NotImplemented_nid_no_patch(__func__);
 return false;
}

bool APS5_VABI sceAgcDriverIsTraceInProgress(void) {
 NotImplemented_nid_no_patch(__func__);
 return false;
}

bool APS5_VABI sceAgcDriverIsSubmitValidationEnabled(void) {
 NotImplemented_nid_no_patch(__func__);
 return false;
}

}
