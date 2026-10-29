#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCameraGetUsageInfo();
int APS5_VABI sceCameraGetUsageInfoWithoutCountProcess();
int APS5_VABI sceCameraOpenByModuleIdWithMode();
int APS5_VABI sceCameraSetForceDriverMode();

}
