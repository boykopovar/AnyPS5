#include "prx/libSceNpWebApi2/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

// The PSN web API has no service behind it: handles can be created, but requests fail.
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80553402);
static constexpr int SCE_NP_WEBAPI2_ERROR_UNAVAILABLE = static_cast<int>(0x80553406);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_CONTEXT = static_cast<int>(0x80553404);
static constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_HANDLE = static_cast<int>(0x8055340D);
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

extern "C" {

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
    (void)api_group;
    (void)path;
    (void)method;
    (void)content_parameter;
    if (!request_id) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    *request_id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceNpWebApi2CreateUserContext(int lib_ctx_id, int user_id) {
    (void)lib_ctx_id;
    (void)user_id;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpWebApi2DeleteRequest(int64_t request_id) {
    (void)request_id;
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
    std::memset(stats, 0, sizeof(*stats));
    stats->pool_size = pool->second;
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
    (void)callback;
    (void)user_arg;
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

int APS5_VABI sceNpWebApi2PushEventRegisterPushContextCallback() {
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

int APS5_VABI sceNpWebApi2AddMultipartPart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApi2CreateMultipartRequest(int user_ctx_id, const char* api_group, const char* path, const char* method, int64_t* request_id) {
    NotImplemented_nid_no_patch(__func__);
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
    if (args == nullptr || args->struct_size != sizeof(NpWebApi2IntInitializeArgs)) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return CreateLibraryContext(args->pool_size);
}

int APS5_VABI sceNpWebApi2IntInitialize2(const NpWebApi2IntInitialize2Args* args) {
    if (args == nullptr || args->struct_size != sizeof(NpWebApi2IntInitialize2Args)) return SCE_NP_WEBAPI2_ERROR_INVALID_ARGUMENT;
    return CreateLibraryContext(args->pool_size);
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApi2SetMultipartContentType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
