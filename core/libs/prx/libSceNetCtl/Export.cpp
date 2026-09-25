#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <mutex>

// Network control reports an initialized but disconnected interface: no network is emulated.
static constexpr int SCE_NET_CTL_ERROR_CALLBACK_MAX = static_cast<int>(0x80412103);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ID = static_cast<int>(0x80412105);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ADDR = static_cast<int>(0x80412107);
static constexpr int SCE_NET_CTL_ERROR_NOT_CONNECTED = static_cast<int>(0x80412108);
static constexpr int NET_CTL_STATE_DISCONNECTED = 0;
static constexpr int MAX_CALLBACKS = 8;

static std::mutex g_callbackLock;
static NetCtlCallback g_callbacks[MAX_CALLBACKS] = {};

extern "C" {

int APS5_VABI sceNetCtlCheckCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
    (void)code;
    (void)info;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
    (void)nat_info;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlGetResult(int event_type, int* error_code) {
    (void)event_type;
    if (!error_code) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *error_code = 0;
    return 0;
}

int APS5_VABI sceNetCtlGetState(int* state) {
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) {
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlInit(void) {
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
    if (!func || !cid) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    std::lock_guard lock(g_callbackLock);
    for (int index = 0; index < MAX_CALLBACKS; ++index) {
        if (!g_callbacks[index]) {
            g_callbacks[index] = func;
            *cid = index;
            return 0;
        }
    }
    return SCE_NET_CTL_ERROR_CALLBACK_MAX;
}

void APS5_VABI sceNetCtlTerm(void) {
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
    if (cid < 0 || cid >= MAX_CALLBACKS) return SCE_NET_CTL_ERROR_INVALID_ID;
    std::lock_guard lock(g_callbackLock);
    g_callbacks[cid] = nullptr;
    return 0;
}

}
