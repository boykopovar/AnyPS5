#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cstdint>
#include <mutex>
#include "SceTypes.hpp"

namespace {

constexpr int GRAPHICS5_ERROR_INVALID_GPU_DEBUG = static_cast<int>(0x8a6c0047u);

struct GpuDebugState {
 uint32_t systemMask = 0;
 uint32_t application = 0;
 bool esvmProcess = false;
};

std::mutex gpuDebugMutex;
GpuDebugState gpuDebugState;

}

extern "C" {

int APS5_VABI sceAgdGetSystemGpuDebug() {
 std::lock_guard lock(gpuDebugMutex);
 return static_cast<int>(gpuDebugState.systemMask);
}

int APS5_VABI sceAgdGetApplicationGpuDebug() {
 std::lock_guard lock(gpuDebugMutex);
 return static_cast<int>(gpuDebugState.application);
}

int APS5_VABI sceAgdSetApplicationGpuDebug(std::uint32_t value) {
 std::lock_guard lock(gpuDebugMutex);
 if ((~gpuDebugState.systemMask & value) != 0) return GRAPHICS5_ERROR_INVALID_GPU_DEBUG;
 gpuDebugState.application = value;
 return 0;
}

int APS5_VABI sceAgcDriverIsEsvmProcess() {
 std::lock_guard lock(gpuDebugMutex);
 return gpuDebugState.esvmProcess ? 1 : 0;
}

APS5_EXPORT("qDjJ+CgiMzs", sceAgdGetSystemGpuDebug);
APS5_EXPORT("KKrEe-LWNG8", sceAgdGetApplicationGpuDebug);
APS5_EXPORT("4JOwdjYqMpU", sceAgdSetApplicationGpuDebug);
APS5_EXPORT("SFB48L57OKQ", sceAgcDriverIsEsvmProcess);

}
