#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSulphaGetClientInfo(void);
int APS5_VABI sceSulphaGetConnectedClientCount(void);
int APS5_VABI sceSulphaGetDefaultConfig(void);
int APS5_VABI sceSulphaGetNeededMemory(void);
int APS5_VABI sceSulphaInit(void);
int APS5_VABI sceSulphaMessage(void);
int APS5_VABI sceSulphaSetBookmark(void);
int APS5_VABI sceSulphaShutdown(void);

}
