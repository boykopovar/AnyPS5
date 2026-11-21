extern "C" {
#include "prx/libSceNpManager/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceNpManager/include/InGameMessageIds.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <atomic>
#include <mutex>
#include <map>
#include <set>
#include <unordered_set>

// PSN is not emulated: the user is reported as signed out and online queries fail.
static constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int SCE_NP_ERROR_INVALID_PLATFORM_TYPE = static_cast<int>(0x80550004);
static constexpr int SCE_NP_ERROR_OUT_OF_MEMORY = static_cast<int>(0x80550005);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int SCE_NP_ERROR_USER_NOT_FOUND = static_cast<int>(0x80550007);
static constexpr int SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED = static_cast<int>(0x80550008);
static constexpr int SCE_NP_ERROR_CALLBACK_NOT_REGISTERED = static_cast<int>(0x80550009);
static constexpr int SCE_NP_ERROR_INVALID_SIZE = static_cast<int>(0x80550011);
static constexpr int SCE_NP_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80550014);
static constexpr int SCE_NP_ERROR_INVALID_ID = static_cast<int>(0x80550015);
static constexpr int SCE_NP_ERROR_CALLBACK_MAX = static_cast<int>(0x8055001D);
static constexpr int NP_ID_MAPPER_REFUSED_ARGUMENT = static_cast<int>(0x80553083);
static constexpr int NP_ID_MAPPER_REFUSED_REQUEST_ID = static_cast<int>(0x80553085);
static constexpr int NP_MANAGER_INT_REQUEST_DUPLICATE = static_cast<int>(0x80559D03);
static constexpr int NP_MANAGER_INT_REQUEST_TABLE_FULL = static_cast<int>(0x80559D05);
static constexpr int NP_MANAGER_INT_REQUEST_NOT_IN_TABLE = static_cast<int>(0x80559D0A);
static constexpr uint32_t NP_STATE_SIGNED_OUT = 1;
static constexpr int NP_POLL_ASYNC_FINISHED = 0;
static constexpr uint32_t NP_REACHABILITY_STATE_UNAVAILABLE = 0;
static constexpr uint32_t NP_GAME_PRESENCE_STATUS_OFFLINE = 0;

static std::atomic<int> g_nextRequest{1};

int APS5_VABI sceNpNotifyPremiumFeature(const void* param) {
    (void)param;
    return 0;
}

int APS5_VABI sceNpRegisterStateCallbackA(NpStateCallbackA callback, void* userdata) {
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(stateMutex);
    for (const auto& entry : stateCallbacksA) {
        if (entry.callback == callback) return SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED;
    }
    for (int index = 0; index < NP_STATE_CALLBACK_MAX; ++index) {
        if (stateCallbacksA[index].callback) continue;
        stateCallbacksA[index] = {callback, userdata, true};
        return index + 1;
    }
    return SCE_NP_ERROR_CALLBACK_MAX;
}

int APS5_VABI sceNpUnregisterStateCallbackA(int callbackId) {
    if (static_cast<uint32_t>(callbackId) - 1u >= static_cast<uint32_t>(NP_STATE_CALLBACK_MAX)) return SCE_NP_ERROR_INVALID_ID;
    std::lock_guard lock(stateMutex);
    auto& entry = stateCallbacksA[callbackId - 1];
    if (!entry.callback) return SCE_NP_ERROR_CALLBACK_NOT_REGISTERED;
    entry = {};
    return 0;
}

int APS5_VABI sceNpUnregisterPremiumEventCallback(void) {
    return 0;
}

int APS5_VABI sceNpUnregisterPlusEventCallback(void) {
    return 0;
}

