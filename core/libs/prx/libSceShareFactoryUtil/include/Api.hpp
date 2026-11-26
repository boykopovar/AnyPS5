#pragma once

#include <cstdint>

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceShareFactoryUtilBackToGameFromShareFactory(const void* param);
int APS5_VABI sceShareFactoryUtilGetLaunchStatus(void* status, const void* param);
int APS5_VABI sceShareFactoryUtilGetLaunchStatusOfShareFactory(void* status, const void* param);
int APS5_VABI sceShareFactoryUtilInitialize(void);
int APS5_VABI sceShareFactoryUtilLaunchShareFactory(const void* param, std::uint32_t option);
int APS5_VABI sceShareFactoryUtilTerminate(void);

}
