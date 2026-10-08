#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceRazorCpuIsCapturing(void) {
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerDispatch(const void* args) {
 (void)args;
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerJob(const void* args) {
 (void)args;
 return 0;
}

int APS5_VABI sceRazorCpuJobManagerSequence(const void* args) {
 (void)args;
 return 0;
}

int APS5_VABI sceRazorCpuNamedSync(void) {
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

int APS5_VABI sceRazorCpuFlushOccurred(uint64_t* timeSpentInFlush) {
 if (timeSpentInFlush != nullptr) *timeSpentInFlush = 0;
 return 0;
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

}
