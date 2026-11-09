#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePngEncQueryMemorySize(const PngEncCreateParam* param);
int APS5_VABI scePngEncCreate(const PngEncCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle);
int APS5_VABI scePngEncDelete(void* handle);
int APS5_VABI scePngEncEncode(void* handle, const PngEncEncodeParam* param, PngEncOutputInfo* outputInfo);

}
