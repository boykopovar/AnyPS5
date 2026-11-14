#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpUtility/include/InventoryExports.hpp"
#include "prx/libSceNpUtility/include/Api.hpp"
#include <cerrno>
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
static constexpr int SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED = static_cast<int>(0x80550702);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550704);
static constexpr int SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS = static_cast<int>(0x80550706);
static constexpr int SCE_NP_COMMUNITY_ERROR_ABORTED = static_cast<int>(0x80550707);
static constexpr int SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT = static_cast<int>(0x8055070C);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_ID = static_cast<int>(0x8055070E);
static constexpr int SCE_NP_COMMUNITY_ERROR_INVALID_TYPE = static_cast<int>(0x80550711);
static constexpr int SCE_NP_LOOKUP_ERROR_SYNCHRONOUS_REQUEST = static_cast<int>(0x80550307);
static constexpr int SCE_NP_SERVICE_CHECKER2_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055B301);
static constexpr int SCE_NP_SERVICE_CHECKER2_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055B302);
static constexpr int AppInfoInvalidArgument = static_cast<int>(0x8055B204);
static constexpr int TitleMetadataInvalidArgument = static_cast<int>(0x8055AA03);
static constexpr int SCE_NP_SERVICE_CHECKER2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055B305);
static constexpr int SCE_NP_SERVICE_CHECKER2_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x8055B304);
static constexpr int SCE_NP_APP_INFO_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055B201);
static constexpr int SCE_NP_APP_INFO_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055B202);
static constexpr int SCE_NP_APP_INFO_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055B204);
static constexpr int SCE_NP_APP_LAUNCH_LINK_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055AE01);
static constexpr int SCE_NP_APP_LAUNCH_LINK_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055AE02);
static constexpr int SCE_NP_APP_LAUNCH_LINK_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x8055AE06);
static constexpr int SCE_NP_APP_LAUNCH_LINK_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055AE0C);
static constexpr int SCE_NP_APP_LAUNCH_LINK2_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055C601);
static constexpr int SCE_NP_APP_LAUNCH_LINK2_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055C602);
static constexpr int SCE_NP_APP_LAUNCH_LINK2_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x8055C606);
static constexpr int SCE_NP_APP_LAUNCH_LINK2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055C60C);
static constexpr int SCE_NP_SERVICE_CHECKER_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055AD01);
static constexpr int SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055AD02);
static constexpr int SCE_NP_SERVICE_CHECKER_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x8055AD06);
static constexpr int SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055AD0B);
static constexpr int SCE_NP_TITLE_METADATA_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055AA03);
static constexpr int SCE_NP_TITLE_METADATA_ERROR_TOO_LONG = static_cast<int>(0x8055AA04);
static constexpr int POLL_FINISHED = 0;
static constexpr int POLL_RUNNING = 1;
static constexpr std::size_t MAX_TITLE_CONTEXTS = 32;
static constexpr std::size_t MAX_REQUESTS = 32;
static constexpr std::size_t MAX_SERVICE_CHECKER2_REQUESTS = 0x1000;
static constexpr std::uint32_t MAX_SERVICE_CHECKER2_REQUEST_ID = 0x7FFFFFFF;
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
Service lookupNet(1, 1);
Service wordFilter(WORD_FILTER_TITLE_CTX_ID_OFFSET + 1, WORD_FILTER_REQUEST_ID_OFFSET + 1);

std::mutex lookupNetStateMutex;
bool lookupNetInitialized = false;
int lookupNetRefCount = 0;

bool LookupNetIsInitialized() {
    std::lock_guard lock(lookupNetStateMutex);
    return lookupNetInitialized;
}

void LookupNetReset() {
    std::lock_guard lock(lookupNet.mutex);
    lookupNet.titleContexts.clear();
    lookupNet.requests.clear();
}

struct ServiceChecker2 {
    std::mutex mutex;
    bool initialized = false;
    std::unordered_set<int> requests;
    std::uint32_t nextRequest = 1;
};
ServiceChecker2 serviceChecker2;

struct FamilyState {
    std::mutex mutex;
    bool initialized = false;
    std::unordered_set<int> requests;
    std::int32_t nextRequest = 1;
    std::int32_t currentRequest = 0;
};

