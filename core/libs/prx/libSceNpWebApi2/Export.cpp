#include "prx/libSceNpWebApi2/include/InventoryExports.hpp"
#include "prx/libSceNpWebApi2/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpWebApi2/include/Firmware320Exports.hpp"

// The PSN web API has no service behind it: handles can be created, but requests fail.
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80553402);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_LIB_CONTEXT_ID = static_cast<int>(0x80553403);
static constexpr int SCE_NP_WEBAPI2_ERROR_UNAVAILABLE = static_cast<int>(0x80553406);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT = static_cast<int>(0x80553404);
static constexpr int SCE_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND = static_cast<int>(0x80553405);
static constexpr int SCE_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80553406);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE = static_cast<int>(0x8055340D);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_CONTENT_PARAMETER = static_cast<int>(0x80553408);
static constexpr int SCE_NP_WEBAPI2_ERROR_USER_CONTEXT_ALREADY_EXISTS = static_cast<int>(0x8055340A);
static constexpr int SCE_NP_WEBAPI2_ERROR_NOT_MULTIPART_REQUEST = static_cast<int>(0x80553414);
static std::atomic<int> g_nextHandle{1};

static_assert(sizeof(NpWebApi2PushContextId) == 37);

static_assert(sizeof(NpWebApi2MemoryPoolStats) == 32);

struct PushHandle {
    std::int32_t libraryContextId;
    std::uint32_t timeout = 0;
    bool aborted = false;
};

struct RequestState {
    bool isMultipart = false;
    bool aborted = false;
    std::uint32_t timeout = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::size_t> parts;
    std::string contentType;
    std::string boundary;
};

struct UserContext {
    int userId = -1;
    std::uint32_t nextRequestSlot = 1;
    std::unordered_map<std::int64_t, RequestState> requests;
};

struct LibraryContext {
    std::size_t poolSize = 0;
    std::uint16_t nextUserSlot = 1;
    std::unordered_map<int, UserContext> userContexts;
};

static std::mutex g_stateMutex;
static std::uint16_t g_nextLibContextSlot = 1;
static std::unordered_map<int, LibraryContext> g_libraryContexts;
static std::unordered_map<int, PushHandle> pushHandles;

static int UnpackLibContextIdFromUser(int userContextId) {
    return static_cast<int>(static_cast<std::uint32_t>(userContextId) >> 16);
}

static int UnpackLibContextIdFromRequest(std::int64_t requestId) {
    return static_cast<int>(static_cast<std::uint64_t>(requestId) >> 48);
}

static int UnpackUserContextIdFromRequest(std::int64_t requestId) {
    return static_cast<int>(static_cast<std::uint64_t>(requestId) >> 32);
}

static LibraryContext* FindLibraryContextLocked(int libCtxId) {
    const auto it = g_libraryContexts.find(libCtxId);
    return it != g_libraryContexts.end() ? &it->second : nullptr;
}

static int FindUserContextLocked(int userContextId, UserContext*& outUserCtx) {
    LibraryContext* libCtx = FindLibraryContextLocked(UnpackLibContextIdFromUser(userContextId));
    if (!libCtx) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const auto it = libCtx->userContexts.find(userContextId);
    if (it == libCtx->userContexts.end()) return SCE_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    outUserCtx = &it->second;
    return 0;
}

static int FindRequestLocked(std::int64_t requestId, RequestState*& outRequest, UserContext** outUserCtx = nullptr) {
    LibraryContext* libCtx = FindLibraryContextLocked(UnpackLibContextIdFromRequest(requestId));
    if (!libCtx) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const auto userIt = libCtx->userContexts.find(UnpackUserContextIdFromRequest(requestId));
    if (userIt == libCtx->userContexts.end()) return SCE_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    const auto reqIt = userIt->second.requests.find(requestId);
    if (reqIt == userIt->second.requests.end()) return SCE_NP_WEBAPI2_ERROR_REQUEST_NOT_FOUND;
    if (outUserCtx) *outUserCtx = &userIt->second;
    outRequest = &reqIt->second;
    return 0;
}

