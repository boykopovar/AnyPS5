#pragma once

#include "SceTypes.hpp"

extern "C" {

int32_t APS5_VABI scePngDecCreate(const PngDecCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle);
int32_t APS5_VABI scePngDecDecode(void* handle, const PngDecDecodeParam* param, PngDecImageInfo* imageInfo);
int32_t APS5_VABI scePngDecDelete(void* handle);
int32_t APS5_VABI scePngDecParseHeader(const PngDecParseParam* param, PngDecImageInfo* imageInfo);
int32_t APS5_VABI scePngDecQueryMemorySize(const PngDecCreateParam* param);
int APS5_VABI scePngDecDecodeWithInputControl();

}