int AppLaunchLinkCreateRequest(FamilyState& state, std::int32_t userId, const void* param, std::int32_t* requestId, int notInitialized, int invalidArgument) {
    if (userId == -1 || !param || !requestId) return invalidArgument;
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return notInitialized;
    const std::int32_t id = state.nextRequest++;
    state.requests.insert(id);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(requestId, &id, sizeof(id));
    errno = savedErrno;
    if (!copied) {
        state.requests.erase(id);
        --state.nextRequest;
        return invalidArgument;
    }
    return 0;
}

int AppLaunchLinkDestroyRequest(FamilyState& state, std::int32_t requestId, int notInitialized) {
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return notInitialized;
    state.requests.erase(requestId);
    return 0;
}

int AppLaunchLinkAbortRequest(FamilyState& state, std::int32_t requestId, int notInitialized, int requestNotFound) {
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return notInitialized;
    if (!state.requests.erase(requestId)) return requestNotFound;
    return 0;
}

int AppLaunchLinkGetCompatibleTitleIdNum(FamilyState& state, std::int32_t requestId, std::int32_t* titleIdNum, int notInitialized, int invalidArgument, int requestNotFound) {
    if (!titleIdNum) return invalidArgument;
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return notInitialized;
    if (!state.requests.contains(requestId)) return requestNotFound;
    const std::int32_t zero = 0;
    if (!GuestCopyOut_nid_no_patch(titleIdNum, &zero, sizeof(zero))) return invalidArgument;
    return 0;
}

int AppLaunchLinkGetCompatibleTitleIdList(FamilyState& state, std::int32_t requestId, void* titleIdList, std::int32_t* titleIdNum, int notInitialized, int invalidArgument, int requestNotFound) {
    if (!titleIdList && titleIdNum) return invalidArgument;
    std::lock_guard lock(state.mutex);
    if (!state.initialized) return notInitialized;
    if (!state.requests.contains(requestId)) return requestNotFound;
    if (titleIdNum) {
        const std::int32_t zero = 0;
        if (!GuestCopyOut_nid_no_patch(titleIdNum, &zero, sizeof(zero))) return invalidArgument;
    }
    return 0;
}
FamilyState appInfo;
FamilyState appLaunchLink;
FamilyState appLaunchLink2;

struct ServiceChecker1 {
    std::mutex mutex;
    bool initialized = false;
    std::unordered_set<int> requests;
    std::int32_t nextRequest = 1;
};
ServiceChecker1 serviceChecker1;

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

bool CopyOutInt(int* dst, int value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(dst, &value, sizeof(value));
    errno = savedErrno;
    return copied;
}

