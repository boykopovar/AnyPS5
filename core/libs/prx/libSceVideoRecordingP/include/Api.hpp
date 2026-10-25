#pragma once

#include "SceTypes.hpp"

struct VideoRecordingContext;

struct VideoRecordingParam {
    std::uint64_t size;
    std::int32_t threadPriority;
    std::uint32_t duration;
    std::uint32_t mode;
    std::int32_t outputMode;
    std::uint32_t option;
    std::uint32_t format;
};

struct VideoRecordingParam2 {
    std::uint64_t size;
    std::uint64_t affinityMask;
    std::int32_t threadPriority;
    std::uint32_t duration;
    std::uint32_t mode;
    std::int32_t outputMode;
    std::uint32_t option;
    std::uint32_t format;
};

struct VideoRecordingQueryParam {
    std::uint64_t size;
    std::uint32_t reserved8;
    std::int32_t mode;
    std::uint8_t reserved16[12];
    std::uint32_t option;
    std::uint8_t reserved32[32];
};

struct VideoRecordingSupportInfo {
    std::uint8_t data[18];
};

extern "C" {

void APS5_VABI _sceVideoRecordingQueryParam_nid_postfix(VideoRecordingQueryParam* param);
int APS5_VABI sceVideoRecordingClose(int discard);
int APS5_VABI sceVideoRecordingCloseByHandle(VideoRecordingContext* handle, int discard);
int APS5_VABI sceVideoRecordingCopyBGRAtoNV12(void* dst, const void* src, uint32_t width, uint32_t height);
int APS5_VABI sceVideoRecordingGetInfo(std::int32_t info, void*, std::size_t);
int APS5_VABI sceVideoRecordingGetInfoByHandle(VideoRecordingContext* handle, int32_t info, void* data, size_t size);
int APS5_VABI sceVideoRecordingGetStatus(void);
int APS5_VABI sceVideoRecordingGetSupportInfo(VideoRecordingSupportInfo* info);
int APS5_VABI sceVideoRecordingGetVideoOutMode(std::uint32_t* mode);
int APS5_VABI sceVideoRecordingInit(void);
int APS5_VABI sceVideoRecordingInitialize(void* param);
int APS5_VABI sceVideoRecordingOpen(const char* path, const VideoRecordingParam* param, void* heap, int heapSize);
int APS5_VABI sceVideoRecordingOpen2(const char* path, const VideoRecordingParam2* param, void* heap, int heapSize);
std::int64_t APS5_VABI sceVideoRecordingQueryHddSize2(const VideoRecordingParam2* param);
int APS5_VABI sceVideoRecordingQueryMemSize(const VideoRecordingParam* param);
int APS5_VABI sceVideoRecordingQueryMemSize2(const VideoRecordingParam2* param);
int APS5_VABI sceVideoRecordingRestModeProcessGetStatus(void* status);
int APS5_VABI sceVideoRecordingRestModeProcessStart(void* param);
int APS5_VABI sceVideoRecordingSendCommand(int cmd, const void* param);
int APS5_VABI sceVideoRecordingSetInfo(std::int32_t info, const void* data, std::size_t size);
int APS5_VABI sceVideoRecordingStart(void);
int APS5_VABI sceVideoRecordingStop(void);
int APS5_VABI sceVideoRecordingTerm(void);
int APS5_VABI videoRecordingUnknown_iQS6DUtLybE(void);

}