static int AllocateLibraryContextIdLocked() {
    for (std::uint32_t attempt = 0; attempt < 0x7FFFu; ++attempt) {
        const int candidate = static_cast<int>(g_nextLibContextSlot);
        g_nextLibContextSlot = g_nextLibContextSlot >= 0x7FFFu ? 1u : static_cast<std::uint16_t>(g_nextLibContextSlot + 1u);
        if (!g_libraryContexts.contains(candidate)) return candidate;
    }
    return 0;
}

static int AllocateUserContextIdLocked(int libCtxId, LibraryContext& libCtx) {
    for (std::uint32_t attempt = 0; attempt < 0xFFFFu; ++attempt) {
        const std::uint16_t slot = libCtx.nextUserSlot;
        libCtx.nextUserSlot = libCtx.nextUserSlot == 0xFFFFu ? 1u : static_cast<std::uint16_t>(libCtx.nextUserSlot + 1u);
        const int candidate = static_cast<int>((static_cast<std::uint32_t>(libCtxId) << 16) | slot);
        if (!libCtx.userContexts.contains(candidate)) return candidate;
    }
    return 0;
}

static std::int64_t AllocateRequestIdLocked(int userCtxId, UserContext& userCtx) {
    for (std::uint32_t attempt = 0; attempt < 0x7FFFFFFFu; ++attempt) {
        const std::uint32_t slot = userCtx.nextRequestSlot;
        userCtx.nextRequestSlot = userCtx.nextRequestSlot >= 0x7FFFFFFFu ? 1u : userCtx.nextRequestSlot + 1u;
        const std::int64_t candidate = static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(userCtxId)) << 32) | slot);
        if (!userCtx.requests.contains(candidate)) return candidate;
    }
    return 0;
}

static int CreateLibraryContext(size_t poolSize) {
    std::lock_guard lock(g_stateMutex);
    const int libCtxId = AllocateLibraryContextIdLocked();
    if (libCtxId == 0) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    g_libraryContexts.emplace(libCtxId, LibraryContext{poolSize});
    return libCtxId;
}

static bool CopyOutRequestId(int64_t* requestId, int64_t created) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(requestId, &created, sizeof(created));
    errno = savedErrno;
    return copied;
}

