#include "prx/libSceNpSessionSignaling/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <mutex>

// Peer-to-peer signaling needs the network: contexts exist, but sessions never activate.
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80553303);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND = static_cast<int>(0x8055330C);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE = static_cast<int>(0x80552D06);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED = static_cast<int>(0x80553301);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80553302);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_LIMIT = static_cast<int>(0x80553307);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND = static_cast<int>(0x80553308);
static constexpr int SCE_NP_SESSION_SIGNALING_ERROR_GROUP_NOT_FOUND = static_cast<int>(0x8055330A);

struct SessionSignalingContext {
    std::uint32_t id;
    std::int32_t userId;
    bool active;
};

static std::mutex g_mutex;
static bool g_initialized = false;
static std::array<SessionSignalingContext, 10> g_contexts{};

static_assert(sizeof(SessionSignalingMemoryInfo) == 24);
static_assert(sizeof(SessionSignalingInitParam) == 40);
static_assert(offsetof(SessionSignalingInitParam, poolSize) == 8);
static_assert(offsetof(SessionSignalingInitParam, affinityMask) == 16);
static_assert(offsetof(SessionSignalingInitParam, threadPriority) == 24);
static_assert(offsetof(SessionSignalingInitParam, stackSize) == 32);
static_assert(offsetof(SessionSignalingMemoryInfo, usedSize) == 8);
static_assert(offsetof(SessionSignalingMemoryInfo, peakUsedSize) == 16);
static_assert(sizeof(SessionSignalingPeerAddress) == 16);
static_assert(sizeof(SessionSignalingSessionParam) == 8);
static_assert(offsetof(SessionSignalingSessionParam, option) == 4);
static_assert(sizeof(SessionSignalingNetAddress) == 8);
static_assert(offsetof(SessionSignalingNetAddress, port) == 4);
static_assert(sizeof(SessionSignalingConnectionInfo) == 16);
static_assert(sizeof(SessionSignalingLocalNetInfo) == 16);
static_assert(offsetof(SessionSignalingLocalNetInfo, mappedAddress) == 4);
static_assert(offsetof(SessionSignalingLocalNetInfo, mappedPort) == 8);
static_assert(offsetof(SessionSignalingLocalNetInfo, mappingStatus) == 12);
static_assert(sizeof(SessionSignalingContextParam1) == 56);
static_assert(sizeof(SessionSignalingContextParam2) == 56);
static_assert(offsetof(SessionSignalingContextParam1, prepareCallback) == 8);
static_assert(offsetof(SessionSignalingContextParam1, sessionCallback) == 24);
static_assert(offsetof(SessionSignalingContextParam1, connectionCallback) == 40);
static_assert(offsetof(SessionSignalingContextParam2, connectionCallback) == 40);
static_assert(offsetof(SessionSignalingContextParam2, connectionUserData) == 48);
static_assert(offsetof(SessionSignalingPeerAddress, platform) == 8);
static_assert(sizeof(SessionSignalingConnectionStatistics) == 16);
static_assert(offsetof(SessionSignalingConnectionStatistics, connectionCount) == 4);
static_assert(offsetof(SessionSignalingConnectionStatistics, connectingCount) == 8);
static_assert(offsetof(SessionSignalingConnectionStatistics, connectedCount) == 12);
static_assert(sizeof(SessionSignalingGroupInfo) == 264);
static_assert(offsetof(SessionSignalingGroupInfo, connectionCount) == 256);

static SessionSignalingContext* FindContext(std::uint32_t contextId) {
    auto& context = g_contexts[contextId % g_contexts.size()];
    return context.active && context.id == contextId ? &context : nullptr;
}

template<typename TConnectionCallback>
static int CreateContext(const SessionSignalingContextParam<TConnectionCallback>* param, std::uint32_t* contextId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!param || !contextId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    for (const auto& context : g_contexts) {
        if (context.active && context.userId == param->userId) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_LIMIT;
    }
    for (auto& context : g_contexts) {
        if (!context.active) {
            context.userId = param->userId;
            context.active = true;
            *contextId = context.id;
            return 0;
        }
    }
    return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_LIMIT;
}

extern "C" {

int APS5_VABI sceNpSessionSignalingInitialize(const SessionSignalingInitParam* param) {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_ALREADY_INITIALIZED;
    if (!param || !param->poolSize) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t index = 0; index < g_contexts.size(); ++index) {
        g_contexts[index] = {1000 + index, 0, false};
    }
    g_initialized = true;
    return 0;
}

