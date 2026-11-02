#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUrlConfigResolverGetAppConfig();
int APS5_VABI sceUrlConfigResolverGetDefaultQueryParameter();
int APS5_VABI sceUrlConfigResolverGetDeviceId();
int APS5_VABI sceUrlConfigResolverGetJscHeapSizeSoftLimit();
int APS5_VABI sceUrlConfigResolverGetReleaseVersion();
int APS5_VABI sceUrlConfigResolverGetSysVer();
int APS5_VABI sceUrlConfigResolverIsUpdated();
int APS5_VABI sceUrlConfigResolverUpdate();

}
