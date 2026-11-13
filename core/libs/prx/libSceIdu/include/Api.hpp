#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceIduGetEnv();
int APS5_VABI sceIduInit(const void* param);
int APS5_VABI sceIduInstallPkg(const void* first, const void* second);
int APS5_VABI sceIduIsAppInstalled();
int APS5_VABI sceIduIsRifInstalled(const char* name);
int APS5_VABI sceIduIsStaffMode();
int APS5_VABI sceIduReboot();
int APS5_VABI sceIduTerm();

}
