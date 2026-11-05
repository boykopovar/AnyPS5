#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_STATUS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_STATE_INCLUDE_STATUS_HPP

#include "SceTypes.hpp"

extern "C" {

bool APS5_VABI sceAgcDriverIsCaptureInProgress(void);
bool APS5_VABI sceAgcDriverIsTraceInProgress(void);
bool APS5_VABI sceAgcDriverIsSubmitValidationEnabled(void);
int APS5_VABI sceAgcDriverRequestCaptureStart(const char* path);
int APS5_VABI sceAgcDriverRequestCaptureStop();
int APS5_VABI sceAgcDriverTriggerCapture();

}

#endif
