#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NET_CTL_AP_ERROR_INVALID_OPT_STR = static_cast<int>(0x80412901);
constexpr int NET_CTL_AP_ERROR_NOT_STARTED = static_cast<int>(0x8041298F);
constexpr int NET_CTL_AP_ERROR_INVALID_TYPE = static_cast<int>(0x80412990);
constexpr int NET_CTL_AP_ERROR_INVALID_ADDR = static_cast<int>(0x80412991);
constexpr int NET_CTL_AP_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412994);
constexpr int NET_CTL_AP_ERROR_CALLBACK_MAX = static_cast<int>(0x80412995);
constexpr int NET_CTL_AP_ERROR_INVALID_ID = static_cast<int>(0x80412996);
constexpr int NET_CTL_AP_ERROR_ID_NOT_FOUND = static_cast<int>(0x80412997);
constexpr int NET_CTL_AP_ERROR_INVALID_SIZE = static_cast<int>(0x80412998);
constexpr int NET_CTL_AP_STATE_STOPPED = 0;
constexpr int NET_CTL_AP_EVENT_TYPE_FIRST = 1;
constexpr int NET_CTL_AP_EVENT_TYPE_LAST = 4;
constexpr std::size_t NET_CTL_AP_INFO_SIZE = 152;
constexpr std::size_t NET_CTL_AP_CONNECT_INFO_SIZE = 184;
constexpr int MAX_CALLBACKS = 8;

using NetCtlApCallback = void(APS5_VABI*)(int, void*);

struct CallbackSlot {
    NetCtlApCallback func = nullptr;
    void* arg = nullptr;
};

std::mutex g_lock;
bool g_initialized = false;
CallbackSlot g_callbacks[MAX_CALLBACKS] = {};

bool ValidEventType(int eventType) {
    return eventType >= NET_CTL_AP_EVENT_TYPE_FIRST && eventType <= NET_CTL_AP_EVENT_TYPE_LAST;
}

}

extern "C" {

int APS5_VABI sceNetCtlApInit() {
    std::lock_guard lock(g_lock);
    if (g_initialized) throw std::runtime_error("sceNetCtlApInit: a second initialization is not implemented");
    g_initialized = true;
    return 0;
}

void APS5_VABI sceNetCtlApTerm() {
    std::lock_guard lock(g_lock);
    g_initialized = false;
    for (auto& slot : g_callbacks) slot = {};
}

int APS5_VABI sceNetCtlApCheckCallback() {
    std::lock_guard lock(g_lock);
    return g_initialized ? 0 : NET_CTL_AP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNetCtlApClearEvent(int eventType) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    return ValidEventType(eventType) ? 0 : NET_CTL_AP_ERROR_INVALID_TYPE;
}

int APS5_VABI sceNetCtlApRegisterCallback(NetCtlApCallback func, void* arg, int* cid) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    if (!func || !cid) return NET_CTL_AP_ERROR_INVALID_ADDR;
    for (int index = 0; index < MAX_CALLBACKS; ++index) {
        if (!g_callbacks[index].func) {
            g_callbacks[index] = {func, arg};
            *cid = index;
            return 0;
        }
    }
    return NET_CTL_AP_ERROR_CALLBACK_MAX;
}

int APS5_VABI sceNetCtlApUnregisterCallback(int cid) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    if (cid < 0 || cid >= MAX_CALLBACKS) return NET_CTL_AP_ERROR_INVALID_ID;
    if (!g_callbacks[cid].func) return NET_CTL_AP_ERROR_ID_NOT_FOUND;
    g_callbacks[cid] = {};
    return 0;
}

int APS5_VABI sceNetCtlApGetResult(int eventType, int* errorCode) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    if (!errorCode) return NET_CTL_AP_ERROR_INVALID_ADDR;
    if (!ValidEventType(eventType)) return NET_CTL_AP_ERROR_INVALID_TYPE;
    *errorCode = 0;
    return 0;
}

int APS5_VABI sceNetCtlApGetInfo(void* info) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    if (!info) return NET_CTL_AP_ERROR_INVALID_ADDR;
    if (*static_cast<const std::uint64_t*>(info) != NET_CTL_AP_INFO_SIZE) return NET_CTL_AP_ERROR_INVALID_SIZE;
    return NET_CTL_AP_ERROR_NOT_STARTED;
}

int APS5_VABI sceNetCtlApGetState(int* state) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    if (!state) return NET_CTL_AP_ERROR_INVALID_ADDR;
    *state = NET_CTL_AP_STATE_STOPPED;
    return 0;
}

int APS5_VABI sceNetCtlApStop() {
    std::lock_guard lock(g_lock);
    return g_initialized ? NET_CTL_AP_ERROR_NOT_STARTED : NET_CTL_AP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNetCtlApGetConnectInfo(const char* optStr, void* connectInfo) {
    std::lock_guard lock(g_lock);
    if (!g_initialized) return NET_CTL_AP_ERROR_NOT_INITIALIZED;
    if (optStr != nullptr) {
        std::size_t length = 0;
        while (length < 9 && optStr[length] != 0) ++length;
        if (length > 8) return NET_CTL_AP_ERROR_INVALID_OPT_STR;
    }
    if (!connectInfo) return NET_CTL_AP_ERROR_INVALID_ADDR;
    if (*static_cast<const std::uint64_t*>(connectInfo) != NET_CTL_AP_CONNECT_INFO_SIZE) return NET_CTL_AP_ERROR_INVALID_SIZE;
    return NET_CTL_AP_ERROR_NOT_STARTED;
}

}
