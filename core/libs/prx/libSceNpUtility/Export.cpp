#include "prx/libSceNpUtility/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <condition_variable>
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
static constexpr int SCE_NP_LOOKUP_ERROR_SYNCHRONOUS_REQUEST = static_cast<int>(0x80550307);
static constexpr int POLL_FINISHED = 0;
static constexpr int POLL_RUNNING = 1;
static constexpr std::size_t MAX_TITLE_CONTEXTS = 32;
static constexpr std::size_t MAX_REQUESTS = 32;
static constexpr int WORD_FILTER_TITLE_CTX_ID_OFFSET = 0x01000000;
static constexpr int WORD_FILTER_REQUEST_ID_OFFSET = 0x02000000;
static constexpr std::size_t WORD_FILTER_COMMENT_MAX_LENGTH = 0x400;
static constexpr std::uint64_t ASYNC_REQUEST_PARAMETER_SIZE = 0x18;
static constexpr std::uint32_t RESOLVE_TIMEOUT_MIN = 1000000;
static constexpr std::uint32_t TRANSFER_TIMEOUT_MIN = 10000000;

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
    std::condition_variable finished;
    std::unordered_set<int> titleContexts;
    std::unordered_map<int, Request> requests;
    int nextTitleContext;
    int nextRequest;
};

Service lookup(1, 1);
Service wordFilter(WORD_FILTER_TITLE_CTX_ID_OFFSET + 1, WORD_FILTER_REQUEST_ID_OFFSET + 1);

