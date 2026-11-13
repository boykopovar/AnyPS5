#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceEditMp4Abort(const void* handle);
int APS5_VABI sceEditMp4Edit(const void* handle, const void* argument2, const void* argument3, const void* argument4);
int APS5_VABI sceEditMp4GetStatus(const void* handle, void* status);
int APS5_VABI sceEditMp4Initialize(const void* param, void* handle);
void APS5_VABI sceEditMp4InitializeParam(void* param);
std::uint64_t APS5_VABI sceEditMp4QueryMemSize();
int APS5_VABI sceEditMp4SetFinishCallback(const void* handle);
int APS5_VABI sceEditMp4Terminate(const void* handle);
int APS5_VABI sceEditMp4Unite(const void* handle, const void* argument2, const void* argument3, const void* argument4);

}
