#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int32_t APS5_VABI sceAc3EncCreateEncoder(int32_t* handle, const void* param, int32_t* status);
int APS5_VABI sceAc3EncDeleteEncoder();
int APS5_VABI sceAc3EncEncode();
int APS5_VABI sceAc3EncEncodeAsync();
int APS5_VABI sceAc3EncFlush();
int APS5_VABI sceAc3EncIsEncoding();
int APS5_VABI sceAc3EncWaitEncode();

}
