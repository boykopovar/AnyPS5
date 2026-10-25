#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePsmUtilFinalize();
int APS5_VABI scePsmUtilGetPsmIniFile();
int APS5_VABI scePsmUtilInitialize();
int APS5_VABI scePsmUtilRegisterCallbacks();

}
