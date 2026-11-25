#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUgcServiceS3Unknown00(void);
int APS5_VABI sceUgcServiceS3Unknown01(void);
int APS5_VABI sceUgcServiceS3CreateRequest(const char* accessKey);
int APS5_VABI sceUgcServiceS3Unknown02(int requestId);
int APS5_VABI sceUgcServiceS3CancelRequest(int requestId);
int APS5_VABI sceUgcServiceS3Unknown03(int requestId, void* callback);
int APS5_VABI sceUgcServiceS3Unknown04(int requestId, void* userData);
int APS5_VABI sceUgcServiceS3Unknown05(int requestId, float* progress);
int APS5_VABI sceUgcServiceS3Unknown06(int requestId, std::uint64_t* progressSize);
int APS5_VABI sceUgcServiceS3UploadFileWithGmsResponse(int requestId, int fileType, const char* filePath, const char* gmsResponse);
int APS5_VABI sceUgcServiceS3Unknown07(int requestId, int fileType, const char* filePath, const char* gmsResponse, const void* partInfo, void* outInfo);
int APS5_VABI sceUgcServiceS3Unknown08(int requestId, int fileType, const char* filePath, const char* gmsResponse, const void* partInfo, void* outInfo);
int APS5_VABI sceUgcServiceS3DownloadFileWithGmsResponse(int requestId, int fileType, const char* filePath, std::uint64_t* outSize, const char* gmsResponse);
int APS5_VABI sceUgcServiceS3Unknown09(int requestId, char* info, std::size_t infoSize);

}
