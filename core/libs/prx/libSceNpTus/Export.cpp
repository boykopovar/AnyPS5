#include "prx/libSceNpTus/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550704);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS = static_cast<int>(0x80550706);
static constexpr int SCE_NP_COMMUNITY_ERROR_ABORTED = static_cast<int>(0x80550707);
static constexpr int SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT = static_cast<int>(0x8055070C);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ID = static_cast<int>(0x8055070E);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_TYPE = static_cast<int>(0x80550711);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID = static_cast<int>(0x80550718);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID = static_cast<int>(0x80550719);
static constexpr int POLL_FINISHED = 0;
static constexpr int POLL_RUNNING = 1;
static constexpr std::size_t MAX_TITLE_CONTEXTS = 32;
static constexpr std::size_t MAX_REQUESTS = 256;
static constexpr int MAX_SLOTS = 64;
static constexpr int MAX_USERS = 101;
static constexpr int MAX_FRIENDS = 100;
static constexpr std::int32_t TSS_MAX_SLOT_ID = 15;
static constexpr std::uint64_t TSS_MAX_SIZE = 64 * 1024;
static constexpr std::int32_t OPETYPE_EQUAL = 1;
static constexpr std::int32_t OPETYPE_LESS_OR_EQUAL = 6;
static constexpr std::int32_t DATASTATUS_SORTTYPE_DESCENDING_DATE = 1;
static constexpr std::int32_t DATASTATUS_SORTTYPE_ASCENDING_DATE = 2;
static constexpr std::int32_t VARIABLE_SORTTYPE_DESCENDING_DATE = 1;
static constexpr std::int32_t VARIABLE_SORTTYPE_ASCENDING_VALUE = 4;

