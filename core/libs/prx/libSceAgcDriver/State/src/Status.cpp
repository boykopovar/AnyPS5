#include "prx/libSceAgcDriver/State/include/Status.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

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
