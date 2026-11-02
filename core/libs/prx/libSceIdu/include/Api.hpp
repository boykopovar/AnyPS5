#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceIduGetEnv();
int APS5_VABI sceIduInit();
int APS5_VABI sceIduInstallPkg();
int APS5_VABI sceIduIsAppInstalled();
int APS5_VABI sceIduIsRifInstalled();
int APS5_VABI sceIduIsStaffMode();
int APS5_VABI sceIduReboot();
int APS5_VABI sceIduTerm();

}