int APS5_VABI sceNpGetUserIdByAccountId(uint64_t account_id, int* user_id) {
    if (account_id == 0 || !user_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetSandboxInfo(void* sandbox_info) {
    if (!sandbox_info) return SCE_NP_ERROR_INVALID_ARGUMENT;
    static constexpr char Platform[] = "RETAIL";
    static constexpr char Environment[] = "LIVE";
    std::array<char, 0x50> localInfo{};
    std::memcpy(localInfo.data(), Platform, sizeof(Platform));
    std::memcpy(localInfo.data() + 0x28, Environment, sizeof(Environment));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(sandbox_info, localInfo.data(), localInfo.size());
    errno = savedErrno;
    if (!copied) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

APS5_EXPORT("8weiQyIdI3A", sceNpUnknown_8weiQyIdI3A);
int APS5_VABI sceNpUnknown_8weiQyIdI3A(int userId, const NpOnlineId* buffer) {
    (void)userId;
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("WxLcN76q19o", sceNpUnknown_WxLcN76q19o);
int APS5_VABI sceNpUnknown_WxLcN76q19o(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpCheckCallbackForLib() {
    return sceNpCheckCallback();
}

}

extern "C" {

int APS5_VABI sceNpInGameMessageCreateHandle(std::int32_t libCtxId) {
    std::lock_guard lock(inGameMessageMutex);
    InGameMessageContext* context = FindInGameMessageContext(libCtxId);
    if (!context) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    const std::int32_t handleId = TakeInGameMessageId(nextInGameMessageHandle, __func__);
    context->handles.insert(handleId);
    return handleId;
}

int APS5_VABI sceNpInGameMessageDeleteHandle(std::int32_t libCtxId, std::int32_t handleId) {
    std::lock_guard lock(inGameMessageMutex);
    InGameMessageContext* context = FindInGameMessageContext(libCtxId);
    if (!context) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    context->callbacks.erase(handleId);
    return context->handles.erase(handleId) == 1 ? 0 : NP_IN_GAME_MESSAGE_ERROR_HANDLE_NOT_FOUND;
}

int APS5_VABI sceNpInGameMessageInitialize(std::size_t poolSize, void* option) {
    (void)poolSize;
    (void)option;
    std::lock_guard lock(inGameMessageMutex);
    const std::int32_t libCtxId = TakeInGameMessageId(nextInGameMessageContext, __func__);
    inGameMessageContexts.emplace(libCtxId, InGameMessageContext{});
    return libCtxId;
}

int APS5_VABI sceNpInGameMessagePrepare(std::int32_t libCtxId, std::int32_t handleId, std::int32_t userId, std::uint64_t callbackAddress, void* userdata) {
    (void)userId;
    if (callbackAddress == 0) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(inGameMessageMutex);
    InGameMessageContext* context = FindInGameMessageContext(libCtxId);
    if (!context) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    if (context->handles.count(handleId) != 1) return NP_IN_GAME_MESSAGE_ERROR_HANDLE_NOT_FOUND;
    context->callbacks[handleId] = {callbackAddress, userdata};
    return 0;
}

int APS5_VABI sceNpInGameMessageSendData(std::int32_t libCtxId, const void* destination, const void* source, const void* message) {
    if (!destination || !source || !message) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::uint64_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, static_cast<const std::uint8_t*>(message) + 0x80, sizeof(size))) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    if (size == 0 || size > 0x200) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(inGameMessageMutex);
    if (!FindInGameMessageContext(libCtxId)) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    return NP_IN_GAME_MESSAGE_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNpInGameMessageTerminate(std::int32_t libCtxId) {
    std::lock_guard lock(inGameMessageMutex);
    return inGameMessageContexts.erase(libCtxId) == 1 ? 0 : NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
}

int APS5_VABI sceNpSnsFacebookCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsFacebookDeleteRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSnsFacebookGetAccessToken() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI npManagerLegacyUnknown_70N4VzVCpQg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("70N4VzVCpQg", npManagerLegacyUnknown_70N4VzVCpQg_nid_no_patch);

int APS5_VABI npManagerLegacyUnknown_SSuF3qPYjgA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SSuF3qPYjgA", npManagerLegacyUnknown_SSuF3qPYjgA_nid_no_patch);

int APS5_VABI npManagerLegacyUnknown_ZKqJVpTOtW0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZKqJVpTOtW0", npManagerLegacyUnknown_ZKqJVpTOtW0_nid_no_patch);

int APS5_VABI npManagerLegacyUnknown_jBLJpaqaV1A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jBLJpaqaV1A", npManagerLegacyUnknown_jBLJpaqaV1A_nid_no_patch);

int APS5_VABI npManagerLegacyUnknown_mtowPSR0JN8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mtowPSR0JN8", npManagerLegacyUnknown_mtowPSR0JN8_nid_no_patch);

int APS5_VABI npManagerLegacyUnknown_qp_minus_DvqvPOYM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qp-DvqvPOYM", npManagerLegacyUnknown_qp_minus_DvqvPOYM_nid_no_patch);

int APS5_VABI npManagerLegacyUnknown_tmnDjDmI3AI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tmnDjDmI3AI", npManagerLegacyUnknown_tmnDjDmI3AI_nid_no_patch);

}
