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
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_SLOTID = static_cast<int>(0x80550718);
static constexpr int POLL_FINISHED = 0;
static constexpr int POLL_RUNNING = 1;
static constexpr std::size_t MAX_TITLE_CONTEXTS = 32;
static constexpr std::size_t MAX_REQUESTS = 256;
static constexpr int MAX_SLOTS = 64;

namespace {
struct Request {
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

int StartOffline(int reqId) {
    std::lock_guard lock(tusMutex);
    const auto request = requests.find(reqId);
    if (request == requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    request->second.started = true;
    request->second.result = request->second.aborted ? SCE_NP_COMMUNITY_ERROR_ABORTED : SCE_NP_ERROR_SIGNED_OUT;
    return 0;
}
}

extern "C" {

int APS5_VABI sceNpTusCreateNpTitleCtxA(std::uint32_t serviceLabel, int userId) {
    (void)serviceLabel;
    (void)userId;
    std::lock_guard lock(tusMutex);
    if (titleContexts.size() >= MAX_TITLE_CONTEXTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int contextId = nextTitleContext++;
    titleContexts.insert(contextId);
    return contextId;
}

int APS5_VABI sceNpTusCreateRequest(int titleCtxId) {
    std::lock_guard lock(tusMutex);
    if (!titleContexts.contains(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (requests.size() >= MAX_REQUESTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int reqId = nextRequest++;
    requests.emplace(reqId, Request{});
    return reqId;
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

}
