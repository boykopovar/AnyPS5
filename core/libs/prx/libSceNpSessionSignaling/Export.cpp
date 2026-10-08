#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <mutex>
#include <unordered_set>

// Peer-to-peer signaling needs the network: contexts exist, but sessions never activate.
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED = static_cast<int>(0x80553301);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80553302);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80553303);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_CTX_NOT_FOUND = static_cast<int>(0x80553308);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND = static_cast<int>(0x8055330C);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE = static_cast<int>(0x80552D06);
static std::atomic<uint32_t> g_nextContext{1};
static std::mutex g_mutex;
static bool g_initialized = false;
static size_t g_poolSize = 0;
static std::unordered_set<uint32_t> g_contexts;

struct NpSessionSignalingInitParam {
    int libhttp2CtxId;
    size_t poolSize;
    uint64_t cpuAffinityMask;
    int32_t threadPriority;
    size_t threadStackSize;
};
static_assert(sizeof(NpSessionSignalingInitParam) == 0x28);

struct NpSessionSignalingMemoryInfo {
    size_t totalMemSize;
    size_t curMemUsage;
    size_t maxMemUsage;
    uint8_t reserved[12];
};
static_assert(sizeof(NpSessionSignalingMemoryInfo) == 0x28);

struct NpSessionSignalingConnectionStatistics {
    uint32_t maxConnection;
    uint32_t totalConnection;
    uint32_t connecting;
    uint32_t connected;
};
static_assert(sizeof(NpSessionSignalingConnectionStatistics) == 0x10);

extern "C" {

int APS5_VABI sceNpSessionSignalingInitialize(const NpSessionSignalingInitParam* param) {
    if (!param) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_ALREADY_INITIALIZED;
    g_initialized = true;
    g_poolSize = param->poolSize;
    return 0;
}

int APS5_VABI sceNpSessionSignalingActivateSession(void) {
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingCreateContext2(const void* param, uint32_t* context_id) {
    (void)param;
    if (!context_id) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(g_mutex);
    *context_id = g_nextContext.fetch_add(1, std::memory_order_relaxed);
    g_contexts.insert(*context_id);
    return 0;
}

int APS5_VABI sceNpSessionSignalingCreateContext(const void* param, uint32_t* context_id) {
    return sceNpSessionSignalingCreateContext2(param, context_id);
}

int APS5_VABI sceNpSessionSignalingDeactivate(uint32_t context_id) {
    (void)context_id;
    return 0;
}

int APS5_VABI sceNpSessionSignalingDestroyContext(uint32_t context_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_contexts.erase(context_id);
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionInfo(void) {
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingActivateUser(void) {
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingGetConnectionFromPeerAddress2(void) {
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingTerminate(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    g_poolSize = 0;
    g_contexts.clear();
    return 0;
}

int32_t APS5_VABI sceNpSessionSignalingGetConnectionStatus(uint32_t contextId, uint32_t connectionId, int32_t* status, void* peerAddress, uint16_t* peerPort) {
    (void)connectionId;
    (void)peerAddress;
    (void)peerPort;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!status) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!g_contexts.contains(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int32_t APS5_VABI sceNpSessionSignalingGetLocalNetInfo(int32_t context_id, void* info) {
 (void)context_id;
 if (!info) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
 return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingRequestPrepare(uint32_t contextId, uint32_t* requestId) {
 (void)contextId;
 if (!requestId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
 return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingGetMemoryInfo(NpSessionSignalingMemoryInfo* memInfo) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!memInfo) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    *memInfo = {};
    memInfo->totalMemSize = g_poolSize;
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionStatistics(NpSessionSignalingConnectionStatistics* stats) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!stats) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    *stats = {};
    return 0;
}
}
