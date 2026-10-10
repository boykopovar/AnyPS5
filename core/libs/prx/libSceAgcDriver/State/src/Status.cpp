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

int APS5_VABI sceAgcDriverSetSubmitValidationMode(uint32_t mode) {
    (void)mode;
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetSubmitValidationMode(uint32_t* mode) {
    (void)mode;
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetSubmitValidationConfig(const void* config) {
    (void)config;
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetSubmitValidationConfig(void* config) {
    (void)config;
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency(uint32_t frequency) {
    (void)frequency;
    return SCE_AGC_DRIVER_ERROR_DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverRequestCaptureStart(const char* path) {
    (void)path;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverRequestCaptureStop() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverTriggerCapture() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
