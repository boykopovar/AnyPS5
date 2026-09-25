#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>

// Peer-to-peer signaling needs the network: contexts exist, but sessions never activate.
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552D02);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE = static_cast<int>(0x80552D06);
static std::atomic<uint32_t> g_nextContext{1};

extern "C" {

int APS5_VABI sceNpSessionSignalingInitialize(void* param) {
    (void)param;
    return 0;
}

int APS5_VABI sceNpSessionSignalingActivateSession(void) {
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingCreateContext2(const void* param, uint32_t* context_id) {
    (void)param;
    if (!context_id) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    *context_id = g_nextContext.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceNpSessionSignalingDeactivate(uint32_t context_id) {
    (void)context_id;
    return 0;
}

int APS5_VABI sceNpSessionSignalingDestroyContext(uint32_t context_id) {
    (void)context_id;
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionInfo(void) {
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingTerminate(void) {
    return 0;
}

}
