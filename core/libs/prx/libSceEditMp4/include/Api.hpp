#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceEditMp4Abort();
int APS5_VABI sceEditMp4Edit();
int APS5_VABI sceEditMp4GetStatus();
int APS5_VABI sceEditMp4Initialize();
void APS5_VABI sceEditMp4InitializeParam(void* param);
std::uint64_t APS5_VABI sceEditMp4QueryMemSize();
int APS5_VABI sceEditMp4SetFinishCallback();
int APS5_VABI sceEditMp4Terminate();
int APS5_VABI sceEditMp4Unite();

}
