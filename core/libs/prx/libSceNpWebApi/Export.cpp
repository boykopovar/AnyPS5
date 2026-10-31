#include "prx/libSceNpWebApi/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

static constexpr int SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552902);
static constexpr int SCE_NP_WEBAPI_ERROR_INVALID_LIB_CONTEXT_ID = static_cast<int>(0x80552903);
static constexpr int SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND = static_cast<int>(0x80552904);
static constexpr int SCE_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND = static_cast<int>(0x80552905);
static constexpr int SCE_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80552906);
static constexpr int SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN = static_cast<int>(0x80552907);
static constexpr int SCE_NP_WEBAPI_ERROR_INVALID_CONTENT_PARAMETER = static_cast<int>(0x80552908);
static constexpr int SCE_NP_WEBAPI_ERROR_ABORTED = static_cast<int>(0x80552909);
static constexpr int SCE_NP_WEBAPI_ERROR_USER_CONTEXT_ALREADY_EXIST = static_cast<int>(0x8055290A);
static constexpr int SCE_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND = static_cast<int>(0x8055290B);
static constexpr int SCE_NP_WEBAPI_ERROR_PUSH_EVENT_CALLBACK_NOT_FOUND = static_cast<int>(0x8055290C);
static constexpr int SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND = static_cast<int>(0x8055290D);
static constexpr int SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND = static_cast<int>(0x8055290E);
static constexpr int SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_CALLBACK_NOT_FOUND = static_cast<int>(0x8055290F);
static constexpr int SCE_NP_WEBAPI_ERROR_SIGNED_IN_USER_NOT_FOUND = static_cast<int>(0x80552910);
static constexpr int SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_MAX = static_cast<int>(0x8055291A);

static constexpr int SCE_NP_WEBAPI_HTTP_METHOD_DELETE = 3;
static constexpr std::uint32_t SCE_NP_INVALID_SERVICE_LABEL = 0xFFFFFFFF;
static constexpr std::uint32_t SDK_VERSION_1_00 = 0x01000000;
static constexpr std::uint32_t SDK_VERSION_2_00 = 0x02000000;
static constexpr std::uint32_t SDK_VERSION_2_50 = 0x02500000;
static constexpr std::uint32_t SDK_VERSION_3_00 = 0x03000000;
static constexpr std::uint64_t TIMEOUT_SCAN_PERIOD = 1000;

namespace {

struct Request {
    bool aborted;
    bool multipart;
    bool expired;
    std::uint32_t timeout;
    std::uint64_t endTime;
};

struct UserContext {
    int userId;
    std::unordered_map<std::int64_t, Request> requests;
    std::unordered_set<int> pushEventCallbacks;
    std::unordered_set<int> servicePushEventCallbacks;
};

struct LibraryContext {
    int libHttpCtxId;
    std::size_t poolSize;
    std::unordered_set<int> handles;
    std::unordered_set<int> timeoutCapableHandles;
    std::unordered_set<int> pushEventFilters;
    std::unordered_map<int, UserContext> userContexts;
};

struct UserContextLookup {
    int error;
    LibraryContext* library;
    UserContext* user;
};

struct RequestLookup {
    int error;
    UserContext* user;
    Request* request;
};

std::mutex contextsMutex;
std::unordered_map<int, LibraryContext> contexts;
int lastContext = 0;
int lastObject = 0;
int lastUserContext = 0;
std::int64_t lastRequest = 0;
std::uint64_t lastTimeoutScan = 0;

int nextObject() {
    lastObject = lastObject % 0xefffffff + 1;
    return lastObject;
}

std::uint32_t compiledSdkVersion() {
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, process + 16, sizeof(version));
    return version;
}

UserContextLookup findUserContext(int titleUserCtxId) {
    const auto library = contexts.find(titleUserCtxId >> 16);
    if (library == contexts.end()) return {SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND, nullptr, nullptr};
    const auto user = library->second.userContexts.find(titleUserCtxId);
    if (user == library->second.userContexts.end()) return {SCE_NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND, &library->second, nullptr};
    return {0, &library->second, &user->second};
}

