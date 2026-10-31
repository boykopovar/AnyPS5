#include "prx/libSceNpSignaling/include/InventoryExports.hpp"
#include "prx/libSceNpSignaling/include/Api.hpp"
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED = static_cast<int>(0x80552701);
static constexpr int SCE_NP_SIGNALING_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80552702);
static constexpr int SCE_NP_SIGNALING_ERROR_CTXID_NOT_AVAILABLE = static_cast<int>(0x80552704);
static constexpr int SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND = static_cast<int>(0x80552705);
static constexpr int SCE_NP_SIGNALING_ERROR_REQ_NOT_FOUND = static_cast<int>(0x80552707);
static constexpr int SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND = static_cast<int>(0x8055270E);
static constexpr int SCE_NP_SIGNALING_ERROR_RESULT_NOT_FOUND = static_cast<int>(0x80552713);
static constexpr int SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552715);
static constexpr int SCE_NP_SIGNALING_ERROR_OWN_NP_ID = static_cast<int>(0x80552716);
static constexpr int CONTEXT_OPTION_FLAG = 1;
static constexpr int MAX_CONTEXT_ID = 8;
static constexpr std::uint32_t CONTEXT_FLAG_BIT = 0x2;
static constexpr std::uint32_t CONN_STATUS_INACTIVE = 0;

namespace {
struct SignalingNpId {
    char handle[16];
    char term;
    char dummy[3];
    std::uint8_t opt[8];
    std::uint8_t reserved[8];
};
static_assert(sizeof(SignalingNpId) == 36);

struct Context {
    SignalingNpId owner{};
    std::uint32_t flags = CONTEXT_FLAG_BIT;
};

std::mutex signalingMutex;
bool initialized = false;
std::unordered_map<int, Context> contexts;
int lastContextId = 0;

bool IsAsciiAlphanumeric(char value) {
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool ReadNpId(const void* npId, SignalingNpId& copy) {
    std::memcpy(&copy, npId, sizeof(copy));
    if (copy.term != 0) return false;
    const std::size_t length = strnlen(copy.handle, sizeof(copy.handle));
    if (length < 3 || !IsAsciiAlphanumeric(copy.handle[0])) return false;
    for (std::size_t index = 1; index < length; ++index) {
        const char value = copy.handle[index];
        if (!IsAsciiAlphanumeric(value) && value != '_' && value != '-') return false;
    }
    return true;
}

int AllocateContextId() {
    int candidate = lastContextId == MAX_CONTEXT_ID ? 1 : lastContextId + 1;
    for (int tried = 0; tried < MAX_CONTEXT_ID; ++tried) {
        if (!contexts.contains(candidate)) {
            lastContextId = candidate;
            return candidate;
        }
        candidate = candidate == MAX_CONTEXT_ID ? 1 : candidate + 1;
    }
    return -1;
}

Context* FindContext(int ctxId) {
    const auto context = contexts.find(ctxId);
    return context == contexts.end() ? nullptr : &context->second;
}
}

extern "C" {

int APS5_VABI sceNpSignalingActivateConnection(int ctxId, const void* peerNpId, int* outConnId) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerNpId || !outConnId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    SignalingNpId peer{};
    if (!ReadNpId(peerNpId, peer)) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    const Context* context = FindContext(ctxId);
    if (!context) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    if (std::memcmp(&context->owner, &peer, sizeof(peer)) == 0) return SCE_NP_SIGNALING_ERROR_OWN_NP_ID;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpSignalingCreateContext(const void* npId, void* callback, void* callbackArg, int* outContextId) {
    (void)callback;
    (void)callbackArg;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!npId || !outContextId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    SignalingNpId owner{};
    if (!ReadNpId(npId, owner)) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    const int ctxId = AllocateContextId();
    if (ctxId < 0) return SCE_NP_SIGNALING_ERROR_CTXID_NOT_AVAILABLE;
    contexts[ctxId] = Context{owner};
    *outContextId = ctxId;
    return 0;
}

int APS5_VABI sceNpSignalingDeleteContext(int ctxId) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    contexts.erase(ctxId);
    return 0;
}

int APS5_VABI sceNpSignalingGetConnectionInfo(int ctxId, int connId, int infoCode, void* outInfo) {
    (void)connId;
    (void)infoCode;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!outInfo) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSignalingInitialize(std::int64_t memorySize, int threadPriority, int cpuAffinityMask, std::int64_t threadStackSize) {
    (void)memorySize;
    (void)threadPriority;
    (void)cpuAffinityMask;
    (void)threadStackSize;
    std::lock_guard lock(signalingMutex);
    if (initialized) return SCE_NP_SIGNALING_ERROR_ALREADY_INITIALIZED;
    initialized = true;
    return 0;
}

int APS5_VABI sceNpSignalingSetContextOption(int ctxId, int optionId, int optionValue) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    Context* context = FindContext(ctxId);
    if (!context) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    if (optionId != CONTEXT_OPTION_FLAG) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (optionValue == 0) {
        context->flags &= ~CONTEXT_FLAG_BIT;
    } else if (optionValue == 1) {
        context->flags |= CONTEXT_FLAG_BIT;
    } else {
        return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    }
    return 0;
}

