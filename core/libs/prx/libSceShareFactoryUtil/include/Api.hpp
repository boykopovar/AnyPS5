#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceShareFactoryUtilBackToGameFromShareFactory();
int APS5_VABI sceShareFactoryUtilGetLaunchStatus();
int APS5_VABI sceShareFactoryUtilGetLaunchStatusOfShareFactory();
int APS5_VABI sceShareFactoryUtilInitialize();
int APS5_VABI sceShareFactoryUtilLaunchShareFactory();
int APS5_VABI sceShareFactoryUtilTerminate();

}
