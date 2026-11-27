#include "prx/libSceRazorCpu_debug/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED = static_cast<int>(0x80580000);

}

extern "C" {

int APS5_VABI sceRazorCpuStartCapture(void) {
    return SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRazorCpuStopCapture(void) {
    return SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRazorCpuSetLiveServerThreadSettings(void) {
    return SCE_RAZOR_CPU_ERROR_NOT_INITIALIZED;
}

}
