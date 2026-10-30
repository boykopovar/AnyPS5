#pragma once

#include "SceTypes.hpp"

extern "C" {

uint32_t APS5_VABI sceRazorCpuIsCapturing(void);
int APS5_VABI sceRazorCpuJobManagerDispatch(const char* label, uint16_t firstId, uint16_t secondId, uint16_t firstCount, uint16_t secondCount, uint64_t firstValue, uint64_t secondValue);
int APS5_VABI sceRazorCpuJobManagerJob(uint16_t firstId, uint16_t secondId, uint16_t thirdId, uint16_t event);
int APS5_VABI sceRazorCpuJobManagerSequence(const char* label, uint16_t firstId, uint16_t secondId, uint16_t event);
int APS5_VABI sceRazorCpuPushMarker(const char* label, uint32_t color, uint32_t flags);
int APS5_VABI sceRazorCpuNamedSync(const char* label);
int APS5_VABI sceRazorCpuPushMarkerStatic(const char* name, uint32_t color, uint32_t flags);
int APS5_VABI sceRazorCpuPopMarker(void);
bool APS5_VABI sceRazorCpuFlushOccurred(uint64_t* timeSpentInFlush);
int APS5_VABI sceRazorCpuPlotValue(const char* series, float value);
int APS5_VABI sceRazorCpuWriteBookmark(const char* label, const char* description);
int APS5_VABI sceRazorCpuSync(void);
void APS5_VABI sceRazorCpuDisableFiberUserMarkers(void);
int APS5_VABI sceRazorCpuInitDataTags(void* buffer, size_t size);
int APS5_VABI sceRazorCpuShutdownDataTags(void);
size_t APS5_VABI sceRazorCpuGetDataTagStorageSize(uint32_t tagCount);
int APS5_VABI sceRazorCpuTagArray(const char* label, uint32_t category, void* array, size_t bufferSize, size_t elementSize);
int APS5_VABI sceRazorCpuTagBuffer(const char* label, uint32_t category, void* buffer, size_t bufferSize);
int APS5_VABI sceRazorCpuResizeTaggedBuffer(uint32_t category, void* buffer, size_t newBufferSize);
int APS5_VABI sceRazorCpuUnTagBuffer(uint32_t category, void* buffer);
int APS5_VABI sceRazorCpuBeginLogicalFileAccess(const char* filename, uint64_t tag, uint64_t size, uint8_t operation);
int APS5_VABI sceRazorCpuEndLogicalFileAccess(void);
int APS5_VABI sceRazorCpuFiberLogNameChange(void);
int APS5_VABI sceRazorCpuFiberSwitch(void);
int APS5_VABI sceRazorCpuGpuMarkerSync(void);
int APS5_VABI sceRazorCpuInitializeGpuMarkerContext(void);
int APS5_VABI sceRazorCpuInitializeInternal(void);
int APS5_VABI sceRazorCpuSetPopMarkerCallback(void);
int APS5_VABI sceRazorCpuSetPushMarkerCallback(void);
int APS5_VABI sceRazorCpuSetPushMarkerStaticCallback(void);
int APS5_VABI sceRazorCpuStartCaptureInternal(void);
int APS5_VABI sceRazorCpuStopCaptureInternal(void);
int APS5_VABI sceRazorCpuWorkloadRunBegin(void);
int APS5_VABI sceRazorCpuWorkloadRunEnd(void);
int APS5_VABI sceRazorCpuWorkloadSubmit(void);
int APS5_VABI sceRazorCpuUserSignalWaitBegin();
int APS5_VABI sceRazorCpuUserSignalPost();
int APS5_VABI sceRazorCpuWriteBookmarkEnd();
int APS5_VABI sceRazorCpuUserSignalWaitEnd();
int APS5_VABI sceRazorCpuWorkloadRunBeginEx();
int APS5_VABI sceRazorCpuUserSignalPostEx();
int APS5_VABI sceRazorCpuUserSignalWaitBeginEx();

}
