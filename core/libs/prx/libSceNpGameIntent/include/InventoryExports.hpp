#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpGameIntentGetPermittedIntentTypes();
int APS5_VABI sceNpGameIntentLaunchAppForPinvoke();
int APS5_VABI sceNpGameIntentLaunchAppRequestToGkp();
int APS5_VABI sceNpGameIntentRemoveLaunchCallback();
int APS5_VABI sceNpGameIntentRequest();

}
