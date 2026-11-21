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

int APS5_VABI sceNpManagerIntGetOnlineIdByAccountId(std::uint64_t accountId, NpOnlineId* onlineId) {
    if (accountId == 0 || !onlineId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetUserIdByNpId(const NpId* npId, int* userId) {
    if (!npId || !userId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetUserIdByOnlineId(const NpOnlineId* onlineId, int* userId) {
    if (!onlineId || !userId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetUserIdByOnlineIdSdk(const NpOnlineId* onlineId, int* userId) {
    if (!onlineId || !userId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetUserList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetUserNum(int userId, void* userNum) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !userNum) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetVshAccessToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetVshAccessTokenWithCheck(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetVshClientId(void* clientId) {
    if (!clientId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntIsSubAccount(int userId, void* isSubAccount) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !isSubAccount) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntIsTemporarySignout(int userId, void* isTemporarySignout) {
    (void)userId;
    if (!isTemporarySignout) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
bool APS5_VABI sceNpManagerIntIsUnregisteredClientError(int error) {
    return static_cast<std::uint32_t>(error) == 0x82e0104d;
}
int APS5_VABI sceNpManagerIntLoginAuthenticate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntLoginBind(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntLoginCreateAuthenticationTicket(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntLoginGetAccessToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpAbortRequest(int req_id) {
    std::lock_guard lock(requestMutex);
    return liveRequests.count(req_id) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}

int APS5_VABI sceNpCheckCallback(void) {
    std::array<StateCallbackA, NP_STATE_CALLBACK_MAX> callbacksA;
    StateCallback legacy;
    ToolkitStateCallback toolkit;
    {
        std::lock_guard lock(stateMutex);
        callbacksA = stateCallbacksA;
        legacy = stateCallback;
        toolkit = toolkitStateCallback;
        for (auto& entry : stateCallbacksA) entry.pending = false;
        stateCallback.pending = false;
        toolkitStateCallback.pending = false;
    }
    if (legacy.pending) legacy.callback(USER_SERVICE_INITIAL_USER_ID, NP_STATE_SIGNED_OUT, nullptr, legacy.userdata);
    for (const auto& entry : callbacksA) {
        if (entry.pending) entry.callback(USER_SERVICE_INITIAL_USER_ID, NP_STATE_SIGNED_OUT, entry.userdata);
    }
    if (toolkit.pending && toolkit.callback) toolkit.callback(USER_SERVICE_INITIAL_USER_ID, NP_STATE_SIGNED_OUT, toolkit.userdata);
    NpReachabilityStateCallback reachability = nullptr;
    void* reachabilityArgument = nullptr;
    {
        std::lock_guard lock(reachabilityMutex);
        if (reachabilityPending) {
            reachability = reachabilityCallback;
            reachabilityArgument = reachabilityUserdata;
            reachabilityPending = false;
        }
    }
    if (reachability) reachability(USER_SERVICE_INITIAL_USER_ID, NP_REACHABILITY_STATE_UNAVAILABLE, reachabilityArgument);
    return 0;
}

int APS5_VABI sceNpCheckNpAvailability(std::int32_t requestId, const NpOnlineId* onlineId, std::uint8_t* available) {
 if (requestId == 0 || onlineId == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
 const std::uint8_t unavailable = 0;
 if (available != nullptr && !CopyGuestOut(available, &unavailable, sizeof(unavailable))) return SCE_NP_ERROR_INVALID_ARGUMENT;
 return SCE_NP_ERROR_USER_NOT_FOUND;
}

int APS5_VABI sceNpCheckNpReachability(int req_id, int user_id) {
 (void)req_id;
 (void)user_id;
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpCheckPremium(int req_id, const NpCheckPremiumParameter* param, NpCheckPremiumResult* result) {
    (void)req_id;
    (void)param;
    (void)result;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) {
    (void)param;
    return AddRequest();
}

int APS5_VABI sceNpCreateRequest(void) {
    return AddRequest();
}

int APS5_VABI sceNpDeleteRequest(int req_id) {
    std::lock_guard lock(requestMutex);
    if (!liveRequests.count(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    liveRequests.erase(req_id);
    return 0;
}

int APS5_VABI sceNpGetAccountAge(int req_id, int user_id, uint8_t* age) {
 (void)req_id;
 (void)user_id;
 if (!age) return SCE_NP_ERROR_INVALID_ARGUMENT;
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetAccountCountry(const NpOnlineId* online_id, void* country_code) {
    if (!online_id || !country_code) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_USER_NOT_FOUND;
}

int APS5_VABI sceNpGetAccountDateOfBirth(const NpOnlineId* online_id, void* date_of_birth) {
    if (!online_id || !date_of_birth) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_USER_NOT_FOUND;
}

int APS5_VABI sceNpGetAccountLanguage(int req_id, const NpOnlineId* online_id, void* language) {
    (void)req_id;
    if (!online_id || !language) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_USER_NOT_FOUND;
}

int APS5_VABI sceNpGetParentalControlInfo(int req_id, const NpOnlineId* online_id, int8_t* age, void* info) {
    (void)req_id;
    if (!online_id || !age || !info) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_USER_NOT_FOUND;
}

int APS5_VABI sceNpGetUserIdByOnlineId(const NpOnlineId* online_id, int* user_id) {
    if (!online_id || !user_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpCheckPlus(int req_id, const void* param, void* result) {
    if (req_id == 0 || !param || !result) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpNotifyPlusFeature(const void* param) {
    (void)param;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGetAccountId(const NpOnlineId* online_id, uint64_t* account_id) {
    if (!online_id || !account_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    NpOnlineId localId{};
    const auto preserved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localId, online_id, sizeof(localId));
    errno = preserved;
    if (!copied) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpSetGamePresenceOnline(int req_id, const NpOnlineId* online_id, uint32_t presence) {
    (void)presence;
    if (req_id == 0 || !online_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    {
        std::lock_guard lock(requestMutex);
        if (!liveRequests.count(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    }
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpSetGamePresenceOnlineA(int reqId, int userId, uint32_t presence) {
    (void)presence;
    if (userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SignedOutRequest(reqId);
}

int APS5_VABI sceNpGetAccountCountryA(int user_id, void* country_code) {
    if (!country_code) return SCE_NP_ERROR_INVALID_ARGUMENT;
    UserServiceRegisteredUserIdList users{};
    if (user_id != USER_SERVICE_USER_ID_INVALID && sceUserServiceGetRegisteredUserIdList(&users) == USER_SERVICE_OK &&
        std::find(std::begin(users.user_id), std::end(users.user_id), user_id) == std::end(users.user_id))
        return SCE_NP_ERROR_USER_NOT_FOUND;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetAccountIdA(int user_id, uint64_t* account_id) {
    if (user_id == USER_SERVICE_USER_ID_INVALID || !account_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id) {
    if (user_id == USER_SERVICE_USER_ID_INVALID || !np_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetNpReachabilityState(int user_id, uint32_t* state) {
    (void)user_id;
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    constexpr uint32_t val = NP_REACHABILITY_STATE_UNAVAILABLE;
    if (!CopyGuestOut(state, &val, sizeof(val))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpGetOnlineId(int user_id, NpOnlineId* online_id) {
    if (user_id == USER_SERVICE_USER_ID_INVALID || !online_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetState(int user_id, uint32_t* state) {
    (void)user_id;
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    constexpr uint32_t localState = NP_STATE_SIGNED_OUT;
    if (!CopyGuestOut(state, &localState, sizeof(localState))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpHasSignedUp(int user_id, bool* has_signed_up) {
    (void)user_id;
    if (!has_signed_up) return SCE_NP_ERROR_INVALID_ARGUMENT;
    constexpr bool localSignedUp = false;
    if (!CopyGuestOut(has_signed_up, &localSignedUp, sizeof(localSignedUp))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpPollAsync(int req_id, int* result) {
    if (!result) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(requestMutex);
    if (!liveRequests.count(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    constexpr int val = SCE_NP_ERROR_SIGNED_OUT;
    if (!CopyGuestOut(result, &val, sizeof(val))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return NP_POLL_ASYNC_FINISHED;
}

int APS5_VABI sceNpWaitAsync(int req_id, int* result) {
    if (!result) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(requestMutex);
    if (!liveRequests.count(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    constexpr int val = SCE_NP_ERROR_SIGNED_OUT;
    if (!CopyGuestOut(result, &val, sizeof(val))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

void APS5_VABI sceNpRegisterGamePresenceCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
}

int APS5_VABI sceNpRegisterGamePresenceCallbackA(void* callback, void* userdata) {
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(gamePresenceMutex);
    for (auto& entry : gamePresenceCallbacksA) {
        if (entry.inUse) continue;
        entry.handle = ++nextGamePresenceHandle;
        entry.callback = callback;
        entry.userdata = userdata;
        entry.inUse = true;
        return entry.handle;
    }
    return SCE_NP_ERROR_CALLBACK_MAX;
}

int APS5_VABI sceNpUnregisterGamePresenceCallbackA(int callbackId) {
    std::lock_guard lock(gamePresenceMutex);
    for (auto& entry : gamePresenceCallbacksA) {
        if (entry.inUse && entry.handle == callbackId) {
            entry = {};
            return 0;
        }
    }
    return SCE_NP_ERROR_CALLBACK_NOT_REGISTERED;
}

int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void* callback, void* userdata) {
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(reachabilityMutex);
    if (reachabilityCallback) return SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED;
    reachabilityCallback = reinterpret_cast<NpReachabilityStateCallback>(callback);
    reachabilityUserdata = userdata;
    reachabilityPending = true;
    return 0;
}

int APS5_VABI sceNpUnregisterNpReachabilityStateCallback(void) {
    std::lock_guard lock(reachabilityMutex);
    if (!reachabilityCallback) return SCE_NP_ERROR_CALLBACK_NOT_REGISTERED;
    reachabilityCallback = nullptr;
    reachabilityUserdata = nullptr;
    reachabilityPending = false;
    return 0;
}

int APS5_VABI sceNpRegisterPlusEventCallback(void* callback, void* userdata) {
    (void)userdata;
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpRegisterPremiumEventCallback(void* callback, void* userdata) {
    (void)userdata;
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpRegisterStateCallback(NpStateCallback callback, void* userdata) {
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(stateMutex);
    if (stateCallback.callback) return SCE_NP_ERROR_CALLBACK_ALREADY_REGISTERED;
    stateCallback = {callback, userdata, true};
    return 0;
}

int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction) {
    struct Fields {
        std::uint64_t size;
        std::int8_t defaultAgeRestriction;
        std::uint8_t padding[3];
        std::int32_t ageRestrictionCount;
        const void* ageRestrictions;
    };
    if (!restriction) return SCE_NP_ERROR_INVALID_ARGUMENT;
    const auto* fields = reinterpret_cast<const Fields*>(restriction);
    if (fields->size != sizeof(Fields)) return SCE_NP_ERROR_INVALID_SIZE;
    if (fields->defaultAgeRestriction < 0 || fields->ageRestrictionCount < 0 || fields->ageRestrictionCount > 0x100) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (fields->ageRestrictionCount > 0 && !fields->ageRestrictions) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpSetNpTitleId(const NpTitleId* title_id, const NpTitleSecret* title_secret) {
    (void)title_id;
    (void)title_secret;
    return 0;
}

int APS5_VABI sceNpSetTimeout(int reqId, int32_t resolveRetry, uint32_t resolveTimeout, uint32_t connTimeout,
    uint32_t sendTimeout, uint32_t recvTimeout) {
    constexpr uint32_t minResolveTimeout = 1000000;
    constexpr uint32_t minTransferTimeout = 10000000;
    auto belowMinimum = [](uint32_t timeout, uint32_t minimum) { return timeout != 0 && timeout < minimum; };
    if (reqId <= 0 || resolveRetry < 0) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (resolveRetry == 0 && resolveTimeout == 0 && connTimeout == 0 && sendTimeout == 0 && recvTimeout == 0) {
        return SCE_NP_ERROR_INVALID_ARGUMENT;
    }
    if (belowMinimum(resolveTimeout, minResolveTimeout) || belowMinimum(connTimeout, minTransferTimeout) ||
        belowMinimum(sendTimeout, minTransferTimeout) || belowMinimum(recvTimeout, minTransferTimeout)) {
        return SCE_NP_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard lock(requestMutex);
    return liveRequests.count(reqId) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}

int APS5_VABI sceNpUnregisterStateCallback(void) {
    std::lock_guard lock(stateMutex);
    if (!stateCallback.callback) return SCE_NP_ERROR_CALLBACK_NOT_REGISTERED;
    stateCallback = {};
    return 0;
}

int APS5_VABI sceNpRegisterStateCallbackForToolkit(NpStateCallbackForToolkit callback, void* userdata) {
    std::lock_guard lock(stateMutex);
    toolkitStateCallback = {callback, userdata, true};
    return 0;
}

int APS5_VABI sceNpUnregisterStateCallbackForToolkit(void) {
    std::lock_guard lock(stateMutex);
    if (!toolkitStateCallback.callback) return SCE_NP_ERROR_CALLBACK_NOT_REGISTERED;
    toolkitStateCallback = {};
    return 0;
}

int APS5_VABI sceNpGetAccountLanguage2(int req_id, int user_id, void* language) {
    (void)req_id;
    (void)user_id;
    (void)language;
    return SCE_NP_ERROR_SIGNED_OUT;
}

}
