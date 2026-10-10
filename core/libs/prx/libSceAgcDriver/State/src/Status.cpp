#include "prx/libSceAgcDriver/State/include/Status.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE = static_cast<int>(0x8A6C1000);

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
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverRequestCaptureStop() {
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverTriggerCapture() {
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetShaderDebuggingStatus() {
    return 1;
}

}