RequestLookup findRequest(std::int64_t requestId) {
    const auto lookup = findUserContext(static_cast<int>(requestId >> 32));
    if (lookup.error != 0) return {lookup.error, nullptr, nullptr};
    const auto request = lookup.user->requests.find(requestId);
    if (request == lookup.user->requests.end()) return {SCE_NP_WEBAPI_ERROR_REQUEST_NOT_FOUND, lookup.user, nullptr};
    return {0, lookup.user, &request->second};
}

int createRequest(int titleUserCtxId, bool multipart, std::int64_t* requestId) {
    const auto lookup = findUserContext(titleUserCtxId);
    if (lookup.error != 0) return lookup.error;
    lastRequest = lastRequest % 0xffffffff + 1;
    const std::int64_t base = static_cast<std::int64_t>(titleUserCtxId) << 32;
    std::int64_t id = base | lastRequest;
    while (lookup.user->requests.contains(id)) --id;
    if (id <= base) throw std::runtime_error("sceNpWebApiCreateRequest: every request id of the user context is in use");
    lookup.user->requests.emplace(id, Request{false, multipart, false, 0, 0});
    if (requestId) *requestId = id;
    return 0;
}

int sendRequest(std::int64_t requestId) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findRequest(requestId);
    if (lookup.error != 0) return lookup.error;
    auto& request = *lookup.request;
    if (request.timeout != 0 && request.endTime == 0) request.endTime = sceKernelGetProcessTime() + request.timeout;
    if (request.multipart) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    if (request.aborted && !request.expired) return SCE_NP_WEBAPI_ERROR_ABORTED;
    return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

}

extern "C" {

int APS5_VABI sceNpWebApiAbortRequest(std::int64_t requestId) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findRequest(requestId);
    if (lookup.error != 0) return lookup.error;
    lookup.request->aborted = true;
    return 0;
}

int APS5_VABI sceNpWebApiCreateContext(int libCtxId, const void* onlineId) {
    if (libCtxId >= 0x8000) return SCE_NP_WEBAPI_ERROR_INVALID_LIB_CONTEXT_ID;
    if (!onlineId) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    auto& userContexts = context->second.userContexts;
    for (const auto& [id, user] : userContexts) {
        if (user.userId == USER_SERVICE_INITIAL_USER_ID) return SCE_NP_WEBAPI_ERROR_USER_CONTEXT_ALREADY_EXIST;
    }
    lastUserContext = lastUserContext % 0xffff + 1;
    const int titleUserCtxId = (libCtxId << 16) | lastUserContext;
    userContexts.emplace(titleUserCtxId, UserContext{USER_SERVICE_INITIAL_USER_ID, {}, {}, {}});
    return titleUserCtxId;
}

int APS5_VABI sceNpWebApiCreateHandle(int libCtxId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    const auto handle = nextObject();
    context->second.handles.insert(handle);
    if (compiledSdkVersion() >= SDK_VERSION_3_00) context->second.timeoutCapableHandles.insert(handle);
    return handle;
}

int APS5_VABI sceNpWebApiCreatePushEventFilter(int libCtxId, const void* filterParam, std::size_t filterParamNum) {
    if (!filterParam || filterParamNum == 0) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    const auto filter = nextObject();
    context->second.pushEventFilters.insert(filter);
    return filter;
}

int APS5_VABI sceNpWebApiCreateRequest(int titleUserCtxId, const char* apiGroup, const char* path, int method, const NpWebApiContentParameter* contentParameter, std::int64_t* requestId) {
    if (!apiGroup || !path) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    if (contentParameter && contentParameter->contentLength != 0 && !contentParameter->contentType) return SCE_NP_WEBAPI_ERROR_INVALID_CONTENT_PARAMETER;
    if (method > SCE_NP_WEBAPI_HTTP_METHOD_DELETE && compiledSdkVersion() >= SDK_VERSION_2_50) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    return createRequest(titleUserCtxId, false, requestId);
}

