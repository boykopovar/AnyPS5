#pragma once

#include "SceTypes.hpp"

extern "C" {

int32_t APS5_VABI sceJpegDecCreate(const JpegDecCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle);
int32_t APS5_VABI sceJpegDecDecode(void* handle, const JpegDecDecodeParam* param, JpegDecImageInfo* imageInfo);
int32_t APS5_VABI sceJpegDecDelete(void* handle);
int32_t APS5_VABI sceJpegDecParseHeader(const JpegDecParseParam* param, JpegDecImageInfo* imageInfo);
int32_t APS5_VABI sceJpegDecQueryMemorySize(const JpegDecCreateParam* param);
int32_t APS5_VABI sceJpegDecDecodeWithInputControl(void* handle, const JpegDecInputControlParam* param, JpegDecImageInfo* imageInfo);

}
