#pragma once

#include "SceTypes.hpp"

struct ShareScreenshotParamExtended {
    std::uint64_t size;
    std::uint32_t source;
    std::uint32_t mode;
    const char* text;
};

struct ShareVideoClipParamExtended {
    std::uint64_t size;
    std::uint64_t value8;
    std::uint64_t value16;
    std::uint32_t mode;
    std::uint32_t reserved;
    const char* text;
};

extern "C" {

int APS5_VABI sceShareCaptureScreenshot(const void* param, int32_t* reqId);
int APS5_VABI sceShareCaptureVideoClip(const void* param, int32_t* reqId);
int APS5_VABI sceShareFeaturePermit(uint32_t featureFlags);
int APS5_VABI sceShareFeatureProhibit(uint32_t featureFlags);
int APS5_VABI sceShareGetCurrentStatus(uint32_t featureFlag, ShareCurrentStatus* status);
int APS5_VABI sceShareInitialize(size_t heapSize, int threadPriority, uint64_t affinityMask);
int APS5_VABI sceShareOpenMenuForContent(const void* contentId);
int APS5_VABI sceShareRegisterContentEventCallback(void* callback, void* userData);
int APS5_VABI sceShareSetCaptureSource(uint32_t featureFlags, const void* tapPoint);
int APS5_VABI sceShareSetContentParam(const char* contentParam);
int APS5_VABI sceShareSetScreenshotOverlayImage(const char* filePath, int32_t marginX, int32_t marginY, int32_t origin);
int APS5_VABI sceShareTerminate(void);
int APS5_VABI sceShareUnregisterContentEventCallback(void* callback);
int APS5_VABI sceShareCaptureScreenshotExtended(const ShareScreenshotParamExtended* param, std::int32_t* requestId);
int APS5_VABI sceShareCaptureVideoClipExtended(const ShareVideoClipParamExtended* param, std::int32_t* requestId);
int APS5_VABI sceShareGetRunningStatus(uint32_t* status);
int APS5_VABI sceShareSetContentParamForApplicationTitle(const char* applicationTitle);

}
