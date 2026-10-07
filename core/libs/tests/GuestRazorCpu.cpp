#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
std::uint32_t APS5_VABI sceRazorCpuIsCapturing(void);
int APS5_VABI sceRazorCpuJobManagerDispatch(const void* args);
int APS5_VABI sceRazorCpuJobManagerJob(const void* args);
int APS5_VABI sceRazorCpuJobManagerSequence(const void* args);
int APS5_VABI sceRazorCpuPushMarkerStatic(const char* name, std::uint32_t color, std::uint32_t flags);
int APS5_VABI sceRazorCpuPopMarker(void);
int APS5_VABI sceRazorCpuFlushOccurred(std::uint64_t* timeSpentInFlush);
}

namespace {

void Require(bool value) {
    if (!value)
        std::abort();
}

}

int main() {
    Require(sceRazorCpuIsCapturing() == 0);
    Require(sceRazorCpuJobManagerDispatch(nullptr) == 0);
    Require(sceRazorCpuJobManagerJob(nullptr) == 0);
    Require(sceRazorCpuJobManagerSequence(nullptr) == 0);
    Require(sceRazorCpuPushMarkerStatic("outer", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuPushMarkerStatic("inner", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPopMarker() == 0);
    Require(sceRazorCpuPushMarkerStatic("unbalanced", 0x80ffffffu, 2) == 0);
    Require(sceRazorCpuIsCapturing() == 0);
    std::uint64_t timeSpentInFlush = 0x123456789abcdef0ull;
    Require(sceRazorCpuFlushOccurred(&timeSpentInFlush) == 0);
    Require(timeSpentInFlush == 0);
    Require(sceRazorCpuFlushOccurred(nullptr) == 0);
}
