#include "prx/libSceAgcDriver/State/include/Status.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

bool APS5_VABI sceAgcDriverIsCaptureInProgress(void) {
    return false;
}

bool APS5_VABI sceAgcDriverIsTraceInProgress(void) {
    return false;
}

bool APS5_VABI sceAgcDriverIsSubmitValidationEnabled(void) {
    return false;
}

int APS5_VABI sceAgcDriverRequestCaptureStart(const char* path) {
    (void)path;
    return static_cast<int>(0x8A6C1000);
}

int APS5_VABI sceAgcDriverRequestCaptureStop() {
    return static_cast<int>(0x8A6C1000);
}

int APS5_VABI sceAgcDriverTriggerCapture() {
    return static_cast<int>(0x8A6C1000);
}

}