static int RegisterRequestForUser(int userContextId, bool isMultipart, int64_t* requestId) {
    std::lock_guard lock(g_stateMutex);
    UserContext* userCtx = nullptr;
    const int lookupError = FindUserContextLocked(userContextId, userCtx);
    if (lookupError != 0) return lookupError;
    const std::int64_t created = AllocateRequestIdLocked(userContextId, *userCtx);
    if (created == 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    if (requestId != nullptr && !CopyOutRequestId(requestId, created)) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    userCtx->requests.emplace(created, RequestState{isMultipart});
    return 0;
}

static int CopyGuestStringPreservingErrno(const char* source, std::size_t maximum, std::string& out) {
    const int savedErrno = errno;
    const int status = CopyGuestString_nid_no_patch(source, maximum, out);
    errno = savedErrno;
    return status;
}

static int AppendCopiedRequestHeader(std::int64_t requestId, std::string fieldName, std::string value) {
    std::lock_guard lock(g_stateMutex);
    RequestState* request = nullptr;
    const int lookupError = FindRequestLocked(requestId, request);
    if (lookupError != 0) return lookupError;
    request->headers.emplace_back(std::move(fieldName), std::move(value));
    return 0;
}

extern "C" {

int APS5_VABI sceNpWebApi2IntGlobalPush2Initialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApi2IntGlobalPush2Terminate() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceNpWebApi2AbortRequest(int64_t request_id) {
    std::lock_guard lock(g_stateMutex);
    RequestState* request = nullptr;
    const int lookupError = FindRequestLocked(request_id, request);
    if (lookupError != 0) return lookupError;
    request->aborted = true;
    return 0;
}

int APS5_VABI sceNpWebApi2AddHttpRequestHeader(int64_t request_id, const char* field_name, const char* value) {
    if (!field_name || !value) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::string fieldNameCopy;
    std::string valueCopy;
    if (CopyGuestStringPreservingErrno(field_name, 4096, fieldNameCopy) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    if (CopyGuestStringPreservingErrno(value, 4096, valueCopy) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return AppendCopiedRequestHeader(request_id, std::move(fieldNameCopy), std::move(valueCopy));
}

int APS5_VABI sceNpWebApi2AddWebTraceTag(std::int64_t requestId, const char* value) {
    if (!value) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::string tagCopy;
    if (CopyGuestStringPreservingErrno(value, 4096, tagCopy) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return AppendCopiedRequestHeader(requestId, "X-Psn-WebTrace-Tag", std::move(tagCopy));
}

void APS5_VABI sceNpWebApi2CheckTimeout(void) {
}

int APS5_VABI sceNpWebApi2CreateRequest(int user_context_id, const char* api_group, const char* path, const char* method, const void* content_parameter, int64_t* request_id) {
    if (!api_group || !path || !method) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    if (content_parameter) {
        const auto* fields = static_cast<const std::uint64_t*>(content_parameter);
        std::uint64_t first = 0;
        std::uint64_t second = 0;
        const int savedErrno = errno;
        const bool firstCopied = GuestCopyIn_nid_no_patch(&first, &fields[0], sizeof(first));
        const bool secondCopied = !firstCopied || first == 0 || GuestCopyIn_nid_no_patch(&second, &fields[1], sizeof(second));
        errno = savedErrno;
        if (!firstCopied || !secondCopied) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
        if (first != 0 && second == 0) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTENT_PARAMETER;
    }
    if (!request_id) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return RegisterRequestForUser(user_context_id, false, request_id);
}

int APS5_VABI sceNpWebApi2CreateUserContext(int lib_ctx_id, int user_id) {
    if (lib_ctx_id <= 0 || lib_ctx_id >= 0x8000) return SCE_NP_WEBAPI2_ERROR_INVALID_LIB_CONTEXT_ID;
    if (user_id == -1) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_stateMutex);
    LibraryContext* libCtx = FindLibraryContextLocked(lib_ctx_id);
    if (!libCtx) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    for (const auto& entry : libCtx->userContexts) {
        if (entry.second.userId == user_id) return SCE_NP_WEBAPI2_ERROR_USER_CONTEXT_ALREADY_EXISTS;
    }
    const int userCtxId = AllocateUserContextIdLocked(lib_ctx_id, *libCtx);
    if (userCtxId == 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    libCtx->userContexts.emplace(userCtxId, UserContext{user_id});
    return userCtxId;
}

int APS5_VABI sceNpWebApi2DeleteRequest(int64_t request_id) {
    std::lock_guard lock(g_stateMutex);
    RequestState* request = nullptr;
    UserContext* userCtx = nullptr;
    const int lookupError = FindRequestLocked(request_id, request, &userCtx);
    if (lookupError != 0) return lookupError;
    (void)request;
    userCtx->requests.erase(request_id);
    return 0;
}

int APS5_VABI sceNpWebApi2DeleteUserContext(int user_context_id) {
    std::lock_guard lock(g_stateMutex);
    LibraryContext* libCtx = FindLibraryContextLocked(UnpackLibContextIdFromUser(user_context_id));
    if (!libCtx) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    if (libCtx->userContexts.erase(user_context_id) == 0) return SCE_NP_WEBAPI2_ERROR_USER_CONTEXT_NOT_FOUND;
    return 0;
}

int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValue(int64_t request_id, const char* field_name, char* value, size_t value_size) {
    (void)request_id;
    (void)field_name;
    (void)value;
    (void)value_size;
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValueLength(int64_t request_id, const char* field_name, size_t* value_length) {
    (void)request_id;
    (void)field_name;
    (void)value_length;
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2Initialize(int lib_http_ctx_id, size_t pool_size) {
    (void)lib_http_ctx_id;
    return CreateLibraryContext(pool_size);
}

int APS5_VABI sceNpWebApi2GetMemoryPoolStats(int libraryContextId, NpWebApi2MemoryPoolStats* stats) {
    std::lock_guard lock(g_stateMutex);
    const LibraryContext* libCtx = FindLibraryContextLocked(libraryContextId);
    if (!libCtx) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    if (!stats) return 0;
    NpWebApi2MemoryPoolStats localStats{};
    localStats.pool_size = libCtx->poolSize;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(stats, &localStats, sizeof(localStats));
    errno = savedErrno;
    if (!copied) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventCreateFilter(int lib_ctx_id, int handle_id, const char* np_service_name, uint32_t np_service_label, const void* filter_param, size_t filter_param_num) {
    (void)lib_ctx_id;
    (void)handle_id;
    (void)np_service_name;
    (void)np_service_label;
    (void)filter_param;
    (void)filter_param_num;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpWebApi2PushEventCreateHandle(int libraryContextId) {
    std::lock_guard lock(g_stateMutex);
    if (!g_libraryContexts.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const int handleId = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    pushHandles.emplace(handleId, PushHandle{libraryContextId});
    return handleId;
}

int APS5_VABI sceNpWebApi2PushEventAbortHandle(int libraryContextId, int handleId) {
    std::lock_guard lock(g_stateMutex);
    if (!g_libraryContexts.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const auto handle = pushHandles.find(handleId);
    if (handle == pushHandles.end() || handle->second.libraryContextId != libraryContextId) return SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE;
    handle->second.aborted = true;
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventDeleteHandle(int libraryContextId, int handleId) {
    std::lock_guard lock(g_stateMutex);
    if (!g_libraryContexts.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const auto handle = pushHandles.find(handleId);
    if (handle == pushHandles.end() || handle->second.libraryContextId != libraryContextId) return SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE;
    pushHandles.erase(handle);
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventDeletePushContext(int user_context_id, const void* push_context_id) {
    (void)user_context_id;
    (void)push_context_id;
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventRegisterCallback(int user_context_id, int filter_id, void* callback, void* user_arg) {
    (void)user_context_id;
    (void)filter_id;
    (void)user_arg;
    if (!callback) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpWebApi2ReadData(int64_t request_id, void* data, size_t size) {
    (void)request_id;
    (void)data;
    (void)size;
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2SendRequest(int64_t request_id, const void* data, size_t data_size, NpWebApi2ResponseInformationOption* response_info_option) {
    (void)request_id;
    (void)data;
    (void)data_size;
    (void)response_info_option;
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2Terminate(int lib_ctx_id) {
    std::lock_guard lock(g_stateMutex);
    if (!g_libraryContexts.contains(lib_ctx_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    std::erase_if(pushHandles, [lib_ctx_id](const auto& item) { return item.second.libraryContextId == lib_ctx_id; });
    g_libraryContexts.erase(lib_ctx_id);
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventCreatePushContext(std::int32_t userContextId, NpWebApi2PushContextId* pushContextId) {
    (void)userContextId;
    if (!pushContextId) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2PushEventDeleteFilter(std::int32_t libraryContextId, std::int32_t filterId) {
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventSetHandleTimeout(int libraryContextId, int handleId, std::uint32_t timeout) {
    std::lock_guard lock(g_stateMutex);
    if (!g_libraryContexts.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const auto handle = pushHandles.find(handleId);
    if (handle == pushHandles.end() || handle->second.libraryContextId != libraryContextId) return SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE;
    handle->second.timeout = timeout;
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventRegisterPushContextCallback(std::int32_t userContextId, std::int32_t filterId, void* callback, void* userArg) {
    if (!callback) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpWebApi2PushEventStartPushContextCallback(std::int32_t userContextId, const NpWebApi2PushContextId* pushContextId) {
    (void)userContextId;
    if (!pushContextId) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    NpWebApi2PushContextId localPushContextId{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localPushContextId, pushContextId, sizeof(localPushContextId));
    errno = savedErrno;
    if (!copied) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventUnregisterCallback(std::int32_t userContextId, std::int32_t callbackId) {
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventUnregisterPushContextCallback(std::int32_t userContextId, std::int32_t callbackId) {
    return 0;
}

int APS5_VABI sceNpWebApi2SetRequestTimeout(int64_t request_id, uint32_t timeout) {
    std::lock_guard lock(g_stateMutex);
    RequestState* request = nullptr;
    const int lookupError = FindRequestLocked(request_id, request);
    if (lookupError != 0) return lookupError;
    request->timeout = timeout;
    return 0;
}

int APS5_VABI sceNpWebApi2AddMultipartPart(int64_t request_id, const void* part_parameter, int32_t* part_index) {
    if (!part_parameter) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::uint64_t parameter[3]{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(parameter, part_parameter, sizeof(parameter));
    errno = savedErrno;
    if (!copied) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_stateMutex);
    RequestState* request = nullptr;
    const int lookupError = FindRequestLocked(request_id, request);
    if (lookupError != 0) return lookupError;
    if (!request->isMultipart) return SCE_NP_WEBAPI2_ERROR_NOT_MULTIPART_REQUEST;
    if (parameter[1] != 0 && parameter[0] == 0) NotImplemented_nid_no_patch(__func__);
    request->parts.push_back(static_cast<std::size_t>(parameter[2]));
    if (part_index != nullptr) {
        const std::int32_t index = static_cast<std::int32_t>(request->parts.size());
        const int outputErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(part_index, &index, sizeof(index));
        errno = outputErrno;
        if (!written) {
            request->parts.pop_back();
            return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
        }
    }
    return 0;
}

int APS5_VABI sceNpWebApi2CreateMultipartRequest(int user_ctx_id, const char* api_group, const char* path, const char* method, int64_t* request_id) {
    if (!api_group || !path || !method) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return RegisterRequestForUser(user_ctx_id, true, request_id);
}

int APS5_VABI sceNpWebApi2InitializeForPresence(int lib_http_ctx_id, uint64_t pool_size) {
    (void)lib_http_ctx_id;
    return CreateLibraryContext(pool_size);
}

int APS5_VABI sceNpWebApi2IntCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApi2IntInitialize(const NpWebApi2IntInitializeArgs* args) {
    NpWebApi2IntInitializeArgs local{};
    const int savedErrno = errno;
    const bool copied = args != nullptr && GuestCopyIn_nid_no_patch(&local, args, sizeof(local));
    errno = savedErrno;
    if (!copied || local.struct_size != sizeof(NpWebApi2IntInitializeArgs)) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return CreateLibraryContext(local.pool_size);
}

int APS5_VABI sceNpWebApi2IntInitialize2(const NpWebApi2IntInitialize2Args* args) {
    NpWebApi2IntInitialize2Args local{};
    const int savedErrno = errno;
    const bool copied = args != nullptr && GuestCopyIn_nid_no_patch(&local, args, sizeof(local));
    errno = savedErrno;
    if (!copied || local.struct_size != sizeof(NpWebApi2IntInitialize2Args)) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return CreateLibraryContext(local.pool_size);
}

int APS5_VABI sceNpWebApi2IntPushEventCreateCtxIndFilter() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApi2IntPushEventCreateDestPs4CtxIndFilter() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApi2SendMultipartRequest(int64_t request_id, int part_index, void* data, uint64_t data_size, void* resp_info_option) {
    (void)request_id;
    (void)resp_info_option;
    if (part_index <= 0 || !data || data_size == 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2SetMultipartContentType(int64_t request_id, const char* type_name, const char* boundary) {
    if (!type_name) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::string contentType;
    std::string boundaryCopy;
    if (CopyGuestStringPreservingErrno(type_name, 64, contentType) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    if (boundary != nullptr && CopyGuestStringPreservingErrno(boundary, 64, boundaryCopy) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_stateMutex);
    RequestState* request = nullptr;
    const int lookupError = FindRequestLocked(request_id, request);
    if (lookupError != 0) return lookupError;
    if (!request->isMultipart) return SCE_NP_WEBAPI2_ERROR_NOT_MULTIPART_REQUEST;
    request->contentType = std::move(contentType);
    request->boundary = std::move(boundaryCopy);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceNpWebApi2Unknown_4N85o28Ifbk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4N85o28Ifbk", libSceNpWebApi2Unknown_4N85o28Ifbk_nid_no_patch);

int APS5_VABI libSceNpWebApi2Unknown_A9IoYzANK3M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("A9IoYzANK3M", libSceNpWebApi2Unknown_A9IoYzANK3M_nid_no_patch);

int APS5_VABI libSceNpWebApi2Unknown_lylvdXiq1UE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lylvdXiq1UE", libSceNpWebApi2Unknown_lylvdXiq1UE_nid_no_patch);

int APS5_VABI libSceNpWebApi2Unknown_qaMcX2_plus_6ZiA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qaMcX2+6ZiA", libSceNpWebApi2Unknown_qaMcX2_plus_6ZiA_nid_no_patch);

}