int AddTitleContext(Service& service) {
    std::lock_guard lock(service.mutex);
    if (service.titleContexts.size() >= MAX_TITLE_CONTEXTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    const int titleCtxId = service.nextTitleContext++;
    service.titleContexts.insert(titleCtxId);
    return titleCtxId;
}

int CreateTitleContext(Service& service, const void* selfNpId) {
    if (!selfNpId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return AddTitleContext(service);
}

int DeleteTitleContext(Service& service, int titleCtxId) {
    std::lock_guard lock(service.mutex);
    if (!service.titleContexts.erase(titleCtxId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    std::erase_if(service.requests, [titleCtxId](const auto& entry) { return entry.second.titleCtxId == titleCtxId; });
    service.finished.notify_all();
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
    if (!service.requests.erase(reqId)) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    service.finished.notify_all();
    return 0;
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
    service.finished.notify_all();
    return request->second.async ? 0 : SCE_NP_ERROR_SIGNED_OUT;
}

int AbortRequest(Service& service, int reqId) {
    std::lock_guard lock(service.mutex);
    const auto request = service.requests.find(reqId);
    if (request == service.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    request->second.aborted = true;
    if (!request->second.finished) {
        request->second.finished = true;
        request->second.result = SCE_NP_COMMUNITY_ERROR_ABORTED;
        service.finished.notify_all();
    }
    return 0;
}

int PollAsync(Service& service, int reqId, int* result, int synchronousError) {
    if (!result) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(service.mutex);
    const auto request = service.requests.find(reqId);
    if (request == service.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.async) return synchronousError;
    if (!request->second.finished) return POLL_RUNNING;
    *result = request->second.result;
    return POLL_FINISHED;
}

int WaitAsync(Service& service, int reqId, int* result, int synchronousError) {
    if (!result) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::unique_lock lock(service.mutex);
    auto request = service.requests.find(reqId);
    if (request == service.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.async) return synchronousError;
    service.finished.wait(lock, [&] {
        request = service.requests.find(reqId);
        return request == service.requests.end() || request->second.finished;
    });
    if (request == service.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    *result = request->second.result;
    return 0;
}

bool ValidTimeout(std::uint32_t timeout, std::uint32_t minimum) {
    return timeout == 0 || timeout >= minimum;
}

int SetTimeout(Service& service, int id, int resolveRetry, std::uint32_t resolveTimeout, std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout) {
    if (resolveRetry < 0 || !ValidTimeout(resolveTimeout, RESOLVE_TIMEOUT_MIN) || !ValidTimeout(connTimeout, TRANSFER_TIMEOUT_MIN) ||
        !ValidTimeout(sendTimeout, TRANSFER_TIMEOUT_MIN) || !ValidTimeout(recvTimeout, TRANSFER_TIMEOUT_MIN))
        return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (resolveRetry == 0 && resolveTimeout == 0 && connTimeout == 0 && sendTimeout == 0 && recvTimeout == 0) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(service.mutex);
    return service.titleContexts.contains(id) || service.requests.contains(id) ? 0 : SCE_NP_COMMUNITY_ERROR_INVALID_ID;
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

int APS5_VABI sceNpAppInfoIntAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailability() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailabilityA() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailabilityAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailabilityAllA() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailability() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityA() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityAllA() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntDestroyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntDestroyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntGetCompatibleTitleIdList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntGetCompatibleTitleIdNum() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntDestroyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntGetCompatibleTitleIdList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntGetCompatibleTitleIdNum() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpBandwidthTestShutdownWithDetailedInfo() {
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}
int APS5_VABI sceNpLookupCreateTitleCtxA(int userId) {
    (void)userId;
    return AddTitleContext(lookup);
}
int APS5_VABI sceNpLookupNetAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetCensorComment() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetConvertJidToNpId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetConvertNpIdToJid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetCreateTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetDeleteRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetDeleteTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetInitWithFunctionPointer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetInitWithMemoryPool() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetIsInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetNpId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetSanitizeComment() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetSetTimeout() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntDestroyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntGetServiceAvailability() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntGetServiceAvailabilityA() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntIsSetServiceType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntDestroyRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntGetAvailability() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntGetAvailabilityList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntIsCached() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpTitleMetadataIntAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpTitleMetadataIntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpTitleMetadataIntDeleteRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpTitleMetadataIntGetInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpTitleMetadataIntGetNpTitleId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpUtilityInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpUtilityTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpWordFilterCreateTitleCtxA(int userId) {
    (void)userId;
    return AddTitleContext(wordFilter);
}
}

extern "C" {

int APS5_VABI sceNpBandwidthTestInitStart() {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpLookupAbortRequest(int reqId) {
    return AbortRequest(lookup, reqId);
}

int APS5_VABI sceNpLookupCreateAsyncRequest(int titleCtxId, const std::uint64_t* param) {
    if (!param || *param != ASYNC_REQUEST_PARAMETER_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    return CreateRequest(lookup, titleCtxId, true);
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

int APS5_VABI sceNpLookupPollAsync(int reqId, int* result) {
    return PollAsync(lookup, reqId, result, SCE_NP_LOOKUP_ERROR_SYNCHRONOUS_REQUEST);
}

int APS5_VABI sceNpLookupSetTimeout(int id, int resolveRetry, std::uint32_t resolveTimeout, std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout) {
    return SetTimeout(lookup, id, resolveRetry, resolveTimeout, connTimeout, sendTimeout, recvTimeout);
}

int APS5_VABI sceNpLookupWaitAsync(int reqId, int* result) {
    return WaitAsync(lookup, reqId, result, SCE_NP_LOOKUP_ERROR_SYNCHRONOUS_REQUEST);
}

int APS5_VABI sceNpWordFilterAbortRequest(int reqId) {
    return AbortRequest(wordFilter, reqId);
}

int APS5_VABI sceNpWordFilterCensorComment(int reqId, const char* comment, void* option) {
    if (!comment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckComment(comment)) return error;
    return StartOffline(wordFilter, reqId);
}

int APS5_VABI sceNpWordFilterCreateAsyncRequest(int titleCtxId, const std::uint64_t* param) {
    if (!param || *param != ASYNC_REQUEST_PARAMETER_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
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
    return PollAsync(wordFilter, reqId, result, SCE_NP_COMMUNITY_ERROR_INVALID_ID);
}

int APS5_VABI sceNpWordFilterSanitizeComment(int reqId, const char* comment, char* sanitizedComment, void* option) {
    if (!comment || !sanitizedComment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckComment(comment)) return error;
    return StartOffline(wordFilter, reqId);
}

int APS5_VABI sceNpWordFilterSetTimeout(int id, int resolveRetry, std::uint32_t resolveTimeout, std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout) {
    return SetTimeout(wordFilter, id, resolveRetry, resolveTimeout, connTimeout, sendTimeout, recvTimeout);
}

int APS5_VABI sceNpWordFilterWaitAsync(int reqId, int* result) {
    return WaitAsync(wordFilter, reqId, result, SCE_NP_COMMUNITY_ERROR_INVALID_ID);
}

}
