#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLoggerInitialize();
int APS5_VABI sceSystemLoggerSetLogData(const char* tag, uint32_t count, const SystemLoggerLogData* data);
int APS5_VABI sceSystemLoggerTerminate();

}
