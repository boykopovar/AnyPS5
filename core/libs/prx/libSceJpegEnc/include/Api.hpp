#pragma once

#include "SceTypes.hpp"

extern "C" {

int32_t APS5_VABI sceJpegEncCreate(const JpegEncCreateParam* param, void* memory, uint32_t memorySize, void** handle);
int32_t APS5_VABI sceJpegEncDelete(void* handle);
int32_t APS5_VABI sceJpegEncEncode(void* handle, const JpegEncEncodeParam* param, JpegEncOutputInfo* outputInfo);
int32_t APS5_VABI sceJpegEncQueryMemorySize(const JpegEncCreateParam* param);

}