int PollAsync(Service& service, int reqId, int* result, int synchronousError) {
    if (!result) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(service.mutex);
    const auto request = service.requests.find(reqId);
    if (request == service.requests.end()) return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
    if (!request->second.async) return synchronousError;
    if (!request->second.finished) return POLL_RUNNING;
    if (!CopyOutInt(result, request->second.result)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
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
    if (!CopyOutInt(result, request->second.result)) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
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

int APS5_VABI sceNpAdvppBandwidthDownloadBegin() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpAdvppBandwidthDownloadEnd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpBandwidthTestInitStartDownloadInternal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpBandwidthTestInitStartUploadInternal() {
    NotImplemented_nid_no_patch(__func__);
}


int SceNpUtilityModuleLoaded_nid_no_patch = 1;

int APS5_VABI sceNpBandwidthTestAbort(int context_id) {
    (void)context_id;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpBandwidthTestGetStatus(int context_id, int* status) {
    (void)context_id;
    if (status && !CopyOutInt(status, 0)) return SCE_NP_ERROR_INVALID_ARGUMENT;
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

int APS5_VABI sceNpAppInfoIntAbortRequest(void) {
    std::lock_guard lock(appInfo.mutex);
    if (!appInfo.initialized) return SCE_NP_APP_INFO_ERROR_NOT_INITIALIZED;
    if (appInfo.currentRequest == 0) return SCE_NP_APP_INFO_ERROR_INVALID_ARGUMENT;
    appInfo.currentRequest = 0;
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailability(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, void* argument4, void* argument5) {
    if (argument2 == nullptr || argument4 == nullptr || argument5 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailabilityA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, void* argument4, void* argument5) {
    if (argument4 == nullptr || argument5 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailabilityAll(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, void* argument4, std::uint64_t argument5, void* argument6) {
    if (argument2 == nullptr || (argument4 == nullptr && argument5 != 0) || argument6 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckAvailabilityAllA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, void* argument4, std::uint64_t argument5, void* argument6) {
    if ((argument4 == nullptr && argument5 != 0) || argument6 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailability(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, void* argument6) {
    if (argument2 == nullptr || argument5 == nullptr || argument6 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, void* argument6) {
    if (argument5 == nullptr || argument6 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityAll(std::uint32_t argument1, const void* argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, std::uint64_t argument6, void* argument7) {
    if (argument2 == nullptr || (argument5 == nullptr && argument6 != 0) || argument7 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCheckServiceAvailabilityAllA(std::uint32_t argument1, std::uint32_t argument2, std::uint32_t argument3, std::uint32_t argument4, void* argument5, std::uint64_t argument6, void* argument7) {
    if ((argument5 == nullptr && argument6 != 0) || argument7 == nullptr) return AppInfoInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpAppInfoIntCreateRequest(std::int32_t* requestId) {
    if (!requestId) return SCE_NP_APP_INFO_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(appInfo.mutex);
    if (!appInfo.initialized) return SCE_NP_APP_INFO_ERROR_NOT_INITIALIZED;
    const std::int32_t id = appInfo.nextRequest++;
    const std::int32_t previousRequest = appInfo.currentRequest;
    appInfo.currentRequest = id;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(requestId, &id, sizeof(id));
    errno = savedErrno;
    if (!copied) {
        appInfo.currentRequest = previousRequest;
        --appInfo.nextRequest;
        return SCE_NP_APP_INFO_ERROR_INVALID_ARGUMENT;
    }
    return 0;
}
int APS5_VABI sceNpAppInfoIntDestroyRequest(void) {
    std::lock_guard lock(appInfo.mutex);
    if (!appInfo.initialized) return SCE_NP_APP_INFO_ERROR_NOT_INITIALIZED;
    appInfo.currentRequest = 0;
    appInfo.requests.clear();
    return 0;
}
void APS5_VABI sceNpAppInfoIntFinalize(void) {
    std::lock_guard lock(appInfo.mutex);
    appInfo.initialized = false;
    appInfo.requests.clear();
    appInfo.currentRequest = 0;
    appInfo.nextRequest = 1;
}
int APS5_VABI sceNpAppInfoIntInitialize(std::int32_t userId) {
    (void)userId;
    std::lock_guard lock(appInfo.mutex);
    if (appInfo.initialized) return SCE_NP_APP_INFO_ERROR_ALREADY_INITIALIZED;
    appInfo.initialized = true;
    return 0;
}
int APS5_VABI sceNpAppLaunchLink2IntAbortRequest(std::int32_t requestId) {
    return AppLaunchLinkAbortRequest(appLaunchLink2, requestId, SCE_NP_APP_LAUNCH_LINK2_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK2_ERROR_REQUEST_NOT_FOUND);
}
int APS5_VABI sceNpAppLaunchLink2IntCreateRequest(std::int32_t userId, const void* param, std::int32_t* requestId) {
    return AppLaunchLinkCreateRequest(appLaunchLink2, userId, param, requestId, SCE_NP_APP_LAUNCH_LINK2_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK2_ERROR_INVALID_ARGUMENT);
}
int APS5_VABI sceNpAppLaunchLink2IntDestroyRequest(std::int32_t requestId) {
    return AppLaunchLinkDestroyRequest(appLaunchLink2, requestId, SCE_NP_APP_LAUNCH_LINK2_ERROR_NOT_INITIALIZED);
}
void APS5_VABI sceNpAppLaunchLink2IntFinalize(void) {
    std::lock_guard lock(appLaunchLink2.mutex);
    appLaunchLink2.initialized = false;
    appLaunchLink2.requests.clear();
    appLaunchLink2.nextRequest = 1;
}
int APS5_VABI sceNpAppLaunchLink2IntGetCompatibleTitleIdList(std::int32_t requestId, void* titleIdList, std::int32_t* titleIdNum) {
    return AppLaunchLinkGetCompatibleTitleIdList(appLaunchLink2, requestId, titleIdList, titleIdNum, SCE_NP_APP_LAUNCH_LINK2_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK2_ERROR_INVALID_ARGUMENT, SCE_NP_APP_LAUNCH_LINK2_ERROR_REQUEST_NOT_FOUND);
}
int APS5_VABI sceNpAppLaunchLink2IntGetCompatibleTitleIdNum(std::int32_t requestId, std::int32_t* titleIdNum) {
    return AppLaunchLinkGetCompatibleTitleIdNum(appLaunchLink2, requestId, titleIdNum, SCE_NP_APP_LAUNCH_LINK2_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK2_ERROR_INVALID_ARGUMENT, SCE_NP_APP_LAUNCH_LINK2_ERROR_REQUEST_NOT_FOUND);
}
int APS5_VABI sceNpAppLaunchLink2IntInitialize(std::int32_t userId) {
    (void)userId;
    std::lock_guard lock(appLaunchLink2.mutex);
    if (appLaunchLink2.initialized) return SCE_NP_APP_LAUNCH_LINK2_ERROR_ALREADY_INITIALIZED;
    appLaunchLink2.initialized = true;
    return 0;
}
int APS5_VABI sceNpAppLaunchLinkIntAbortRequest(std::int32_t requestId) {
    return AppLaunchLinkAbortRequest(appLaunchLink, requestId, SCE_NP_APP_LAUNCH_LINK_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK_ERROR_REQUEST_NOT_FOUND);
}
int APS5_VABI sceNpAppLaunchLinkIntCreateRequest(std::int32_t userId, const void* param, std::int32_t* requestId) {
    return AppLaunchLinkCreateRequest(appLaunchLink, userId, param, requestId, SCE_NP_APP_LAUNCH_LINK_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK_ERROR_INVALID_ARGUMENT);
}
int APS5_VABI sceNpAppLaunchLinkIntDestroyRequest(std::int32_t requestId) {
    return AppLaunchLinkDestroyRequest(appLaunchLink, requestId, SCE_NP_APP_LAUNCH_LINK_ERROR_NOT_INITIALIZED);
}
void APS5_VABI sceNpAppLaunchLinkIntFinalize(void) {
    std::lock_guard lock(appLaunchLink.mutex);
    appLaunchLink.initialized = false;
    appLaunchLink.requests.clear();
    appLaunchLink.nextRequest = 1;
}
int APS5_VABI sceNpAppLaunchLinkIntGetCompatibleTitleIdList(std::int32_t requestId, void* titleIdList, std::int32_t* titleIdNum) {
    return AppLaunchLinkGetCompatibleTitleIdList(appLaunchLink, requestId, titleIdList, titleIdNum, SCE_NP_APP_LAUNCH_LINK_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK_ERROR_INVALID_ARGUMENT, SCE_NP_APP_LAUNCH_LINK_ERROR_REQUEST_NOT_FOUND);
}
int APS5_VABI sceNpAppLaunchLinkIntGetCompatibleTitleIdNum(std::int32_t requestId, std::int32_t* titleIdNum) {
    return AppLaunchLinkGetCompatibleTitleIdNum(appLaunchLink, requestId, titleIdNum, SCE_NP_APP_LAUNCH_LINK_ERROR_NOT_INITIALIZED, SCE_NP_APP_LAUNCH_LINK_ERROR_INVALID_ARGUMENT, SCE_NP_APP_LAUNCH_LINK_ERROR_REQUEST_NOT_FOUND);
}
int APS5_VABI sceNpAppLaunchLinkIntInitialize(std::int32_t userId) {
    (void)userId;
    std::lock_guard lock(appLaunchLink.mutex);
    if (appLaunchLink.initialized) return SCE_NP_APP_LAUNCH_LINK_ERROR_ALREADY_INITIALIZED;
    appLaunchLink.initialized = true;
    return 0;
}
int APS5_VABI sceNpBandwidthTestShutdownWithDetailedInfo() {
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}
int APS5_VABI sceNpLookupCreateTitleCtxA(int userId) {
    (void)userId;
    return AddTitleContext(lookup);
}
int APS5_VABI sceNpLookupNetAbortRequest(std::int32_t requestId) {
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    return AbortRequest(lookupNet, requestId);
}
int APS5_VABI sceNpLookupNetCensorComment(std::int32_t requestId, const void* npId, const char* comment, const void* option) {
    if (!comment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckComment(comment)) return error;
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    if (!npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return StartOffline(lookupNet, requestId);
}
int APS5_VABI sceNpLookupNetConvertJidToNpId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetConvertNpIdToJid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpLookupNetCreateRequest(std::int32_t titleCtxId) {
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    return CreateRequest(lookupNet, titleCtxId, false);
}
int APS5_VABI sceNpLookupNetCreateTitleCtx(const void* selfNpId, const void* npId) {
    (void)selfNpId;
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    if (!npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return AddTitleContext(lookupNet);
}
int APS5_VABI sceNpLookupNetDeleteRequest(std::int32_t requestId) {
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    return DeleteRequest(lookupNet, requestId);
}
int APS5_VABI sceNpLookupNetDeleteTitleCtx(std::int32_t titleCtxId) {
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    return DeleteTitleContext(lookupNet, titleCtxId);
}
int APS5_VABI sceNpLookupNetInit(void) {
    std::lock_guard lock(lookupNetStateMutex);
    ++lookupNetRefCount;
    lookupNetInitialized = true;
    return 0;
}
int APS5_VABI sceNpLookupNetInitWithFunctionPointer(const void* param, const void* functionPointer) {
    (void)param;
    (void)functionPointer;
    return sceNpLookupNetInit();
}
int APS5_VABI sceNpLookupNetInitWithMemoryPool(const void* param, const void* pool) {
    (void)param;
    (void)pool;
    return sceNpLookupNetInit();
}
int APS5_VABI sceNpLookupNetIsInit(std::int32_t* out) {
    const std::int32_t value = LookupNetIsInitialized() ? 1 : 0;
    if (out && !GuestCopyOut_nid_no_patch(out, &value, sizeof(value))) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 0;
}
int APS5_VABI sceNpLookupNetNpId(std::int32_t requestId, const void* onlineId, void* npId, const void* option) {
    (void)option;
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    if (!onlineId || !npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return StartOffline(lookupNet, requestId);
}
int APS5_VABI sceNpLookupNetSanitizeComment(std::int32_t requestId, const void* npId, const char* comment, char* sanitizedComment, const void* option) {
    if (!comment || !sanitizedComment) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    if (option) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    if (const int error = CheckComment(comment)) return error;
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    if (!npId) return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    return StartOffline(lookupNet, requestId);
}
int APS5_VABI sceNpLookupNetSetTimeout(std::int32_t id, std::int32_t resolveRetry, std::int32_t resolveTimeout, std::int32_t connectTimeout, std::int32_t sendTimeout, std::int32_t receiveTimeout) {
    if (!LookupNetIsInitialized()) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
    return SetTimeout(lookupNet, id, resolveRetry, static_cast<std::uint32_t>(resolveTimeout), static_cast<std::uint32_t>(connectTimeout), static_cast<std::uint32_t>(sendTimeout), static_cast<std::uint32_t>(receiveTimeout));
}
int APS5_VABI sceNpLookupNetTerm(void) {
    {
        std::lock_guard lock(lookupNetStateMutex);
        if (!lookupNetInitialized) return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
        if (--lookupNetRefCount > 0) return 0;
        lookupNetInitialized = false;
    }
    LookupNetReset();
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntAbortRequest(int requestId) {
    std::lock_guard lock(serviceChecker2.mutex);
    if (!serviceChecker2.initialized) return SCE_NP_SERVICE_CHECKER2_ERROR_NOT_INITIALIZED;
    if (!serviceChecker2.requests.contains(requestId)) return SCE_NP_SERVICE_CHECKER2_ERROR_REQUEST_NOT_FOUND;
    serviceChecker2.requests.erase(requestId);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntCreateRequest(int* requestId) {
    std::lock_guard lock(serviceChecker2.mutex);
    if (!serviceChecker2.initialized) return SCE_NP_SERVICE_CHECKER2_ERROR_NOT_INITIALIZED;
    if (!requestId) return SCE_NP_SERVICE_CHECKER2_ERROR_INVALID_ARGUMENT;
    if (serviceChecker2.requests.size() >= MAX_SERVICE_CHECKER2_REQUESTS) return SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    std::uint32_t id = serviceChecker2.nextRequest;
    while (serviceChecker2.requests.contains(static_cast<int>(id))) id = id >= MAX_SERVICE_CHECKER2_REQUEST_ID ? 1 : id + 1;
    const int value = static_cast<int>(id);
    if (!GuestCopyOut_nid_no_patch(requestId, &value, sizeof(value))) return SCE_NP_SERVICE_CHECKER2_ERROR_INVALID_ARGUMENT;
    serviceChecker2.nextRequest = id >= MAX_SERVICE_CHECKER2_REQUEST_ID ? 1 : id + 1;
    serviceChecker2.requests.insert(value);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntDestroyRequest(int requestId) {
    std::lock_guard lock(serviceChecker2.mutex);
    if (!serviceChecker2.initialized) return SCE_NP_SERVICE_CHECKER2_ERROR_NOT_INITIALIZED;
    serviceChecker2.requests.erase(requestId);
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntFinalize() {
    std::lock_guard lock(serviceChecker2.mutex);
    serviceChecker2.requests.clear();
    serviceChecker2.initialized = false;
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntGetServiceAvailability(int requestId, const void* npId, void* result) {
    std::lock_guard lock(serviceChecker2.mutex);
    if (!serviceChecker2.initialized) return SCE_NP_SERVICE_CHECKER2_ERROR_NOT_INITIALIZED;
    if (!npId || !result) return SCE_NP_SERVICE_CHECKER2_ERROR_INVALID_ARGUMENT;
    if (!serviceChecker2.requests.contains(requestId)) return SCE_NP_SERVICE_CHECKER2_ERROR_REQUEST_NOT_FOUND;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpServiceChecker2IntGetServiceAvailabilityA(int requestId, int userId, void* result) {
    std::lock_guard lock(serviceChecker2.mutex);
    if (!serviceChecker2.initialized) return SCE_NP_SERVICE_CHECKER2_ERROR_NOT_INITIALIZED;
    if (userId == -1 || !result) return SCE_NP_SERVICE_CHECKER2_ERROR_INVALID_ARGUMENT;
    if (!serviceChecker2.requests.contains(requestId)) return SCE_NP_SERVICE_CHECKER2_ERROR_REQUEST_NOT_FOUND;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpServiceChecker2IntInitialize() {
    std::lock_guard lock(serviceChecker2.mutex);
    if (serviceChecker2.initialized) return SCE_NP_SERVICE_CHECKER2_ERROR_ALREADY_INITIALIZED;
    serviceChecker2.requests.clear();
    serviceChecker2.nextRequest = 1;
    serviceChecker2.initialized = true;
    return 0;
}
int APS5_VABI sceNpServiceChecker2IntIsSetServiceType(const std::uint32_t* serviceTypes, std::uint32_t serviceType) {
    if (!serviceTypes || serviceType > 9) return 0;
    std::uint32_t word = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&word, &serviceTypes[serviceType >> 5], sizeof(word));
    errno = savedErrno;
    if (!copied) return 0;
    return static_cast<int>((word >> serviceType) & 1);
}
int APS5_VABI sceNpServiceCheckerIntAbortRequest(std::int32_t requestId) {
    std::lock_guard lock(serviceChecker1.mutex);
    if (!serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED;
    if (serviceChecker1.requests.erase(requestId) == 0) return SCE_NP_SERVICE_CHECKER_ERROR_REQUEST_NOT_FOUND;
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntCreateRequest(const void* npId, std::int32_t* requestId) {
    if (!npId || !requestId) return SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(serviceChecker1.mutex);
    if (!serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED;
    const std::int32_t id = serviceChecker1.nextRequest++;
    serviceChecker1.requests.insert(id);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(requestId, &id, sizeof(id));
    errno = savedErrno;
    if (!copied) {
        serviceChecker1.requests.erase(id);
        --serviceChecker1.nextRequest;
        return SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT;
    }
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntDestroyRequest(std::int32_t requestId) {
    std::lock_guard lock(serviceChecker1.mutex);
    if (!serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED;
    serviceChecker1.requests.erase(requestId);
    return 0;
}
void APS5_VABI sceNpServiceCheckerIntFinalize(void) {
    std::lock_guard lock(serviceChecker1.mutex);
    serviceChecker1.initialized = false;
    serviceChecker1.requests.clear();
    serviceChecker1.nextRequest = 1;
}
int APS5_VABI sceNpServiceCheckerIntGetAvailability(std::int32_t requestId, const void* npId, void* result) {
    if (!npId || !result) return SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(serviceChecker1.mutex);
    if (!serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED;
    if (!serviceChecker1.requests.contains(requestId)) return SCE_NP_SERVICE_CHECKER_ERROR_REQUEST_NOT_FOUND;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpServiceCheckerIntGetAvailabilityList(std::int32_t requestId, const void* const* npIds, std::size_t count, void* results) {
    if ((!npIds || !results) && count != 0) return SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(serviceChecker1.mutex);
    if (!serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED;
    if (!serviceChecker1.requests.contains(requestId)) return SCE_NP_SERVICE_CHECKER_ERROR_REQUEST_NOT_FOUND;
    if (count == 0) return 0;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpServiceCheckerIntInitialize(std::int32_t userId) {
    (void)userId;
    std::lock_guard lock(serviceChecker1.mutex);
    if (serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_ALREADY_INITIALIZED;
    serviceChecker1.initialized = true;
    return 0;
}
int APS5_VABI sceNpServiceCheckerIntIsCached(std::int32_t requestId, std::uint8_t* cached) {
    if (!cached) return SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(serviceChecker1.mutex);
    if (!serviceChecker1.initialized) return SCE_NP_SERVICE_CHECKER_ERROR_NOT_INITIALIZED;
    if (!serviceChecker1.requests.contains(requestId)) return SCE_NP_SERVICE_CHECKER_ERROR_REQUEST_NOT_FOUND;
    const std::uint8_t value = 0;
    if (!GuestCopyOut_nid_no_patch(cached, &value, sizeof(value))) return SCE_NP_SERVICE_CHECKER_ERROR_INVALID_ARGUMENT;
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
int APS5_VABI sceNpTitleMetadataIntGetInfo(std::uint32_t argument1, void* argument2, void* argument3) {
    if (argument2 == nullptr || argument3 == nullptr) return TitleMetadataInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceNpTitleMetadataIntGetNpTitleId(const char* titleId, std::size_t titleIdLength, char* npTitleId) {
    if (!titleId || !npTitleId) return SCE_NP_TITLE_METADATA_ERROR_INVALID_ARGUMENT;
    if (titleIdLength > 9) return SCE_NP_TITLE_METADATA_ERROR_TOO_LONG;
    char buffer[16] = {};
    if (!GuestCopyIn_nid_no_patch(buffer, titleId, titleIdLength)) return SCE_NP_TITLE_METADATA_ERROR_INVALID_ARGUMENT;
    const std::size_t length = strnlen(buffer, sizeof(buffer));
    std::memcpy(buffer + length, "_00", 4);
    if (!GuestCopyOut_nid_no_patch(npTitleId, buffer, sizeof(buffer))) return SCE_NP_TITLE_METADATA_ERROR_INVALID_ARGUMENT;
    return 0;
}
int APS5_VABI sceNpUtilityInit() {
    return 0;
}
int APS5_VABI sceNpUtilityTerm() {
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
    if (!param) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::uint64_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, param, sizeof(value));
    errno = savedErrno;
    if (!copied || value != ASYNC_REQUEST_PARAMETER_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
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
    if (!param) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    std::uint64_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, param, sizeof(value));
    errno = savedErrno;
    if (!copied || value != ASYNC_REQUEST_PARAMETER_SIZE) return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
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

extern "C" {

int APS5_VABI libSceNpUtilityUnknown_3BZG8C8D62Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3BZG8C8D62Q", libSceNpUtilityUnknown_3BZG8C8D62Q_nid_no_patch);

int APS5_VABI libSceNpUtilityUnknown_VVM_plus_zY9FiEU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VVM+zY9FiEU", libSceNpUtilityUnknown_VVM_plus_zY9FiEU_nid_no_patch);

}
