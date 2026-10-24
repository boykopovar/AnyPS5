#include "prx/libSceRazorCpu/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceRazorCpuIsCapturing(void) {
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerDispatch(const char* label, uint16_t firstId, uint16_t secondId, uint16_t firstCount, uint16_t secondCount, uint64_t firstValue, uint64_t secondValue) {
 (void)label;
 (void)firstId;
 (void)secondId;
 (void)firstCount;
 (void)secondCount;
 (void)firstValue;
 (void)secondValue;
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerJob(uint16_t firstId, uint16_t secondId, uint16_t thirdId, uint16_t event) {
 (void)firstId;
 (void)secondId;
 (void)thirdId;
 (void)event;
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerSequence(const char* label, uint16_t firstId, uint16_t secondId, uint16_t event) {
 (void)label;
 (void)firstId;
 (void)secondId;
 (void)event;
 return 0;
}

int APS5_VABI sceRazorCpuPushMarker(const char* label, uint32_t color, uint32_t flags) {
 (void)label;
 (void)color;
 (void)flags;
 return 0;
}

int APS5_VABI sceRazorCpuNamedSync(const char* label) {
 (void)label;
 return 0;
}

int APS5_VABI sceRazorCpuPushMarkerStatic(const char* name, uint32_t color, uint32_t flags) {
 (void)name;
 (void)color;
 (void)flags;
 return 0;
}

int APS5_VABI sceRazorCpuPopMarker(void) {
 return 0;
}

bool APS5_VABI sceRazorCpuFlushOccurred(uint64_t* timeSpentInFlush) {
 (void)timeSpentInFlush;
 return false;
}

int APS5_VABI sceRazorCpuPlotValue(const char* series, float value) {
 (void)series;
 (void)value;
 return 0;
}

int APS5_VABI sceRazorCpuWriteBookmark(const char* label, const char* description) {
 (void)label;
 (void)description;
 return 0;
}

int APS5_VABI sceRazorCpuSync(void) {
 return 0;
}

void APS5_VABI sceRazorCpuDisableFiberUserMarkers(void) {
}

int APS5_VABI sceRazorCpuInitDataTags(void* buffer, size_t size) {
 (void)buffer;
 (void)size;
 return 0;
}

int APS5_VABI sceRazorCpuShutdownDataTags(void) {
 return 0;
}

size_t APS5_VABI sceRazorCpuGetDataTagStorageSize(uint32_t tagCount) {
 (void)tagCount;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceRazorCpuTagArray(const char* label, uint32_t category, void* array, size_t bufferSize, size_t elementSize) {
 (void)label;
 (void)category;
 (void)array;
 (void)bufferSize;
 (void)elementSize;
 return 0;
}

int APS5_VABI sceRazorCpuTagBuffer(const char* label, uint32_t category, void* buffer, size_t bufferSize) {
 (void)label;
 (void)category;
 (void)buffer;
 (void)bufferSize;
 return 0;
}

int APS5_VABI sceRazorCpuResizeTaggedBuffer(uint32_t category, void* buffer, size_t newBufferSize) {
 (void)category;
 (void)buffer;
 (void)newBufferSize;
 return 0;
}

int APS5_VABI sceRazorCpuUnTagBuffer(uint32_t category, void* buffer) {
 (void)category;
 (void)buffer;
 return 0;
}

int APS5_VABI sceRazorCpuBeginLogicalFileAccess(const char* filename, uint64_t tag, uint64_t size, uint8_t operation) {
 (void)filename;
 (void)tag;
 (void)size;
 (void)operation;
 return 0;
}

int APS5_VABI sceRazorCpuEndLogicalFileAccess(void) {
 return 0;
}

int APS5_VABI sceRazorCpuFiberLogNameChange(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuFiberSwitch(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuGpuMarkerSync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuInitializeGpuMarkerContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuInitializeInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuSetPopMarkerCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuSetPushMarkerCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuSetPushMarkerStaticCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuStartCaptureInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuStopCaptureInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuWorkloadRunBegin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuWorkloadRunEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuWorkloadSubmit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