int APS5_VABI sceNpSignalingTerminate() {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    initialized = false;
    contexts.clear();
    lastContextId = 0;
    return 0;
}

int APS5_VABI sceNpSignalingTerminateConnection(int ctxId, int connId) {
    (void)connId;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSignalingDeactivateConnection(int ctxId, int connId) {
    (void)connId;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND;
}

}

extern "C" {

int APS5_VABI sceNpSignalingCreateContextA(int userId, void* callback, void* callbackArg, int* outContextId) {
    (void)callback;
    (void)callbackArg;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (userId == -1 || !outContextId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpSignalingActivateConnectionA(int ctxId, const NpSignalingAccountPlatformPair* peerAddr, int* outConnId) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerAddr || !outConnId || peerAddr->accountId == 0 || peerAddr->platformType == 0) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpSignalingGetConnectionStatus(int ctxId, int connId, std::uint32_t* outStatus, std::uint32_t* outPeerAddr, std::uint16_t* outPeerPort) {
    (void)connId;
    (void)outPeerAddr;
    (void)outPeerPort;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!outStatus) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    *outStatus = CONN_STATUS_INACTIVE;
    return 0;
}

int APS5_VABI sceNpSignalingGetConnectionInfoA(int ctxId, int connId, int infoCode, void* outInfo) {
    (void)connId;
    (void)infoCode;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!outInfo) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSignalingGetConnectionFromNpId(int ctxId, const void* peerNpId, int* outConnId) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerNpId || !outConnId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    SignalingNpId peer{};
    if (!ReadNpId(peerNpId, peer)) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSignalingGetConnectionFromPeerAddress(int ctxId, std::uint32_t peerAddr, std::uint16_t peerPort, int* outConnId) {
    (void)peerAddr;
    (void)peerPort;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!outConnId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_CONN_NOT_FOUND;
}

int APS5_VABI sceNpSignalingGetConnectionFromPeerAddressA(int ctxId, const NpSignalingAccountPlatformPair* peerAddr, int* outConnId) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerAddr || !outConnId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpSignalingGetContextOption(int ctxId, int optionId, int* outOptionValue) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!outOptionValue) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    const Context* context = FindContext(ctxId);
    if (!context) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    if (optionId != CONTEXT_OPTION_FLAG) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    *outOptionValue = (context->flags & CONTEXT_FLAG_BIT) != 0 ? 1 : 0;
    return 0;
}

int APS5_VABI sceNpSignalingGetPeerNetInfoA(int ctxId, const void* peerAccount, std::uint32_t* outReqId) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerAccount || !outReqId) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpSignalingCancelPeerNetInfo(int ctxId, int reqId) {
    (void)reqId;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    return SCE_NP_SIGNALING_ERROR_REQ_NOT_FOUND;
}

int APS5_VABI sceNpSignalingGetPeerNetInfoResult(int ctxId, int connId, NpSignalingNetInfo* peerNetInfo) {
    (void)connId;
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!peerNetInfo) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    if (!FindContext(ctxId)) return SCE_NP_SIGNALING_ERROR_CTX_NOT_FOUND;
    if (peerNetInfo->size != sizeof(NpSignalingNetInfo)) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    return SCE_NP_SIGNALING_ERROR_RESULT_NOT_FOUND;
}

int APS5_VABI sceNpSignalingGetConnectionStatistics(NpSignalingConnectionStatistics* stats) {
    std::lock_guard lock(signalingMutex);
    if (!initialized) return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    if (!stats) return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    *stats = NpSignalingConnectionStatistics{};
    return 0;
}

int APS5_VABI sceNpSignalingGetLocalNetInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSignalingGetMemoryInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSignalingGetPeerNetInfo() {
    NotImplemented_nid_no_patch(__func__);
}

}
