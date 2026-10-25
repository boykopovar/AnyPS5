#include "prx/libSceNpMatching2/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80550C02);
static constexpr int SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80550C03);
static constexpr int SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550C0A);
static constexpr int SCE_NP_MATCHING2_ERROR_CONTEXT_MAX = static_cast<int>(0x80550C04);
static constexpr int SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED = static_cast<int>(0x80550C08);
static constexpr int SCE_NP_MATCHING2_ERROR_INVALID_CONTEXT_ID = static_cast<int>(0x80550C0B);
static constexpr int SCE_NET_ERROR_RESOLVER_ETIMEDOUT = static_cast<int>(0x804101E2);
static constexpr int MAX_CONTEXTS = 10;
static constexpr std::uint64_t CREATE_CONTEXT_PARAMETER_SIZE = 0x28;

namespace {

static_assert(offsetof(NpMatching2InitializeParameter, size) == 0x20);
static_assert(sizeof(NpMatching2InitializeParameter) == 0x30);

static_assert(sizeof(NpMatching2CreateContextParameter) == CREATE_CONTEXT_PARAMETER_SIZE);

std::mutex matching2Mutex;
bool matching2Initialized = false;
std::unordered_set<std::uint16_t> contexts;
int nextContextId = 1;

int CheckContext(std::uint16_t ctxId) {
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    return contexts.contains(ctxId) ? 0 : SCE_NP_MATCHING2_ERROR_INVALID_CONTEXT_ID;
}

}

extern "C" {

int APS5_VABI sceNpMatching2ContextStart(std::uint16_t ctxId, std::uint64_t timeout) {
    (void)ctxId;
    (void)timeout;
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    return SCE_NET_ERROR_RESOLVER_ETIMEDOUT;
}

int APS5_VABI sceNpMatching2ContextStop(std::uint16_t ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (const int error = CheckContext(ctxId)) return error;
    return SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED;
}

int APS5_VABI sceNpMatching2CreateContext(const NpMatching2CreateContextParameter* param, std::uint16_t* ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!param || param->size != CREATE_CONTEXT_PARAMETER_SIZE || !ctxId) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    for (int tried = 0; tried < MAX_CONTEXTS; ++tried) {
        const auto candidate = static_cast<std::uint16_t>((nextContextId - 1 + tried) % MAX_CONTEXTS + 1);
        if (contexts.contains(candidate)) continue;
        contexts.insert(candidate);
        nextContextId = candidate % MAX_CONTEXTS + 1;
        *ctxId = candidate;
        return 0;
    }
    return SCE_NP_MATCHING2_ERROR_CONTEXT_MAX;
}

int APS5_VABI sceNpMatching2CreateJoinRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2DestroyContext(std::uint16_t ctxId) {
    std::lock_guard lock(matching2Mutex);
    if (const int error = CheckContext(ctxId)) return error;
    contexts.erase(ctxId);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomDataExternalList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomDataInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetRoomMemberDataInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetServerId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetUserInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2GetWorldInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2Initialize(const NpMatching2InitializeParameter* param) {
    std::lock_guard lock(matching2Mutex);
    if (matching2Initialized) return SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED;
    if (!param || (param->size != offsetof(NpMatching2InitializeParameter, sslPoolSize) && param->size != sizeof(NpMatching2InitializeParameter))) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    matching2Initialized = true;
    return 0;
}

int APS5_VABI sceNpMatching2JoinRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2KickoutRoomMember() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2LeaveRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2RegisterContextCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return matching2Initialized ? 0 : SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpMatching2RegisterRoomEventCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2RegisterRoomMessageCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2RegisterSignalingCallback(std::uint16_t ctxId, void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    std::lock_guard lock(matching2Mutex);
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2SearchRoom() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SendRoomChatMessage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SendRoomMessage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetDefaultRequestOptParam(std::uint16_t ctxId, const void* requestOpt) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (!requestOpt) return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return CheckContext(ctxId);
}

int APS5_VABI sceNpMatching2SetRoomDataExternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetRoomDataInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetRoomMemberDataInternal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SetUserInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2SignalingGetConnectionInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpMatching2Terminate(void) {
    std::lock_guard lock(matching2Mutex);
    if (!matching2Initialized) return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    matching2Initialized = false;
    contexts.clear();
    nextContextId = 1;
    return 0;
}

}
