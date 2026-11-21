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

int APS5_VABI sceNpIdMapperOnlineIdToAccountId(int reqId, int userId, const void* input, void* output, std::uint32_t value, std::uint8_t flag, std::uint64_t extra) {
    (void)flag;
    (void)extra;
    if (reqId <= 0) return NP_ID_MAPPER_REFUSED_REQUEST_ID;
    if (userId == USER_SERVICE_USER_ID_INVALID || !input || !output || value < 1 || value > 1000) return NP_ID_MAPPER_REFUSED_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpInGameMessageAbortHandle(std::int32_t libCtxId, std::int32_t handleId) {
    std::lock_guard lock(inGameMessageMutex);
    InGameMessageContext* context = FindInGameMessageContext(libCtxId);
    if (!context) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    return context->handles.count(handleId) == 1 ? 0 : NP_IN_GAME_MESSAGE_ERROR_HANDLE_NOT_FOUND;
}
int APS5_VABI sceNpInGameMessageCheckCallback(void) {
    return 0;
}
int APS5_VABI sceNpInGameMessageGetMemoryPoolStatistics(std::int32_t libCtxId, void* statistics) {
    if (statistics == nullptr) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(inGameMessageMutex);
    if (!FindInGameMessageContext(libCtxId)) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    std::uint8_t empty[0x20] {};
    if (!GuestCopyOut_nid_no_patch(statistics, empty, sizeof(empty))) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    return 0;
}
int APS5_VABI sceNpInGameMessagePrepareA(std::int32_t libCtxId, std::int32_t handleId, std::int32_t userId, std::uint64_t callbackAddress, void* userdata) {
    (void)userId;
    if (callbackAddress == 0) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(inGameMessageMutex);
    InGameMessageContext* context = FindInGameMessageContext(libCtxId);
    if (!context) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    if (context->handles.count(handleId) != 1) return NP_IN_GAME_MESSAGE_ERROR_HANDLE_NOT_FOUND;
    context->callbacks[handleId] = {callbackAddress, userdata};
    return 0;
}
int APS5_VABI sceNpInGameMessageSendDataA(std::int32_t libCtxId, const void* destination, const void* source, const void* message) {
    if (!destination || !source || !message) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::uint64_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, static_cast<const std::uint8_t*>(message) + 0x200, sizeof(size))) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    if (size == 0 || size > 0x200) return NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(inGameMessageMutex);
    if (!FindInGameMessageContext(libCtxId)) return NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND;
    return NP_IN_GAME_MESSAGE_ERROR_NOT_CONNECTED;
}
int APS5_VABI sceNpIntGetAppType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpIntGetGamePresenceStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpIntGetNpTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpIntGetNpTitleIdSecret(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpIntRegisterCheckCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpIntRegisterGamePresenceCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpIsPlusMember(int, bool* isMember) {
    if (!isMember) return SCE_NP_ERROR_INVALID_ARGUMENT;
    constexpr bool val = false;
    if (!CopyGuestOut(isMember, &val, sizeof(val))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}
int APS5_VABI sceNpManagerIntAbortRequest(int reqId) {
    std::lock_guard lock(managerIntMutex);
    return managerIntRequests.count(reqId) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}
int APS5_VABI sceNpManagerIntAddActiveSigninStateCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntAddOnlineIdChangeCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntAddPlusMemberTypeCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntAddSigninStateCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntAuthGetAuthorizationCode(int reqId, int userId, const void* parameter, const void* optionalParameter, std::uint32_t firstValue, std::uint32_t secondValue, std::uint8_t flag, void* output) {
    (void)reqId;
    (void)optionalParameter;
    (void)flag;
    if (userId == USER_SERVICE_USER_ID_INVALID || !parameter || !output || firstValue > 3 || secondValue < 1 || secondValue > 3) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntAuthGetIdToken(int reqId, int userId, const void* firstParameter, const void* secondParameter, const void* thirdParameter, std::uint64_t value, void* output) {
    (void)reqId;
    (void)value;
    if (userId == USER_SERVICE_USER_ID_INVALID || !firstParameter || !secondParameter || !thirdParameter || !output) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntCheckNpAvailability(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCheckNpAvailabilityByPid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCheckNpState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCheckNpStateA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntClearGameAccessToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntClearOnlineIdChangeFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntClearTicket(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntClearUsedFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntClearVshAccessToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCreateAuthenticationTicket(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCreateLoginContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCreateLoginRequest(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntCreateRequest(void) {
    std::lock_guard lock(managerIntMutex);
    if (managerIntRequests.size() >= NP_MANAGER_INT_REQUEST_SLOTS) return NP_MANAGER_INT_REQUEST_TABLE_FULL;
    const int reqId = static_cast<int>(nextManagerIntRequest++);
    if (reqId <= 0) return reqId;
    if (!managerIntRequests.insert(reqId).second) return NP_MANAGER_INT_REQUEST_DUPLICATE;
    return reqId;
}
int APS5_VABI sceNpManagerIntDeleteLoginContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntDeleteRequest(int reqId) {
    std::lock_guard lock(managerIntMutex);
    return managerIntRequests.erase(reqId) ? 0 : NP_MANAGER_INT_REQUEST_NOT_IN_TABLE;
}
int APS5_VABI sceNpManagerIntGetAccessTokenViaImplicitFlow(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetAccountAge(int reqId, int userId, std::uint8_t* age) {
    (void)reqId;
    if (userId == USER_SERVICE_USER_ID_INVALID || !age) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountCountry(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetAccountCountryA(int userId, void* countryCode) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !countryCode) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountCountrySdk(int userId, void* countryCode) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !countryCode) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountDateOfBirthSdk(int userId, void* dateOfBirth) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !dateOfBirth) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountId(int userId, std::uint64_t* accountId) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !accountId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountIdSdk(int userId, std::uint64_t* accountId) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !accountId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountLanguage(int userId, const void* accountId, void* language) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !accountId || !language) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountLanguageA(int reqId, int userId, void* language) {
    (void)reqId;
    if (userId == USER_SERVICE_USER_ID_INVALID || !language) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountNpEnv(int userId, void* npEnv) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !npEnv) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAccountType(int userId, std::int32_t* accountType) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !accountType) return SCE_NP_ERROR_INVALID_ARGUMENT;
    constexpr std::int32_t val = 0;
    if (!CopyGuestOut(accountType, &val, sizeof(val))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetActiveSigninState(int userId, void* state) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAuthServerErrorFlag(int userId, void* flag) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !flag) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAuthorizationCode(int reqId, int userId, const void* firstParameter, const void* secondParameter, const void* optionalParameter, std::uint32_t mode, std::uint8_t flag, void* optionalOutput, void* output) {
    (void)reqId;
    (void)optionalParameter;
    (void)flag;
    (void)optionalOutput;
    if (userId == USER_SERVICE_USER_ID_INVALID || !firstParameter || !secondParameter || !output) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (mode != 0 && mode != 3 && mode != 5) return SCE_NP_ERROR_INVALID_PLATFORM_TYPE;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetAuthorizationCodeWithPsnoUri(int reqId, int userId, const char* uri, std::uint32_t mode, void* output, std::size_t size) {
    (void)reqId;
    if (userId == USER_SERVICE_USER_ID_INVALID || !uri || !output) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (size < 1 || size > 0x801) return SCE_NP_ERROR_INVALID_SIZE;
    if (mode != 0 && mode != 3 && mode != 5) return SCE_NP_ERROR_INVALID_PLATFORM_TYPE;
    std::string copiedUri;
    const int savedErrno = errno;
    const int copyResult = CopyGuestString_nid_no_patch(uri, 0x2000, copiedUri);
    errno = savedErrno;
    if (copyResult == 63 || copyResult == 12) return SCE_NP_ERROR_OUT_OF_MEMORY;
    if (copyResult != 0) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetClientCredentialAccessToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetCommunicationRestrictionStatus(int userId, int unknown, void* status) {
    (void)userId;
    if (unknown == -1 || !status) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetGameAccessToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetIssuerId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetNpEnv(void* npEnv) {
    if (!npEnv) return SCE_NP_ERROR_INVALID_ARGUMENT;
    constexpr std::array<std::uint8_t, 0x18> cleared{};
    if (!CopyGuestOut(npEnv, cleared.data(), cleared.size())) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetNpId(int userId, NpId* npId) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !npId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetNpIdByOnlineId(const NpOnlineId* onlineId, NpId* npId) {
    if (!onlineId || !npId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetNpIdSdk(int userId, NpId* npId) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !npId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetOfflineAccountId(int userId, void* accountId, void* flag) {
    (void)accountId;
    (void)flag;
    if (userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetOnlineId(int userId, NpOnlineId* onlineId) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !onlineId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetOnlineIdChangeFlag(int userId, void* flag) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !flag) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetOnlineIdSdk(int userId, NpOnlineId* onlineId) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !onlineId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetParentalControlFlag(int userId, void* flag) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !flag) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetParentalControlInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetPlusMemberType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetPlusMemberTypeNB(int userId, void* memberType) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !memberType) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetServerError(int userId, void* error) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !error) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetSigninState(int userId, void* state) {
    if (userId == USER_SERVICE_USER_ID_INVALID || !state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpManagerIntGetSigninTelemetryInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetTicket(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetTicketA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetTitleToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetTitleTokenWithCheck(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpManagerIntGetUserIdByAccountId(std::uint64_t accountId, int* userId) {
    if (accountId == 0 || !userId) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
}