namespace {
struct Request {
    int titleCtxId = 0;
    bool started = false;
    bool aborted = false;
    int result = 0;
};

std::mutex tusMutex;
std::unordered_set<int> titleContexts;
std::unordered_map<int, Request> requests;
int nextTitleContext = 1;
int nextRequest = 1;

int ValidateSlots(const std::int32_t* slotIds, int arrayLen, const void* option) {
    if (!slotIds) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (arrayLen < 1 || option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    for (int index = 0; index < arrayLen; ++index) {
        if (slotIds[index] < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return 0;
}

int ValidateNonEmptySlots(const std::int32_t* slotIds, int arrayLen, const void* option) {
    if (!slotIds || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return ValidateSlots(slotIds, arrayLen, option);
}

bool IsEmpty(const NpOnlineId* user) {
    return !user->data[0];
}

int StartOffline(int reqId) {
    std::lock_guard lock(tusMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    request->second.started = true;
    request->second.result = request->second.aborted ? SCE_NP_COMMUNITY_ERROR_ABORTED : SCE_NP_ERROR_SIGNED_OUT;
    return 0;
}

int WaitOffline(int reqId, int* result) {
    std::lock_guard lock(tusMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.started) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (result) *result = request->second.result;
    return 0;
}

int Complete(int reqId, int asyncResult) {
    if (asyncResult < 0) return asyncResult;
    int result = 0;
    if (const int error = WaitOffline(reqId, &result)) return error;
    return result;
}

constexpr std::uint32_t INVALID_SERVICE_LABEL = 0xFFFFFFFF;

int CreateTitleContext() {
    std::lock_guard lock(tusMutex);
    if (titleContexts.size() >= MAX_TITLE_CONTEXTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int contextId = nextTitleContext++;
    titleContexts.insert(contextId);
    return contextId;
}
}

extern "C" {

int APS5_VABI sceNpTusCreateNpTitleCtxA(std::uint32_t serviceLabel, int userId) {
    (void)serviceLabel;
    (void)userId;
    return CreateTitleContext();
}

int APS5_VABI sceNpTusCreateRequest(int titleCtxId) {
    std::lock_guard lock(tusMutex);
    if (!titleContexts.contains(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (requests.size() >= MAX_REQUESTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int reqId = nextRequest++;
    requests.emplace(reqId, Request{titleCtxId});
    return reqId;
}

int APS5_VABI sceNpTusDeleteNpTitleCtx(int titleCtxId) {
    std::lock_guard lock(tusMutex);
    if (!titleContexts.erase(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    std::erase_if(requests, [titleCtxId](const auto& entry) { return entry.second.titleCtxId == titleCtxId; });
    return 0;
}

int APS5_VABI sceNpTusDeleteRequest(int reqId) {
    std::lock_guard lock(tusMutex);
    return requests.erase(reqId) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
}

int APS5_VABI sceNpTusAbortRequest(int reqId) {
    std::lock_guard lock(tusMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    request->second.aborted = true;
    return 0;
}

int APS5_VABI sceNpTusPollAsync(int reqId, int* result) {
    std::lock_guard lock(tusMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.started) return POLL_RUNNING;
    if (result) *result = request->second.result;
    return POLL_FINISHED;
}

int APS5_VABI sceNpTusGetMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)targetAccountId;
    (void)variableArraySize;
    if (!variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option) {
    (void)targetAccountId;
    if (!variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusDeleteMultiSlotVariableAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option) {
    (void)targetAccountId;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusAddAndGetVariableAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    (void)targetAccountId;
    (void)inVariable;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)outVariableSize;
    if (!outVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0 || option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetThreadParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTssCreateNpTitleCtx(std::uint32_t serviceLabel, const NpId* npId) {
    if (!npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (serviceLabel == INVALID_SERVICE_LABEL) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return CreateTitleContext();
}

int APS5_VABI sceNpTssCreateNpTitleCtxA(std::uint32_t serviceLabel, int userId) {
    return sceNpTusCreateNpTitleCtxA(serviceLabel, userId);
}

int APS5_VABI sceNpTusCreateTitleCtx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusSetTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusChangeModeForOtherSaveDataOwners(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusWaitAsync(int reqId, int* result) {
    return WaitOffline(reqId, result);
}

int APS5_VABI sceNpTusGetMultiSlotVariableA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotVariableAAsync(reqId, targetAccountId, slotIds, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotVariableAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)targetNpId;
    (void)variableArraySize;
    if (!variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetVirtualUserId || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateNonEmptySlots(slotIds, arrayLen, option)) return error;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetVirtualUserId || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotVariableAVUserAsync(reqId, targetVirtualUserId, slotIds, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)targetAccountId;
    (void)variableArraySize;
    if (!variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSave(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotVariableForCrossSaveAsync(reqId, targetAccountId, slotIds, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetVirtualUserId || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotVariableForCrossSaveVUserAsync(reqId, targetVirtualUserId, slotIds, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusSetMultiSlotVariableA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusSetMultiSlotVariableAAsync(reqId, targetAccountId, slotIds, variableArray, arrayLen, option));
}

int APS5_VABI sceNpTusSetMultiSlotVariableAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option) {
    (void)targetNpId;
    if (!variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetMultiSlotVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, const std::int64_t* variableArray, int arrayLen, void* option) {
    if (!targetVirtualUserId || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateSlots(slotIds, arrayLen, option)) return error;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusDeleteMultiSlotVariableA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusDeleteMultiSlotVariableAAsync(reqId, targetAccountId, slotIds, arrayLen, option));
}

int APS5_VABI sceNpTusDeleteMultiSlotVariableAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option) {
    if (!targetNpId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateNonEmptySlots(slotIds, arrayLen, option)) return error;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusDeleteMultiSlotVariable(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusDeleteMultiSlotVariableAsync(reqId, targetNpId, slotIds, arrayLen, option));
}

int APS5_VABI sceNpTusDeleteMultiSlotVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, int arrayLen, void* option) {
    if (!targetVirtualUserId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (const int error = ValidateNonEmptySlots(slotIds, arrayLen, option)) return error;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusDeleteMultiSlotVariableVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusDeleteMultiSlotVariableVUserAsync(reqId, targetVirtualUserId, slotIds, arrayLen, option));
}

int APS5_VABI sceNpTusDeleteMultiSlotDataVUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusDeleteMultiSlotDataVUserAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTssGetStorage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTssGetStorageAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTssGetDataAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetDataForCrossSaveVUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetDataForCrossSaveVUserAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetMultiUserVariableForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsVariableA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsVariableAAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsVariableForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsVariableForCrossSaveAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsDataStatusA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsDataStatusAAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsDataStatusForCrossSave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetFriendsDataStatusForCrossSaveAsync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTssGetSmallStorageAsync(int reqId, void* data, std::uint64_t maxSize, std::uint64_t* contentLength, void* option) {
    (void)data;
    (void)contentLength;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (maxSize > TSS_MAX_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusAddAndGetVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    (void)inVariable;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)outVariableSize;
    if (!targetVirtualUserId || !outVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusAddAndGetVariableAsync(int reqId, const void* targetNpId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    (void)targetNpId;
    (void)inVariable;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)outVariableSize;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!outVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusAddAndGetVariableForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    (void)targetAccountId;
    (void)inVariable;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)outVariableSize;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!outVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusAddAndGetVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    (void)inVariable;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)outVariableSize;
    if (!targetVirtualUserId || !outVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusAddAndGetVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    (void)inVariable;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)outVariableSize;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!targetVirtualUserId || !outVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusDeleteMultiSlotDataAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option) {
    (void)targetAccountId;
    if (!slotIds) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusDeleteMultiSlotDataAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option) {
    if (!targetNpId || !slotIds || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetDataAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    (void)targetAccountId;
    (void)data;
    (void)recvSize;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!dataStatus || dataStatusSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetDataAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    (void)data;
    (void)recvSize;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!targetVirtualUserId || !dataStatus || dataStatusSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetDataAsync(int reqId, const void* targetNpId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    (void)targetNpId;
    (void)data;
    (void)recvSize;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!dataStatus || dataStatusSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetDataForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    (void)targetAccountId;
    (void)data;
    (void)recvSize;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!dataStatus || dataStatusSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetDataVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    (void)data;
    (void)recvSize;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!targetVirtualUserId || !dataStatus || dataStatusSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetFriendsDataStatusAsync(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)includeSelf;
    (void)statusArraySize;
    if (!statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_FRIENDS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (sortType < DATASTATUS_SORTTYPE_DESCENDING_DATE || sortType > DATASTATUS_SORTTYPE_ASCENDING_DATE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetFriendsVariableAsync(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)includeSelf;
    (void)variableArraySize;
    if (!variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_FRIENDS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (sortType < VARIABLE_SORTTYPE_DESCENDING_DATE || sortType > VARIABLE_SORTTYPE_ASCENDING_VALUE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusAAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)targetAccountId;
    (void)statusArraySize;
    if (!slotIds || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetVirtualUserId || !slotIds || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusAsync(int reqId, const void* targetNpId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetNpId || !slotIds || !statusArray || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)targetAccountId;
    (void)statusArraySize;
    if (!slotIds || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetVirtualUserId || !slotIds || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetVirtualUserId || !slotIds || !statusArray || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_SLOTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    if (std::any_of(slotIds, slotIds + arrayLen, [](std::int32_t id) { return id < 0; })) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserDataStatusAAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetAccountIdArray || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserDataStatusAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetVirtualUserIdArray || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    for (int index = 0; index < arrayLen; ++index) {
        if (IsEmpty(&targetVirtualUserIdArray[index])) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserDataStatusAsync(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetNpIdArray || !statusArray || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSaveAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetAccountIdArray || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetVirtualUserIdArray || !statusArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    for (int index = 0; index < arrayLen; ++index) {
        if (IsEmpty(&targetVirtualUserIdArray[index])) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserDataStatusVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    (void)statusArraySize;
    if (!targetVirtualUserIdArray || !statusArray || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    for (int index = 0; index < arrayLen; ++index) {
        if (IsEmpty(&targetVirtualUserIdArray[index])) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserVariableAAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetAccountIdArray || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetVirtualUserIdArray || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    for (int index = 0; index < arrayLen; ++index) {
        if (IsEmpty(&targetVirtualUserIdArray[index])) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserVariableAsync(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetNpIdArray || !variableArray || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserVariableForCrossSaveAsync(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetAccountIdArray || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetVirtualUserIdArray || !variableArray) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || arrayLen < 1 || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    for (int index = 0; index < arrayLen; ++index) {
        if (IsEmpty(&targetVirtualUserIdArray[index])) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusGetMultiUserVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    (void)variableArraySize;
    if (!targetVirtualUserIdArray || !variableArray || arrayLen < 1) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (arrayLen > MAX_USERS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_NPID;
    for (int index = 0; index < arrayLen; ++index) {
        if (IsEmpty(&targetVirtualUserIdArray[index])) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetDataAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option) {
    (void)targetAccountId;
    (void)info;
    (void)infoStructSize;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!data || sendSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0 || sendSize > totalSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetDataAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option) {
    (void)info;
    (void)infoStructSize;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!targetVirtualUserId || !data || sendSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0 || sendSize > totalSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetDataAsync(int reqId, const void* targetNpId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const void* isLastChangedAuthor, const void* isLastChangedDate, void* option) {
    (void)targetNpId;
    (void)info;
    (void)infoStructSize;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!data || sendSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0 || sendSize > totalSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusSetDataVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const void* isLastChangedAuthor, const void* isLastChangedDate, void* option) {
    (void)info;
    (void)infoStructSize;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!targetVirtualUserId || !data || sendSize == 0) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (slotId < 0 || sendSize > totalSize) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusTryAndSetVariableAAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    (void)targetAccountId;
    (void)variable;
    (void)compareValue;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)resultVariableSize;
    if (!resultVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (opeType < OPETYPE_EQUAL || opeType > OPETYPE_LESS_OR_EQUAL) return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusTryAndSetVariableAVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    (void)variable;
    (void)compareValue;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)resultVariableSize;
    if (!targetVirtualUserId || !resultVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (opeType < OPETYPE_EQUAL || opeType > OPETYPE_LESS_OR_EQUAL) return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusTryAndSetVariableAsync(int reqId, const void* targetNpId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    (void)variable;
    (void)compareValue;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)resultVariableSize;
    if (!targetNpId || !resultVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (opeType < OPETYPE_EQUAL || opeType > OPETYPE_LESS_OR_EQUAL) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusTryAndSetVariableForCrossSaveAsync(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    (void)targetAccountId;
    (void)variable;
    (void)compareValue;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)resultVariableSize;
    if (!resultVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (opeType < OPETYPE_EQUAL || opeType > OPETYPE_LESS_OR_EQUAL) return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusTryAndSetVariableForCrossSaveVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    (void)variable;
    (void)compareValue;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)resultVariableSize;
    if (!targetVirtualUserId || !resultVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (opeType < OPETYPE_EQUAL || opeType > OPETYPE_LESS_OR_EQUAL) return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTusTryAndSetVariableVUserAsync(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    (void)variable;
    (void)compareValue;
    (void)isLastChangedAuthor;
    (void)isLastChangedDate;
    (void)resultVariableSize;
    if (!targetVirtualUserId || !resultVariable) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option || slotId < 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (opeType < OPETYPE_EQUAL || opeType > OPETYPE_LESS_OR_EQUAL) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (IsEmpty(targetVirtualUserId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return StartOffline(reqId);
}

int APS5_VABI sceNpTssGetSmallStorage(int reqId, void* data, std::uint64_t maxSize, std::uint64_t* contentLength, void* option) {
    return Complete(reqId, sceNpTssGetSmallStorageAsync(reqId, data, maxSize, contentLength, option));
}

int APS5_VABI sceNpTusAddAndGetVariable(int reqId, const void* targetNpId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    return Complete(reqId, sceNpTusAddAndGetVariableAsync(reqId, targetNpId, slotId, inVariable, isLastChangedAuthor, isLastChangedDate, outVariable, outVariableSize, option));
}

int APS5_VABI sceNpTusAddAndGetVariableA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    return Complete(reqId, sceNpTusAddAndGetVariableAAsync(reqId, targetAccountId, slotId, inVariable, isLastChangedAuthor, isLastChangedDate, outVariable, outVariableSize, option));
}

int APS5_VABI sceNpTusAddAndGetVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    return Complete(reqId, sceNpTusAddAndGetVariableAVUserAsync(reqId, targetVirtualUserId, slotId, inVariable, isLastChangedAuthor, isLastChangedDate, outVariable, outVariableSize, option));
}

int APS5_VABI sceNpTusAddAndGetVariableForCrossSave(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    return Complete(reqId, sceNpTusAddAndGetVariableForCrossSaveAsync(reqId, targetAccountId, slotId, inVariable, isLastChangedAuthor, isLastChangedDate, outVariable, outVariableSize, option));
}

int APS5_VABI sceNpTusAddAndGetVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    return Complete(reqId, sceNpTusAddAndGetVariableForCrossSaveVUserAsync(reqId, targetVirtualUserId, slotId, inVariable, isLastChangedAuthor, isLastChangedDate, outVariable, outVariableSize, option));
}

int APS5_VABI sceNpTusAddAndGetVariableVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int64_t inVariable, const void* isLastChangedAuthor, const void* isLastChangedDate, void* outVariable, std::uint64_t outVariableSize, void* option) {
    return Complete(reqId, sceNpTusAddAndGetVariableVUserAsync(reqId, targetVirtualUserId, slotId, inVariable, isLastChangedAuthor, isLastChangedDate, outVariable, outVariableSize, option));
}

int APS5_VABI sceNpTusDeleteMultiSlotData(int reqId, const void* targetNpId, const std::int32_t* slotIds, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusDeleteMultiSlotDataAsync(reqId, targetNpId, slotIds, arrayLen, option));
}

int APS5_VABI sceNpTusDeleteMultiSlotDataA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusDeleteMultiSlotDataAAsync(reqId, targetAccountId, slotIds, arrayLen, option));
}

int APS5_VABI sceNpTusGetDataA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    return Complete(reqId, sceNpTusGetDataAAsync(reqId, targetAccountId, slotId, dataStatus, dataStatusSize, data, recvSize, option));
}

int APS5_VABI sceNpTusGetDataAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    return Complete(reqId, sceNpTusGetDataAVUserAsync(reqId, targetVirtualUserId, slotId, dataStatus, dataStatusSize, data, recvSize, option));
}

int APS5_VABI sceNpTusGetDataForCrossSave(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, void* dataStatus, std::uint64_t dataStatusSize, void* data, std::uint64_t recvSize, void* option) {
    return Complete(reqId, sceNpTusGetDataForCrossSaveAsync(reqId, targetAccountId, slotId, dataStatus, dataStatusSize, data, recvSize, option));
}

int APS5_VABI sceNpTusGetFriendsDataStatus(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetFriendsDataStatusAsync(reqId, slotId, includeSelf, sortType, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetFriendsVariable(int reqId, std::int32_t slotId, std::int32_t includeSelf, std::int32_t sortType, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetFriendsVariableAsync(reqId, slotId, includeSelf, sortType, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotDataStatus(int reqId, const void* targetNpId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotDataStatusAsync(reqId, targetNpId, slotIds, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusA(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotDataStatusAAsync(reqId, targetAccountId, slotIds, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusAVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotDataStatusAVUserAsync(reqId, targetVirtualUserId, slotIds, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSave(int reqId, std::uint64_t targetAccountId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotDataStatusForCrossSaveAsync(reqId, targetAccountId, slotIds, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotDataStatusForCrossSaveVUserAsync(reqId, targetVirtualUserId, slotIds, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiSlotDataStatusVUser(int reqId, const NpOnlineId* targetVirtualUserId, const std::int32_t* slotIds, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiSlotDataStatusVUserAsync(reqId, targetVirtualUserId, slotIds, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserDataStatus(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserDataStatusAsync(reqId, targetNpIdArray, slotId, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserDataStatusA(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserDataStatusAAsync(reqId, targetAccountIdArray, slotId, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserDataStatusAVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserDataStatusAVUserAsync(reqId, targetVirtualUserIdArray, slotId, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSave(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserDataStatusForCrossSaveAsync(reqId, targetAccountIdArray, slotId, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserDataStatusForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserDataStatusForCrossSaveVUserAsync(reqId, targetVirtualUserIdArray, slotId, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserDataStatusVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* statusArray, std::uint64_t statusArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserDataStatusVUserAsync(reqId, targetVirtualUserIdArray, slotId, statusArray, statusArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserVariable(int reqId, const void* targetNpIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserVariableAsync(reqId, targetNpIdArray, slotId, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserVariableA(int reqId, const std::uint64_t* targetAccountIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserVariableAAsync(reqId, targetAccountIdArray, slotId, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserVariableAVUserAsync(reqId, targetVirtualUserIdArray, slotId, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserVariableForCrossSaveVUserAsync(reqId, targetVirtualUserIdArray, slotId, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusGetMultiUserVariableVUser(int reqId, const NpOnlineId* targetVirtualUserIdArray, std::int32_t slotId, void* variableArray, std::uint64_t variableArraySize, int arrayLen, void* option) {
    return Complete(reqId, sceNpTusGetMultiUserVariableVUserAsync(reqId, targetVirtualUserIdArray, slotId, variableArray, variableArraySize, arrayLen, option));
}

int APS5_VABI sceNpTusSetDataA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option) {
    return Complete(reqId, sceNpTusSetDataAAsync(reqId, targetAccountId, slotId, totalSize, sendSize, data, info, infoStructSize, isLastChangedAuthor, isLastChangedDate, option));
}

int APS5_VABI sceNpTusSetDataAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::uint64_t totalSize, std::uint64_t sendSize, const void* data, const void* info, std::uint64_t infoStructSize, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* option) {
    return Complete(reqId, sceNpTusSetDataAVUserAsync(reqId, targetVirtualUserId, slotId, totalSize, sendSize, data, info, infoStructSize, isLastChangedAuthor, isLastChangedDate, option));
}

int APS5_VABI sceNpTusTryAndSetVariable(int reqId, const void* targetNpId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    return Complete(reqId, sceNpTusTryAndSetVariableAsync(reqId, targetNpId, slotId, opeType, variable, compareValue, isLastChangedAuthor, isLastChangedDate, resultVariable, resultVariableSize, option));
}

int APS5_VABI sceNpTusTryAndSetVariableA(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    return Complete(reqId, sceNpTusTryAndSetVariableAAsync(reqId, targetAccountId, slotId, opeType, variable, compareValue, isLastChangedAuthor, isLastChangedDate, resultVariable, resultVariableSize, option));
}

int APS5_VABI sceNpTusTryAndSetVariableAVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    return Complete(reqId, sceNpTusTryAndSetVariableAVUserAsync(reqId, targetVirtualUserId, slotId, opeType, variable, compareValue, isLastChangedAuthor, isLastChangedDate, resultVariable, resultVariableSize, option));
}

int APS5_VABI sceNpTusTryAndSetVariableForCrossSave(int reqId, std::uint64_t targetAccountId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    return Complete(reqId, sceNpTusTryAndSetVariableForCrossSaveAsync(reqId, targetAccountId, slotId, opeType, variable, compareValue, isLastChangedAuthor, isLastChangedDate, resultVariable, resultVariableSize, option));
}

int APS5_VABI sceNpTusTryAndSetVariableForCrossSaveVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const std::uint64_t* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    return Complete(reqId, sceNpTusTryAndSetVariableForCrossSaveVUserAsync(reqId, targetVirtualUserId, slotId, opeType, variable, compareValue, isLastChangedAuthor, isLastChangedDate, resultVariable, resultVariableSize, option));
}

int APS5_VABI sceNpTusTryAndSetVariableVUser(int reqId, const NpOnlineId* targetVirtualUserId, std::int32_t slotId, std::int32_t opeType, std::int64_t variable, const std::int64_t* compareValue, const void* isLastChangedAuthor, const void* isLastChangedDate, void* resultVariable, std::uint64_t resultVariableSize, void* option) {
    return Complete(reqId, sceNpTusTryAndSetVariableVUserAsync(reqId, targetVirtualUserId, slotId, opeType, variable, compareValue, isLastChangedAuthor, isLastChangedDate, resultVariable, resultVariableSize, option));
}

int APS5_VABI sceNpTssGetData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusCreateNpTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetDataVUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetMultiSlotVariable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusGetMultiSlotVariableVUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusSetData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusSetDataVUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusSetMultiSlotVariable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTusSetMultiSlotVariableVUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
