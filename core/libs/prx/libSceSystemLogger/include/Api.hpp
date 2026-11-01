#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLoggerInitialize();
int APS5_VABI sceSystemLoggerSetLogData();
int APS5_VABI sceSystemLoggerTerminate();

}
