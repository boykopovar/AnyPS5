#include "prx/libSceNpManager/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceNpManager/include/InGameMessageIds.hpp"
#include <cstdint>
#include <cstddef>
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

static constexpr int NP_IN_GAME_MESSAGE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552B02);
static constexpr int SCE_NP_ASM_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055A201);
static constexpr int SCE_NP_ASM_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055A202);
static constexpr int SCE_NP_ASM_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055A203);
static constexpr int SCE_NP_ASM_CLIENT_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055A281);
static constexpr int SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055A282);
static constexpr int SCE_NP_ASM_CLIENT_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055A283);

namespace {
std::mutex g_asmMutex;
bool g_asmInitialized = false;
bool g_asmClientInitialized = false;
const void* g_asmClient = nullptr;
std::array<char, 16> g_asmTitleId = {};
std::set<const void*> g_asmConnections;
std::set<const void*> g_asmRequests;
}

static constexpr int NP_IN_GAME_MESSAGE_ERROR_LIB_CONTEXT_NOT_FOUND = static_cast<int>(0x80552B03);
static constexpr int NP_IN_GAME_MESSAGE_ERROR_HANDLE_NOT_FOUND = static_cast<int>(0x80552B05);
static constexpr int NP_IN_GAME_MESSAGE_ERROR_NOT_CONNECTED = static_cast<int>(0x80552B08);

namespace {
struct InGameMessageCallback {
    std::uint64_t address = 0;
    void* userdata = nullptr;
};

struct InGameMessageContext {
    std::set<std::int32_t> handles;
    std::map<std::int32_t, InGameMessageCallback> callbacks;
};

std::mutex inGameMessageMutex;
std::map<std::int32_t, InGameMessageContext> inGameMessageContexts;
std::int32_t nextInGameMessageContext = 1;
std::int32_t nextInGameMessageHandle = 1;

InGameMessageContext* FindInGameMessageContext(std::int32_t libCtxId) {
    const auto found = inGameMessageContexts.find(libCtxId);
    return found == inGameMessageContexts.end() ? nullptr : &found->second;
}
}

namespace {
std::mutex reachabilityMutex;
void* reachabilityCallback = nullptr;
void* reachabilityUserdata = nullptr;
std::mutex requestMutex;
std::unordered_set<int> liveRequests;

int AddRequest() {
    const int reqId = g_nextRequest.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(requestMutex);
    liveRequests.insert(reqId);
    return reqId;
}

std::mutex idMapperMutex;
std::uint32_t nextIdMapperRequest = 1;
std::unordered_set<int> idMapperRequests;
constexpr std::size_t NP_MANAGER_INT_REQUEST_SLOTS = 64;
std::mutex managerIntMutex;
std::uint32_t nextManagerIntRequest = 1;
std::unordered_set<int> managerIntRequests;

int SignedOutRequest(int reqId) {
    if (reqId == 0) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(requestMutex);
    return liveRequests.count(reqId) ? SCE_NP_ERROR_SIGNED_OUT : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}

constexpr int NP_STATE_CALLBACK_MAX = 8;

struct StateCallbackA {
    NpStateCallbackA callback = nullptr;
    void* userdata = nullptr;
    bool pending = false;
};

struct StateCallback {
    NpStateCallback callback = nullptr;
    void* userdata = nullptr;
    bool pending = false;
};

std::mutex stateMutex;
std::array<StateCallbackA, NP_STATE_CALLBACK_MAX> stateCallbacksA;
StateCallback stateCallback;

constexpr int NP_GAME_PRESENCE_CALLBACK_MAX = 8;

struct GamePresenceCallbackEntry {
    int handle = 0;
    void* callback = nullptr;
    void* userdata = nullptr;
    bool inUse = false;
};

std::mutex gamePresenceMutex;
std::array<GamePresenceCallbackEntry, NP_GAME_PRESENCE_CALLBACK_MAX> gamePresenceCallbacksA{};
int nextGamePresenceHandle = 0;
}

