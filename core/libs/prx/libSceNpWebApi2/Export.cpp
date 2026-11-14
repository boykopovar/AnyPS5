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
static constexpr int SCE_NP_WEBAPI2_ERROR_UNAVAILABLE = static_cast<int>(0x80553406);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT = static_cast<int>(0x80553404);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE = static_cast<int>(0x8055340D);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_CONTENT_PARAMETER = static_cast<int>(0x80553408);
static std::atomic<int> g_nextHandle{1};

static_assert(sizeof(NpWebApi2PushContextId) == 37);

static_assert(sizeof(NpWebApi2MemoryPoolStats) == 32);

static std::mutex g_poolsMutex;
static std::unordered_map<int, size_t> g_pools;

static int CreateLibraryContext(size_t poolSize) {
    const int libCtxId = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_poolsMutex);
    g_pools[libCtxId] = poolSize;
    return libCtxId;
}

struct PushHandle {
    std::int32_t libraryContextId;
    std::uint32_t timeout = 0;
    bool aborted = false;
};

static std::unordered_map<int, PushHandle> pushHandles;

struct MultipartRequest {
    std::vector<std::size_t> parts;
    std::string contentType;
    std::string boundary;
};

static std::mutex g_multipartMutex;
static std::unordered_map<std::int64_t, MultipartRequest> g_multipartRequests;

static void RegisterRequest(std::int64_t requestId) {
    std::lock_guard lock(g_multipartMutex);
    g_multipartRequests.try_emplace(requestId);
}

static void ForgetRequest(std::int64_t requestId) {
    std::lock_guard lock(g_multipartMutex);
    g_multipartRequests.erase(requestId);
}

extern "C" {

int APS5_VABI sceNpWebApi2IntGlobalPush2Initialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpWebApi2IntGlobalPush2Terminate() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceNpWebApi2AbortRequest(int64_t request_id) {
    (void)request_id;
    return 0;
}

int APS5_VABI sceNpWebApi2AddHttpRequestHeader(int64_t request_id, const char* field_name, const char* value) {
    (void)request_id;
    (void)field_name;
    (void)value;
    return 0;
}

int APS5_VABI sceNpWebApi2AddWebTraceTag(std::int64_t requestId, const char* value) {
    (void)requestId;
    if (!value) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return 0;
}

void APS5_VABI sceNpWebApi2CheckTimeout(void) {
}

int APS5_VABI sceNpWebApi2CreateRequest(int user_context_id, const char* api_group, const char* path, const char* method, const void* content_parameter, int64_t* request_id) {
    (void)user_context_id;
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
    const int64_t created = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    if (!GuestCopyOut_nid_no_patch(request_id, &created, sizeof(created))) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    RegisterRequest(created);
    return 0;
}

int APS5_VABI sceNpWebApi2CreateUserContext(int lib_ctx_id, int user_id) {
    (void)lib_ctx_id;
    (void)user_id;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpWebApi2DeleteRequest(int64_t request_id) {
    ForgetRequest(request_id);
    return 0;
}

int APS5_VABI sceNpWebApi2DeleteUserContext(int user_context_id) {
    (void)user_context_id;
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
    std::lock_guard lock(g_poolsMutex);
    const auto pool = g_pools.find(libraryContextId);
    if (pool == g_pools.end()) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    if (!stats) return 0;
    NpWebApi2MemoryPoolStats localStats{};
    localStats.pool_size = pool->second;
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
    std::lock_guard lock(g_poolsMutex);
    if (!g_pools.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const int handleId = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    pushHandles.emplace(handleId, PushHandle{libraryContextId});
    return handleId;
}

int APS5_VABI sceNpWebApi2PushEventAbortHandle(int libraryContextId, int handleId) {
    std::lock_guard lock(g_poolsMutex);
    if (!g_pools.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    const auto handle = pushHandles.find(handleId);
    if (handle == pushHandles.end() || handle->second.libraryContextId != libraryContextId) return SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE;
    handle->second.aborted = true;
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventDeleteHandle(int libraryContextId, int handleId) {
    std::lock_guard lock(g_poolsMutex);
    if (!g_pools.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
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
    std::lock_guard lock(g_poolsMutex);
    if (!g_pools.contains(lib_ctx_id)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
    std::erase_if(pushHandles, [lib_ctx_id](const auto& item) { return item.second.libraryContextId == lib_ctx_id; });
    g_pools.erase(lib_ctx_id);
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventCreatePushContext(std::int32_t userContextId, NpWebApi2PushContextId* pushContextId) {
    return SCE_NP_WEBAPI2_ERROR_UNAVAILABLE;
}

int APS5_VABI sceNpWebApi2PushEventDeleteFilter(std::int32_t libraryContextId, std::int32_t filterId) {
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventSetHandleTimeout(int libraryContextId, int handleId, std::uint32_t timeout) {
    std::lock_guard lock(g_poolsMutex);
    if (!g_pools.contains(libraryContextId)) return SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT;
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
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventUnregisterCallback(std::int32_t userContextId, std::int32_t callbackId) {
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventUnregisterPushContextCallback(std::int32_t userContextId, std::int32_t callbackId) {
    return 0;
}

int APS5_VABI sceNpWebApi2SetRequestTimeout(int64_t request_id, uint32_t timeout) {
    (void)request_id;
    (void)timeout;
    return 0;
}

int APS5_VABI sceNpWebApi2AddMultipartPart(int64_t request_id, const void* part_parameter, int32_t* part_index) {
    if (!part_parameter) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::uint64_t parameter[3]{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(parameter, part_parameter, sizeof(parameter));
    errno = savedErrno;
    if (!copied) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    if (parameter[1] != 0 && parameter[0] == 0) NotImplemented_nid_no_patch(__func__);
    std::lock_guard lock(g_multipartMutex);
    const auto request = g_multipartRequests.find(request_id);
    if (request == g_multipartRequests.end()) return SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE;
    request->second.parts.push_back(static_cast<std::size_t>(parameter[2]));
    if (part_index != nullptr) {
        const std::int32_t index = static_cast<std::int32_t>(request->second.parts.size());
        const int outputErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(part_index, &index, sizeof(index));
        errno = outputErrno;
        if (!written) {
            request->second.parts.pop_back();
            return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
        }
    }
    return 0;
}

int APS5_VABI sceNpWebApi2CreateMultipartRequest(int user_ctx_id, const char* api_group, const char* path, const char* method, int64_t* request_id) {
    (void)user_ctx_id;
    if (!api_group || !path || !method) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    const int64_t created = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    if (request_id && !GuestCopyOut_nid_no_patch(request_id, &created, sizeof(created))) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    RegisterRequest(created);
    return 0;
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
    if (CopyGuestString_nid_no_patch(type_name, 64, contentType) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    if (boundary != nullptr && CopyGuestString_nid_no_patch(boundary, 64, boundaryCopy) != 0) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(g_multipartMutex);
    const auto request = g_multipartRequests.find(request_id);
    if (request == g_multipartRequests.end()) return SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE;
    request->second.contentType = std::move(contentType);
    request->second.boundary = std::move(boundaryCopy);
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
