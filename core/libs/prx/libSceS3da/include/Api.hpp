#pragma once

#include "SceTypes.hpp"

struct S3daInfo;

extern "C" {

int APS5_VABI sceS3daClose(std::uint32_t port);
int APS5_VABI sceS3daGetInfo(std::uint32_t port, S3daInfo* info);
int APS5_VABI sceS3daOpen(std::uint32_t port);
int APS5_VABI sceS3daSendData(std::uint32_t port, const void* data, std::uint32_t size);

}
