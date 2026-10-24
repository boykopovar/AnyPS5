#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550704);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS = static_cast<int>(0x80550706);
static constexpr int SCE_NP_COMMUNITY_ERROR_ABORTED = static_cast<int>(0x80550707);
static constexpr int SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT = static_cast<int>(0x8055070C);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ID = static_cast<int>(0x8055070E);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_TYPE = static_cast<int>(0x80550711);
static constexpr int POLL_FINISHED = 0;
static constexpr int POLL_RUNNING = 1;
static constexpr std::size_t MAX_TITLE_CONTEXTS = 32;
static constexpr std::size_t MAX_REQUESTS = 32;
static constexpr int WORD_FILTER_TITLE_CTX_ID_OFFSET = 0x01000000;
static constexpr int WORD_FILTER_REQUEST_ID_OFFSET = 0x02000000;
static constexpr std::size_t WORD_FILTER_COMMENT_MAX_LENGTH = 0x400;
static constexpr std::uint64_t WORD_FILTER_ASYNC_PARAMETER_SIZE = 0x18;

namespace {
struct Request {
    int titleCtxId = 0;
    bool async = false;
    bool used = false;
    bool aborted = false;
    bool finished = false;
    int result = 0;
};

struct Service {
    Service(int firstTitleContext, int firstRequest) : nextTitleContext(firstTitleContext), nextRequest(firstRequest) {}
    std::mutex mutex;
    std::unordered_set<int> titleContexts;
    std::unordered_map<int, Request> requests;
    int nextTitleContext;
    int nextRequest;
};

Service lookup(1, 1);
Service wordFilter(WORD_FILTER_TITLE_CTX_ID_OFFSET + 1, WORD_FILTER_REQUEST_ID_OFFSET + 1);

int CreateTitleContext(Service& service, const void* selfNpId) {
    if (!selfNpId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    std::lock_guard lock(service.mutex);
    if (service.titleContexts.size() >= MAX_TITLE_CONTEXTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int titleCtxId = service.nextTitleContext++;
    service.titleContexts.insert(titleCtxId);
    return titleCtxId;
}

int DeleteTitleContext(Service& service, int titleCtxId) {
    std::lock_guard lock(service.mutex);
    if (!service.titleContexts.erase(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    std::erase_if(service.requests, [titleCtxId](const auto& entry) { return entry.second.titleCtxId == titleCtxId; });
    return 0;
}

int CreateRequest(Service& service, int titleCtxId, bool async) {
    std::lock_guard lock(service.mutex);
    if (!service.titleContexts.contains(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (service.requests.size() >= MAX_REQUESTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int reqId = service.nextRequest++;
    service.requests.emplace(reqId, Request{titleCtxId, async});
    return reqId;
}

int DeleteRequest(Service& service, int reqId) {
    std::lock_guard lock(service.mutex);
    return service.requests.erase(reqId) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
}

int StartOffline(Service& service, int reqId) {
    std::lock_guard lock(service.mutex);
    const auto request = service.requests.find(reqId);
    if (request == service.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (request->second.aborted) return SCE_NP_COMMUNITY_ERROR_ABORTED;
    if (request->second.used) return SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
    request->second.used = true;
    request->second.finished = true;
    request->second.result = SCE_NP_ERROR_SIGNED_OUT;
    return request->second.async ? 0 : SCE_NP_ERROR_SIGNED_OUT;
}

int CheckComment(const char* comment) {
    return strnlen(comment, WORD_FILTER_COMMENT_MAX_LENGTH + 1) > WORD_FILTER_COMMENT_MAX_LENGTH ? SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT : 0;
}
}

extern "C" {

int SceNpUtilityModuleLoaded_nid_no_patch = 1;

int APS5_VABI sceNpBandwidthTestAbort(int context_id) {
    (void)context_id;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpBandwidthTestGetStatus(int context_id, int* status) {
    (void)context_id;
    if (status) *status = 0;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpBandwidthTestInitStartDownload(const void* param) {
    (void)param;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpBandwidthTestInitStartUpload(const void* param) {
    (void)param;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpBandwidthTestShutdown(int context_id, void* result) {
    (void)context_id;
    (void)result;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

}

extern "C" {

int APS5_VABI sceNpBandwidthTestInitStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpLookupCreateRequest(int titleCtxId) {
    return CreateRequest(lookup, titleCtxId, false);
}

int APS5_VABI sceNpLookupCreateTitleCtx(const void* selfNpId) {
    return CreateTitleContext(lookup, selfNpId);
}

int APS5_VABI sceNpLookupDeleteRequest(int reqId) {
    return DeleteRequest(lookup, reqId);
}

int APS5_VABI sceNpLookupDeleteTitleCtx(int titleCtxId) {
    return DeleteTitleContext(lookup, titleCtxId);
}

int APS5_VABI sceNpLookupNpId(int reqId, const NpOnlineId* onlineId, void* npId, void* option) {
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (!onlineId || !npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return StartOffline(lookup, reqId);
}

int APS5_VABI sceNpWordFilterAbortRequest(int reqId) {
    std::lock_guard lock(wordFilter.mutex);
    const auto request = wordFilter.requests.find(reqId);
    if (request == wordFilter.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    request->second.aborted = true;
    if (!request->second.finished) {
        request->second.finished = true;
        request->second.result = SCE_NP_COMMUNITY_ERROR_ABORTED;
    }
    return 0;
}

int APS5_VABI sceNpWordFilterCensorComment(int reqId, const char* comment, void* option) {
    if (!comment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckComment(comment)) return error;
    return StartOffline(wordFilter, reqId);
}

int APS5_VABI sceNpWordFilterCreateAsyncRequest(int titleCtxId, const std::uint64_t* param) {
    if (!param || *param != WORD_FILTER_ASYNC_PARAMETER_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return CreateRequest(wordFilter, titleCtxId, true);
}

int APS5_VABI sceNpWordFilterCreateRequest(int titleCtxId) {
    return CreateRequest(wordFilter, titleCtxId, false);
}

int APS5_VABI sceNpWordFilterCreateTitleCtx(const void* selfNpId) {
    return CreateTitleContext(wordFilter, selfNpId);
}

int APS5_VABI sceNpWordFilterDeleteRequest(int reqId) {
    return DeleteRequest(wordFilter, reqId);
}

int APS5_VABI sceNpWordFilterDeleteTitleCtx(int titleCtxId) {
    return DeleteTitleContext(wordFilter, titleCtxId);
}

int APS5_VABI sceNpWordFilterPollAsync(int reqId, int* result) {
    if (!result) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(wordFilter.mutex);
    const auto request = wordFilter.requests.find(reqId);
    if (request == wordFilter.requests.end() || !request->second.async) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.finished) return POLL_RUNNING;
    *result = request->second.result;
    return POLL_FINISHED;
}

int APS5_VABI sceNpWordFilterSanitizeComment(int reqId, const char* comment, char* sanitizedComment, void* option) {
    if (!comment || !sanitizedComment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckComment(comment)) return error;
    return StartOffline(wordFilter, reqId);
}

}