extern "C" {

int APS5_VABI sceNpAsmAbort(void) {
    std::lock_guard lock(g_asmMutex);
    if (!g_asmInitialized) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    return 0;
}
int APS5_VABI sceNpAsmClientAbortRequest(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientClearNpTitleToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientClearNpTitleTokenA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientCreateRequest2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientCreateResourceContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientCreateResourceContext2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientDeleteRequest(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientDeleteResourceContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientDeleteResourceContext2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetCacheControlMaxAge(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetGameNpTitleInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetGameNpTitleToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetGameTitleBanInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo2A(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleToken2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleTokenA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetRelatedGameNpTitleIds(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetRelatedGameNpTitleIdsA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetRelatedGameNpTitleIdsResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrlA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrlWithNpTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrlWithNpTitleIdA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceIdInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceIdInfoA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientInitialize(void* client, const void* param) {
    std::lock_guard lock(g_asmMutex);
    if (g_asmClientInitialized) return SCE_NP_ASM_CLIENT_ERROR_ALREADY_INITIALIZED;
    if (!param) return SCE_NP_ASM_CLIENT_ERROR_INVALID_ARGUMENT;
    g_asmClientInitialized = true;
    g_asmClient = client;
    return 0;
}
int APS5_VABI sceNpAsmClientSetNpTitleId(void* client, const char* npTitleId) {
    std::lock_guard lock(g_asmMutex);
    if (!g_asmClientInitialized) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    if (!client || !npTitleId) return SCE_NP_ASM_CLIENT_ERROR_INVALID_ARGUMENT;
    std::array<char, 16> title = {};
    for (std::size_t index = 0; index < title.size(); ++index) {
        if (!GuestCopyIn_nid_no_patch(&title[index], npTitleId + index, sizeof(char))) return SCE_NP_ASM_CLIENT_ERROR_INVALID_ARGUMENT;
        if (title[index] == 0) break;
    }
    g_asmTitleId = title;
    return 0;
}
int APS5_VABI sceNpAsmClientTerminate(void* client) {
    (void)client;
    std::lock_guard lock(g_asmMutex);
    if (!g_asmClientInitialized) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    g_asmClientInitialized = false;
    g_asmClient = nullptr;
    g_asmTitleId.fill(0);
    return 0;
}
int APS5_VABI sceNpAsmCreateConnection(std::int32_t connectionId, void* connection) {
    (void)connectionId;
    std::lock_guard lock(g_asmMutex);
    if (!g_asmInitialized) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    if (!connection) return SCE_NP_ASM_ERROR_INVALID_ARGUMENT;
    g_asmConnections.insert(connection);
    return 0;
}
int APS5_VABI sceNpAsmCreateRequest(void* connection, void* request, void* result) {
    std::lock_guard lock(g_asmMutex);
    if (!g_asmInitialized) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    if (!connection || !request || !result) return SCE_NP_ASM_ERROR_INVALID_ARGUMENT;
    g_asmRequests.insert(request);
    return 0;
}
int APS5_VABI sceNpAsmDeleteConnection(void* connection) {
    std::lock_guard lock(g_asmMutex);
    if (!g_asmInitialized) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    if (!connection) return SCE_NP_ASM_ERROR_INVALID_ARGUMENT;
    g_asmConnections.erase(connection);
    return 0;
}
int APS5_VABI sceNpAsmDeleteRequest(void* request) {
    std::lock_guard lock(g_asmMutex);
    if (!g_asmInitialized) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    g_asmRequests.erase(request);
    return 0;
}
int APS5_VABI sceNpAsmGenerateNpTitleToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGenerateNpTitleToken2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetNpCommInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetNpCommInfo2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetRelatedGameNpTitleIds(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetServiceBaseUrl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetServiceIdInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmInitialize(void) {
    std::lock_guard lock(g_asmMutex);
    if (g_asmInitialized) return SCE_NP_ASM_ERROR_ALREADY_INITIALIZED;
    g_asmInitialized = true;
    return 0;
}
int APS5_VABI sceNpAsmTerminate(void) {
    std::lock_guard lock(g_asmMutex);
    if (!g_asmInitialized) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    g_asmInitialized = false;
    return 0;
}
int APS5_VABI sceNpCheckNpAvailabilityA(int reqId, int userId) {
    if (userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SignedOutRequest(reqId);
}
int APS5_VABI sceNpGetAccountDateOfBirthA(int userId, void* dateOfBirth) {
    if (!dateOfBirth || userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpGetAccountLanguageA(int reqId, int userId, void* language) {
    if (!language || userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SignedOutRequest(reqId);
}
int APS5_VABI sceNpGetGamePresenceStatus(const NpOnlineId* onlineId, uint32_t* status) {
    if (!onlineId || !status) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *status = NP_GAME_PRESENCE_STATUS_OFFLINE;
    return 0;
}
int APS5_VABI sceNpGetGamePresenceStatusA(int userId, uint32_t* status) {
    if (!status || userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *status = NP_GAME_PRESENCE_STATUS_OFFLINE;
    return 0;
}
int APS5_VABI sceNpGetParentalControlInfoA(int reqId, int userId, int8_t* age, void* info) {
    if (!age || !info || userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SignedOutRequest(reqId);
}
int APS5_VABI sceNpIdMapperAbortRequest(int reqId) {
    if (reqId <= 0) return NP_ID_MAPPER_REFUSED_REQUEST_ID;
    std::lock_guard lock(idMapperMutex);
    return idMapperRequests.count(reqId) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}
int APS5_VABI sceNpIdMapperAccountIdToNpId(int reqId, int userId, const void* input, void* output, std::uint32_t value, std::uint8_t flag, std::uint64_t extra) {
    (void)flag;
    (void)extra;
    if (reqId <= 0) return NP_ID_MAPPER_REFUSED_REQUEST_ID;
    if (userId == USER_SERVICE_USER_ID_INVALID || !input || !output || value < 1 || value > 1000) return NP_ID_MAPPER_REFUSED_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpIdMapperAccountIdToOnlineId(int reqId, int userId, const void* input, void* output, std::uint32_t value, std::uint8_t flag, std::uint64_t extra) {
    (void)flag;
    (void)extra;
    if (reqId <= 0) return NP_ID_MAPPER_REFUSED_REQUEST_ID;
    if (userId == USER_SERVICE_USER_ID_INVALID || !input || !output || value < 1 || value > 1000) return NP_ID_MAPPER_REFUSED_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpIdMapperCreateRequest(void) {
    std::lock_guard lock(idMapperMutex);
    const int reqId = static_cast<int>(nextIdMapperRequest++);
    idMapperRequests.insert(reqId);
    return reqId;
}
int APS5_VABI sceNpIdMapperDeleteRequest(int reqId) {
    if (reqId <= 0) return NP_ID_MAPPER_REFUSED_REQUEST_ID;
    std::lock_guard lock(idMapperMutex);
    return idMapperRequests.erase(reqId) ? 0 : SCE_NP_ERROR_REQUEST_NOT_FOUND;
}
int APS5_VABI sceNpIdMapperNpIdToAccountId(int reqId, int userId, const void* input, void* output, std::uint32_t value, std::uint8_t flag, std::uint64_t extra) {
    (void)flag;
    (void)extra;
    if (reqId <= 0) return NP_ID_MAPPER_REFUSED_REQUEST_ID;
    if (userId == USER_SERVICE_USER_ID_INVALID || !input || !output || value < 1 || value > 1000) return NP_ID_MAPPER_REFUSED_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}
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
    *isMember = false;
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
    *accountType = 0;
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
    if (!std::memchr(uri, 0, 0x2000)) return SCE_NP_ERROR_OUT_OF_MEMORY;
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
    std::memset(npEnv, 0, 0x18);
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
    {
        std::lock_guard lock(stateMutex);
        callbacksA = stateCallbacksA;
        legacy = stateCallback;
        for (auto& entry : stateCallbacksA) entry.pending = false;
        stateCallback.pending = false;
    }
    if (legacy.pending) legacy.callback(USER_SERVICE_INITIAL_USER_ID, NP_STATE_SIGNED_OUT, nullptr, legacy.userdata);
    for (const auto& entry : callbacksA) {
        if (entry.pending) entry.callback(USER_SERVICE_INITIAL_USER_ID, NP_STATE_SIGNED_OUT, entry.userdata);
    }
    return 0;
}

int APS5_VABI sceNpCheckNpAvailability(std::int32_t requestId, const NpOnlineId* onlineId, std::uint8_t* available) {
 if (requestId == 0 || onlineId == nullptr) return SCE_NP_ERROR_INVALID_ARGUMENT;
 if (available != nullptr) *available = 0;
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
    (void)user_id;
    (void)account_id;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id) {
    (void)user_id;
    if (!np_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetNpReachabilityState(int user_id, uint32_t* state) {
    (void)user_id;
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *state = NP_REACHABILITY_STATE_UNAVAILABLE;
    return 0;
}

int APS5_VABI sceNpGetOnlineId(int user_id, NpOnlineId* online_id) {
    (void)user_id;
    if (!online_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetState(int user_id, uint32_t* state) {
    (void)user_id;
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *state = NP_STATE_SIGNED_OUT;
    return 0;
}

int APS5_VABI sceNpHasSignedUp(int user_id, bool* has_signed_up) {
    (void)user_id;
    if (!has_signed_up) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *has_signed_up = false;
    return 0;
}

int APS5_VABI sceNpPollAsync(int req_id, int* result) {
    if (!result) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(requestMutex);
    if (!liveRequests.count(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    *result = SCE_NP_ERROR_SIGNED_OUT;
    return NP_POLL_ASYNC_FINISHED;
}

int APS5_VABI sceNpWaitAsync(int req_id, int* result) {
    if (!result) return SCE_NP_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(requestMutex);
    if (!liveRequests.count(req_id)) return SCE_NP_ERROR_REQUEST_NOT_FOUND;
    *result = SCE_NP_ERROR_SIGNED_OUT;
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
    reachabilityCallback = callback;
    reachabilityUserdata = userdata;
    return 0;
}

int APS5_VABI sceNpUnregisterNpReachabilityStateCallback(void) {
    std::lock_guard lock(reachabilityMutex);
    if (!reachabilityCallback) return SCE_NP_ERROR_CALLBACK_NOT_REGISTERED;
    reachabilityCallback = nullptr;
    reachabilityUserdata = nullptr;
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

int APS5_VABI sceNpGetAccountLanguage2(int req_id, int user_id, void* language) {
    (void)req_id;
    (void)user_id;
    (void)language;
    return SCE_NP_ERROR_SIGNED_OUT;
}

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
    auto* info = static_cast<char*>(sandbox_info);
    std::memset(info, 0, 0x50);
    std::memcpy(info, Platform, sizeof(Platform));
    std::memcpy(info + 0x28, Environment, sizeof(Environment));
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
