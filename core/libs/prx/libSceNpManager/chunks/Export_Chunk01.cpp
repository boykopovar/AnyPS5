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

static bool CopyGuestOut(void* dst, const void* src, std::size_t size) {
    const auto preserved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = preserved;
    return ok;
}

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

static bool AsmInitialized() {
    std::lock_guard lock(g_asmMutex);
    return g_asmInitialized;
}

static bool AsmClientInitialized() {
    std::lock_guard lock(g_asmMutex);
    return g_asmClientInitialized;
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
NpReachabilityStateCallback reachabilityCallback = nullptr;
void* reachabilityUserdata = nullptr;
bool reachabilityPending = false;
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

struct ToolkitStateCallback {
    NpStateCallbackForToolkit callback = nullptr;
    void* userdata = nullptr;
    bool pending = false;
};

std::mutex stateMutex;
std::array<StateCallbackA, NP_STATE_CALLBACK_MAX> stateCallbacksA;
StateCallback stateCallback;
ToolkitStateCallback toolkitStateCallback;

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
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientClearNpTitleTokenA(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientCreateRequest2(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientCreateResourceContext(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientCreateResourceContext2(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientDeleteRequest(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientDeleteResourceContext(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientDeleteResourceContext2(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetCacheControlMaxAge(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetGameNpTitleInfo(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetGameNpTitleToken(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetGameTitleBanInfo(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo2(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo2A(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo3(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpComInfo4(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleId(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleToken(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleToken2(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetNpTitleTokenA(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetRelatedGameNpTitleIds(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetRelatedGameNpTitleIdsA(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetRelatedGameNpTitleIdsResult(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrl(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrlA(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrlWithNpTitleId(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceBaseUrlWithNpTitleIdA(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceIdInfo(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmClientGetServiceIdInfoA(void) {
    if (!AsmClientInitialized()) return SCE_NP_ASM_CLIENT_ERROR_NOT_INITIALIZED;
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
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGenerateNpTitleToken2(void) {
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetNpCommInfo(void) {
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetNpCommInfo2(void) {
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetRelatedGameNpTitleIds(void) {
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetServiceBaseUrl(void) {
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAsmGetServiceIdInfo(void) {
    if (!AsmInitialized()) return SCE_NP_ASM_ERROR_NOT_INITIALIZED;
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
    NpOnlineId localOnlineId{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localOnlineId, onlineId, sizeof(localOnlineId))) {
        errno = savedErrno;
        return SCE_NP_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t presence = NP_GAME_PRESENCE_STATUS_OFFLINE;
    const bool copied = GuestCopyOut_nid_no_patch(status, &presence, sizeof(presence));
    errno = savedErrno;
    if (!copied) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}
int APS5_VABI sceNpGetGamePresenceStatusA(int userId, uint32_t* status) {
    if (!status || userId == USER_SERVICE_USER_ID_INVALID) return SCE_NP_ERROR_INVALID_ARGUMENT;
    const uint32_t presence = NP_GAME_PRESENCE_STATUS_OFFLINE;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &presence, sizeof(presence));
    errno = savedErrno;
    if (!copied) return SCE_NP_ERROR_INVALID_ARGUMENT;
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
}
