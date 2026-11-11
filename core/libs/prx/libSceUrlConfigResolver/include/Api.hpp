#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceUrlConfigResolverGetAppConfig(const char* key, int parameter, void* buffer, std::uint32_t size, std::uint32_t* length);
int APS5_VABI sceUrlConfigResolverGetDefaultQueryParameter();
int APS5_VABI sceUrlConfigResolverGetDeviceId();
int APS5_VABI sceUrlConfigResolverGetJscHeapSizeSoftLimit(std::uint32_t* limit);
int APS5_VABI sceUrlConfigResolverGetReleaseVersion();
int APS5_VABI sceUrlConfigResolverGetSysVer();
bool APS5_VABI sceUrlConfigResolverIsUpdated();
int APS5_VABI sceUrlConfigResolverUpdate(const char* parameter);

}
