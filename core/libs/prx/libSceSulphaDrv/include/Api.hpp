#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSulphaGetClientInfo(std::uint32_t client, void* info);
int APS5_VABI sceSulphaGetConnectedClientCount(void);
int APS5_VABI sceSulphaGetDefaultConfig(void* config);
int APS5_VABI sceSulphaGetNeededMemory(const void* config, void* neededSize);
int APS5_VABI sceSulphaInit(const void* config);
int APS5_VABI sceSulphaMessage(std::uint32_t type, const void* message);
int APS5_VABI sceSulphaSetBookmark(const void* bookmark);
int APS5_VABI sceSulphaShutdown(void);

}