int APS5_VABI sceNpSessionSignalingActivateSession(std::uint32_t contextId, const char* sessionId, std::uint32_t sessionType, const SessionSignalingSessionParam* param, std::uint32_t* groupId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!sessionId || !param || !groupId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    if ((param->mode == 1 && param->option != 0) || (param->mode == 2 && param->option == 0)) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingCreateContext2(const SessionSignalingContextParam2* param, std::uint32_t* contextId) {
    return CreateContext(param, contextId);
}

int APS5_VABI sceNpSessionSignalingCreateContext(const SessionSignalingContextParam1* param, std::uint32_t* contextId) {
    return CreateContext(param, contextId);
}

int APS5_VABI sceNpSessionSignalingDeactivate(std::uint32_t contextId, std::uint32_t connectionId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_GROUP_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingDestroyContext(std::uint32_t contextId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    auto* context = FindContext(contextId);
    if (!context) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    context->active = false;
    const auto nextId = context->id + static_cast<std::uint32_t>(g_contexts.size());
    context->id = nextId < 100001 ? nextId : 1000 + contextId % static_cast<std::uint32_t>(g_contexts.size());
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionInfo(std::uint32_t contextId, std::uint32_t connectionId, std::uint32_t code, SessionSignalingConnectionInfo* info) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!info) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingActivateUser(std::uint32_t contextId, const SessionSignalingPeerAddress* peerAddress, std::uint32_t* connectionId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerAddress || !connectionId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingGetConnectionFromPeerAddress2(std::uint32_t contextId, std::uint32_t groupId, const SessionSignalingPeerAddress* peerAddress, std::uint32_t* connectionId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerAddress || !connectionId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_GROUP_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingTerminate(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    for (auto& context : g_contexts) context.active = false;
    return 0;
}

int32_t APS5_VABI sceNpSessionSignalingGetConnectionStatus(std::uint32_t contextId, std::uint32_t connectionId, std::int32_t* status, std::uint32_t* peerAddress, std::uint16_t* peerPort) {
 (void)connectionId;
 (void)peerAddress;
 (void)peerPort;
 std::lock_guard lock(g_mutex);
 if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
 if (!status) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
 if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
 return SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int32_t APS5_VABI sceNpSessionSignalingGetLocalNetInfo(std::uint32_t contextId, SessionSignalingLocalNetInfo* info) {
 (void)contextId;
 std::lock_guard lock(g_mutex);
 if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
 if (!info) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
 return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingRequestPrepare(uint32_t contextId, uint32_t* requestId) {
 std::lock_guard lock(g_mutex);
 if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
 if (!requestId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
 if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
 return SCE_NP_SESSION_SIGNALING_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpSessionSignalingGetMemoryInfo(SessionSignalingMemoryInfo* memoryInfo) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionStatistics(SessionSignalingConnectionStatistics* statistics) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionFromPeerAddress(std::uint32_t contextId, const SessionSignalingPeerAddress* peerAddress, std::uint32_t* connectionId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerAddress || !connectionId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingGetGroupFromSessionId(std::uint32_t contextId, const char* sessionId, std::uint32_t* groupId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!sessionId || !groupId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_GROUP_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingGetGroupInfo(std::uint32_t contextId, std::uint32_t groupId, SessionSignalingGroupInfo* groupInfo) {
    (void)groupId;
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!groupInfo) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_GROUP_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingGetConnectionFromNetAddress2(std::uint32_t contextId, std::uint32_t groupId, std::uint32_t address, std::uint16_t port, std::uint32_t* connectionId) {
    (void)groupId;
    (void)address;
    (void)port;
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!connectionId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_GROUP_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingCreateJoinActivate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetConnectionFromNetAddress(std::uint32_t contextId, std::uint32_t address, std::uint16_t port, std::uint32_t* connectionId) {
    (void)address;
    (void)port;
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_NP_SESSION_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!connectionId) return SCE_NP_SESSION_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(contextId)) return SCE_NP_SESSION_SIGNALING_ERROR_CONTEXT_NOT_FOUND;
    return SCE_NP_SESSION_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSessionSignalingGetCustomProperty() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingGetGroupFromPeerAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingInitializeWithPort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingJoinActivate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingSetCustomProperty() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingManualUdpAbortConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSessionSignalingManualUdpCreateJoin() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSessionSignalingManualUdpEstablishConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSessionSignalingManualUdpJoin() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSessionSignalingRegisterInternalCallback() {
    NotImplemented_nid_no_patch(__func__);
}

}