int APS5_VABI sceNpWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char* npServiceName, std::uint32_t npServiceLabel, const void* filterParam, std::size_t filterParamNum) {
    if (!npServiceName || !filterParam || filterParamNum == 0) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    if (npServiceLabel == SCE_NP_INVALID_SERVICE_LABEL && compiledSdkVersion() >= SDK_VERSION_2_00) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    if (!context->second.handles.contains(handleId)) return SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    return SCE_NP_WEBAPI_ERROR_SIGNED_IN_USER_NOT_FOUND;
}

int APS5_VABI sceNpWebApiDeleteContext(int titleUserCtxId) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findUserContext(titleUserCtxId);
    if (lookup.error != 0) return lookup.error;
    lookup.library->userContexts.erase(titleUserCtxId);
    return 0;
}

int APS5_VABI sceNpWebApiDeleteHandle(int libCtxId, int handleId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    context->second.timeoutCapableHandles.erase(handleId);
    return context->second.handles.erase(handleId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
}

int APS5_VABI sceNpWebApiDeletePushEventFilter(int libCtxId, int filterId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return context->second.pushEventFilters.erase(filterId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
}

int APS5_VABI sceNpWebApiDeleteRequest(std::int64_t requestId) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findRequest(requestId);
    if (lookup.error != 0) return lookup.error;
    lookup.user->requests.erase(requestId);
    return 0;
}

int APS5_VABI sceNpWebApiDeleteServicePushEventFilter(int libCtxId, int filterId) {
    (void)filterId;
    std::lock_guard lock(contextsMutex);
    if (!contexts.contains(libCtxId)) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND;
}

int APS5_VABI sceNpWebApiGetHttpResponseHeaderValue() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiGetHttpResponseHeaderValueLength() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiGetHttpStatusCode(std::int64_t requestId, int* statusCode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiInitialize(int libHttpCtxId, std::size_t poolSize) {
    std::lock_guard lock(contextsMutex);
    for (int attempt = 0; attempt < 0x7fff; ++attempt) {
        lastContext = lastContext % 0x7fff + 1;
        if (contexts.contains(lastContext)) continue;
        contexts[lastContext] = {libHttpCtxId, poolSize, {}, {}, {}};
        return lastContext;
    }
    return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_MAX;
}

int APS5_VABI sceNpWebApiReadData(std::int64_t requestId, void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiRegisterPushEventCallback(int titleUserCtxId, int filterId, void* callback, void* userArg) {
    (void)userArg;
    if (!callback && compiledSdkVersion() >= SDK_VERSION_1_00) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto lookup = findUserContext(titleUserCtxId);
    if (lookup.error != 0) return lookup.error;
    if (!lookup.library->pushEventFilters.contains(filterId) && compiledSdkVersion() >= SDK_VERSION_2_50) return SCE_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
    const auto callbackId = nextObject();
    lookup.user->pushEventCallbacks.insert(callbackId);
    return callbackId;
}

int APS5_VABI sceNpWebApiRegisterServicePushEventCallback(int titleUserCtxId, int filterId, void* callback, void* userArg) {
    (void)filterId;
    (void)userArg;
    if (!callback && compiledSdkVersion() >= SDK_VERSION_1_00) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto lookup = findUserContext(titleUserCtxId);
    if (lookup.error != 0) return lookup.error;
    if (compiledSdkVersion() >= SDK_VERSION_2_50) return SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND;
    if (!callback) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    const auto callbackId = nextObject();
    lookup.user->servicePushEventCallbacks.insert(callbackId);
    return callbackId;
}

int APS5_VABI sceNpWebApiSendRequest(std::int64_t requestId, const void* data, std::size_t dataSize) {
    (void)data;
    (void)dataSize;
    return sendRequest(requestId);
}

int APS5_VABI sceNpWebApiTerminate(int libCtxId) {
    std::lock_guard lock(contextsMutex);
    return contexts.erase(libCtxId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
}

int APS5_VABI sceNpWebApiUnregisterPushEventCallback(int titleUserCtxId, int callbackId) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findUserContext(titleUserCtxId);
    if (lookup.error != 0) return lookup.error;
    return lookup.user->pushEventCallbacks.erase(callbackId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_PUSH_EVENT_CALLBACK_NOT_FOUND;
}

int APS5_VABI sceNpWebApiUnregisterServicePushEventCallback(int titleUserCtxId, int callbackId) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findUserContext(titleUserCtxId);
    if (lookup.error != 0) return lookup.error;
    return lookup.user->servicePushEventCallbacks.erase(callbackId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_CALLBACK_NOT_FOUND;
}

int APS5_VABI sceNpWebApiUtilityParseNpId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiAbortHandle(int libCtxId, int handleId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return context->second.handles.contains(handleId) ? 0 : SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
}

int APS5_VABI sceNpWebApiAddHttpRequestHeader(std::int64_t requestId, const char* fieldName, const char* value) {
    if (!fieldName || !value) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    return findRequest(requestId).error;
}

int APS5_VABI sceNpWebApiAddMultipartPart() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sceNpWebApiCheckTimeout() {
    const std::uint64_t now = sceKernelGetProcessTime();
    std::lock_guard lock(contextsMutex);
    if (contexts.empty() || now < lastTimeoutScan + TIMEOUT_SCAN_PERIOD) return;
    lastTimeoutScan = now;
    for (auto& [libCtxId, library] : contexts) {
        for (auto& [titleUserCtxId, user] : library.userContexts) {
            for (auto& [requestId, request] : user.requests) {
                if (request.expired || request.endTime == 0 || request.endTime >= now) continue;
                request.expired = true;
                request.aborted = true;
            }
        }
    }
}

int APS5_VABI sceNpWebApiClearAllUnusedConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiClearUnusedConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiCreateMultipartRequest(int titleUserCtxId, const char* apiGroup, const char* path, int method, std::int64_t* requestId) {
    if (!apiGroup || !path) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    if (method > SCE_NP_WEBAPI_HTTP_METHOD_DELETE && compiledSdkVersion() >= SDK_VERSION_2_50) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    return createRequest(titleUserCtxId, true, requestId);
}

int APS5_VABI sceNpWebApiGetConnectionStats() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiGetErrorCode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiInitializeForPresence() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiIntCreateCtxIndExtdPushEventFilter() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiIntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiIntCreateServicePushEventFilter() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiIntInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiIntRegisterServicePushEventCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiIntRegisterServicePushEventCallbackA() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiRegisterExtdPushEventCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiRegisterNotificationCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiSendMultipartRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiSendMultipartRequest2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiSendRequest2(std::int64_t requestId, const void* data, std::size_t dataSize, void* responseInformation) {
    (void)data;
    (void)dataSize;
    (void)responseInformation;
    return sendRequest(requestId);
}

int APS5_VABI sceNpWebApiSetHandleTimeout(int libCtxId, int handleId, std::uint32_t timeout) {
    (void)timeout;
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return context->second.timeoutCapableHandles.contains(handleId) ? 0 : SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
}

int APS5_VABI sceNpWebApiSetMaxConnection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiSetMultipartContentType(std::int64_t requestId, const char* typeName, const char* boundary) {
    (void)typeName;
    (void)boundary;
    std::lock_guard lock(contextsMutex);
    return findRequest(requestId).error;
}

int APS5_VABI sceNpWebApiSetRequestTimeout(std::int64_t requestId, std::uint32_t timeout) {
    std::lock_guard lock(contextsMutex);
    const auto lookup = findRequest(requestId);
    if (lookup.error != 0) return lookup.error;
    lookup.request->timeout = timeout;
    return 0;
}

int APS5_VABI sceNpWebApiUnregisterNotificationCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApiVshInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

}
