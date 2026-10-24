#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include <atomic>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <mutex>
#include <memory>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static constexpr int ERROR_NETWORK = static_cast<int>(0x80436063);
static constexpr int ERROR_INVALID_ID = static_cast<int>(0x817B1100);
static constexpr int ERROR_NULL_POINTER = static_cast<int>(0x817B1225);

enum class Http2Kind : std::uint8_t { Context, Template, Request, CookieBox };

using Http2AuthInfoCallback = int (APS5_VABI*)(int request, int authType, const char* realm, char* username, char* password, int isNeedEntity, std::uint8_t** entityBody, std::size_t* entitySize, int* isSave, void* userArg);

struct Http2Handle {
    Http2Kind kind;
    int owner;
    std::size_t poolSize;
    std::shared_ptr<std::byte[]> cookieMemory;
    bool authEnabled = true;
    bool autoRedirect = true;
    std::vector<std::pair<std::string, std::string>> headers;
    Http2AuthInfoCallback authInfoCallback = nullptr;
    void* authInfoArgument = nullptr;
    void* preSendCallback = nullptr;
    void* preSendArgument = nullptr;
};

static std::atomic<int> g_nextHandle{1};
static std::mutex g_handlesMutex;
static std::unordered_map<int, Http2Handle> g_handles;

static Http2Handle* FindHandle(int id, Http2Kind kind) {
    const auto entry = g_handles.find(id);
    return entry != g_handles.end() && entry->second.kind == kind ? &entry->second : nullptr;
}

static Http2Handle* FindAnyHandle(int id) {
    const auto entry = g_handles.find(id);
    return entry != g_handles.end() ? &entry->second : nullptr;
}

static std::vector<int> ReleaseOwnedHandles(int owner) {
    std::vector<int> owned;
    for (const auto& entry : g_handles) {
        if (entry.second.owner == owner) owned.push_back(entry.first);
    }
    std::vector<int> removed;
    for (const int id : owned) {
        const std::vector<int> descendants = ReleaseOwnedHandles(id);
        removed.insert(removed.end(), descendants.begin(), descendants.end());
        g_handles.erase(id);
        removed.push_back(id);
    }
    return removed;
}

static bool HandleIsLive(int id) {
    std::lock_guard lock(g_handlesMutex);
    return FindAnyHandle(id) != nullptr;
}

struct Http2MemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

static std::mutex g_completionsMutex;
static std::unordered_map<int, std::deque<Http2AsyncResult>> g_completions;

static void EraseCompletions(const std::vector<int>& ids) {
    std::lock_guard lock(g_completionsMutex);
    for (const int id : ids) g_completions.erase(id);
}

static int CompleteAsync(const char* function, int req_id, const Http2AsyncOption* kqueue_option, const void* option) {
    if (option != nullptr) NotImplemented_nid_no_patch(function);
    bool queued = false;
    {
        std::lock_guard handlesLock(g_handlesMutex);
        if (FindHandle(req_id, Http2Kind::Request) != nullptr) {
            std::lock_guard completionsLock(g_completionsMutex);
            Http2AsyncResult completion{};
            completion.req_id = req_id;
            completion.result = ERROR_NETWORK;
            g_completions[req_id].push_back(completion);
            queued = true;
        }
    }
    if (!queued) return ERROR_INVALID_ID;
    if (kqueue_option == nullptr) return 0;
    if (EqueueTriggerEvent_nid_postfix(kqueue_option->equeue, static_cast<uintptr_t>(kqueue_option->user_event_id), EVFILT_USER, kqueue_option->user_data) != 0) {
        {
            std::lock_guard lock(g_completionsMutex);
            const auto pending = g_completions.find(req_id);
            if (pending != g_completions.end() && !pending->second.empty()) pending->second.pop_back();
        }
        NotImplemented_nid_no_patch(function);
    }
    return 0;
}

static bool HeaderNameEquals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
        if (lower(left[i]) != lower(right[i])) return false;
    }
    return true;
}

static void EraseHeader(Http2Handle& handle, std::string_view name) {
    std::erase_if(handle.headers, [&](const auto& header) { return HeaderNameEquals(header.first, name); });
}

static int SetFlag(int id, bool Http2Handle::*flag, int enable) {
    std::lock_guard lock(g_handlesMutex);
    Http2Handle* handle = FindAnyHandle(id);
    if (handle == nullptr) return ERROR_INVALID_ID;
    handle->*flag = enable != 0;
    return 0;
}

static int GetFlag(const char* function, int id, bool Http2Handle::*flag, int* enable) {
    std::lock_guard lock(g_handlesMutex);
    const Http2Handle* handle = FindAnyHandle(id);
    if (handle == nullptr) return ERROR_INVALID_ID;
    if (enable == nullptr) NotImplemented_nid_no_patch(function);
    *enable = handle->*flag ? 1 : 0;
    return 0;
}

static int FlushContextCache(int lib_http2_ctx_id) {
    std::lock_guard lock(g_handlesMutex);
    return FindHandle(lib_http2_ctx_id, Http2Kind::Context) != nullptr ? 0 : ERROR_INVALID_ID;
}

using Http2WebSocketTextCallback = void (APS5_VABI*)(std::int32_t requestId, const char* text, std::uint32_t flags, void* userData);
using Http2WebSocketDataCallback = void (APS5_VABI*)(std::int32_t requestId, const void* data, std::size_t dataSize, std::uint32_t flags, void* userData);
using Http2WebSocketCloseCallback = void (APS5_VABI*)(std::int32_t requestId, const std::uint16_t* status, const char* reason, std::uint32_t flags, void* userData);
using Http2WebSocketErrorCallback = void (APS5_VABI*)(std::int32_t requestId, std::int32_t error, void* userData);

extern "C" {

int APS5_VABI sceHttp2AddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    std::lock_guard lock(g_handlesMutex);
    Http2Handle* handle = FindAnyHandle(id);
    if (handle == nullptr) return ERROR_INVALID_ID;
    if (name == nullptr || value == nullptr || mode > 1) NotImplemented_nid_no_patch(__func__);
    if (mode == 0) EraseHeader(*handle, name);
    handle->headers.emplace_back(name, value);
    return 0;
}

int APS5_VABI sceHttp2CreateRequestWithURL(int tmpl_id, const char* method, const char* url, uint64_t content_length) {
    (void)method;
    (void)url;
    (void)content_length;
    std::lock_guard lock(g_handlesMutex);
    const Http2Handle* parent = FindHandle(tmpl_id, Http2Kind::Template);
    if (parent == nullptr) return ERROR_INVALID_ID;
    Http2Handle request{Http2Kind::Request, tmpl_id, 0};
    request.authEnabled = parent->authEnabled;
    request.autoRedirect = parent->autoRedirect;
    request.headers = parent->headers;
    request.authInfoCallback = parent->authInfoCallback;
    request.authInfoArgument = parent->authInfoArgument;
    request.preSendCallback = parent->preSendCallback;
    request.preSendArgument = parent->preSendArgument;
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    g_handles[id] = std::move(request);
    return id;
}

int APS5_VABI sceHttp2CreateTemplate(int lib_http2_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)user_agent;
    (void)http_ver;
    (void)is_auto_proxy_conf;
    std::lock_guard lock(g_handlesMutex);
    if (FindHandle(lib_http2_ctx_id, Http2Kind::Context) == nullptr) return ERROR_INVALID_ID;
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    g_handles[id] = {Http2Kind::Template, lib_http2_ctx_id, 0};
    return id;
}

int APS5_VABI sceHttp2DeleteRequest(int req_id) {
    {
        std::lock_guard lock(g_handlesMutex);
        if (FindHandle(req_id, Http2Kind::Request) == nullptr) return ERROR_INVALID_ID;
        g_handles.erase(req_id);
    }
    EraseCompletions({req_id});
    return 0;
}

int APS5_VABI sceHttp2DeleteTemplate(int tmpl_id) {
    std::vector<int> removed;
    {
        std::lock_guard lock(g_handlesMutex);
        if (FindHandle(tmpl_id, Http2Kind::Template) == nullptr) return ERROR_INVALID_ID;
        removed = ReleaseOwnedHandles(tmpl_id);
        g_handles.erase(tmpl_id);
    }
    EraseCompletions(removed);
    return 0;
}

int APS5_VABI sceHttp2GetAllResponseHeaders(int req_id, char** header, size_t* header_size) {
    (void)req_id;
    (void)header;
    (void)header_size;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttp2GetResponseContentLength(int req_id, int* result, uint64_t* content_length) {
    (void)req_id;
    (void)result;
    (void)content_length;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttp2GetStatusCode(int req_id, int* status_code) {
    (void)req_id;
    (void)status_code;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttp2Init(int libnet_mem_id, int libssl_ctx_id, size_t pool_size, int max_concurrent_request) {
    (void)libnet_mem_id;
    (void)libssl_ctx_id;
    (void)max_concurrent_request;
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_handlesMutex);
    g_handles[id] = {Http2Kind::Context, 0, pool_size};
    return id;
}

int APS5_VABI sceHttp2ReadData(int req_id, void* data, size_t size) {
    (void)req_id;
    (void)data;
    (void)size;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttp2ReadDataAsync(int req_id, void* data, size_t size, void* kqueue_option, void* option) {
    (void)req_id;
    (void)data;
    (void)size;
    (void)kqueue_option;
    (void)option;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttp2SendRequest(int req_id, const void* post_data, size_t size) {
    (void)req_id;
    (void)post_data;
    (void)size;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttp2SendRequestAsync(int req_id, const void* post_data, size_t size, Http2AsyncOption* kqueue_option, void* option) {
    (void)post_data;
    (void)size;
    return CompleteAsync(__func__, req_id, kqueue_option, option);
}

int APS5_VABI sceHttp2SetAuthEnabled(int id, int is_enable) {
    return SetFlag(id, &Http2Handle::authEnabled, is_enable);
}

int APS5_VABI sceHttp2SetAutoRedirect(int id, int enable) {
    return SetFlag(id, &Http2Handle::autoRedirect, enable);
}

int APS5_VABI sceHttp2SetConnectionWaitTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttp2SetConnectTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttp2SetInflateGZIPEnabled(int id, int enable) {
    (void)id;
    (void)enable;
    return 0;
}

int APS5_VABI sceHttp2SetRecvTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttp2SetRedirectCallback(int id, void* cb_func, void* user_arg) {
    (void)id;
    (void)cb_func;
    (void)user_arg;
    return 0;
}

int APS5_VABI sceHttp2SetRequestContentLength(int id, uint64_t content_length) {
    (void)id;
    (void)content_length;
    return 0;
}

int APS5_VABI sceHttp2SetResolveTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttp2SetSendTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttp2SetSslCallback(int id, void* cb_func, void* user_arg) {
    (void)id;
    (void)cb_func;
    (void)user_arg;
    return 0;
}

int APS5_VABI sceHttp2SetTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttp2SslDisableOption(int id, uint32_t ssl_flags) {
    (void)ssl_flags;
    return HandleIsLive(id) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2SslEnableOption(int id, uint32_t ssl_flags) {
    (void)ssl_flags;
    return HandleIsLive(id) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2SetMinSslVersion(int id, uint32_t ssl_version) {
    (void)ssl_version;
    return HandleIsLive(id) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2Term(int lib_http2_ctx_id) {
    std::vector<int> removed;
    {
        std::lock_guard lock(g_handlesMutex);
        if (FindHandle(lib_http2_ctx_id, Http2Kind::Context) == nullptr) return ERROR_INVALID_ID;
        removed = ReleaseOwnedHandles(lib_http2_ctx_id);
        g_handles.erase(lib_http2_ctx_id);
    }
    EraseCompletions(removed);
    return 0;
}

int APS5_VABI sceHttp2WaitAsync(int req_id, Http2AsyncResult* result, uint32_t* timeout, void* option) {
    (void)timeout;
    if (result == nullptr || option != nullptr) NotImplemented_nid_no_patch(__func__);
    std::lock_guard lock(g_completionsMutex);
    const auto pending = g_completions.find(req_id);
    if (pending == g_completions.end() || pending->second.empty()) NotImplemented_nid_no_patch("sceHttp2WaitAsync: waiting for an operation that has not completed");
    *result = pending->second.front();
    pending->second.pop_front();
    return 0;
}

int APS5_VABI sceHttp2AbortRequest(int req_id) {
    (void)req_id;
    return 0;
}

int APS5_VABI sceHttp2GetMemoryPoolStats(int lib_http2_ctx_id, Http2MemoryPoolStats* stats) {
    if (stats == nullptr) return ERROR_NULL_POINTER;
    std::lock_guard lock(g_handlesMutex);
    const Http2Handle* handle = FindHandle(lib_http2_ctx_id, Http2Kind::Context);
    if (handle == nullptr) {
        std::memset(stats, 0, sizeof(*stats));
        return ERROR_INVALID_ID;
    }
    *stats = {handle->poolSize, 0, 0, 0};
    return 0;
}

int APS5_VABI sceHttp2CookieFlush(int id) {
    (void)id;
    return 0;
}

int APS5_VABI sceHttp2CreateCookieBox(std::size_t poolSize) {
    if (poolSize < 0x10000) return static_cast<int>(0x817b121du);
    if (poolSize > std::numeric_limits<std::size_t>::max() - 0x3fff) throw std::overflow_error("sceHttp2CreateCookieBox: pool size overflow");
    const auto allocationSize = (poolSize + 0x3fff) & ~std::size_t{0x3fff};
    auto memory = std::make_shared<std::byte[]>(allocationSize);
    std::lock_guard lock(g_handlesMutex);
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    g_handles[id] = {Http2Kind::CookieBox, 0, poolSize, std::move(memory)};
    return id;
}

int APS5_VABI sceHttp2DeleteCookieBox(int cookie_box_id) {
    std::lock_guard lock(g_handlesMutex);
    if (FindHandle(cookie_box_id, Http2Kind::CookieBox) == nullptr) return ERROR_INVALID_ID;
    g_handles.erase(cookie_box_id);
    return 0;
}

int APS5_VABI sceHttp2SetCookieBox(int id, int cookie_box_id) {
    (void)id;
    (void)cookie_box_id;
    return 0;
}

int APS5_VABI sceHttp2SetRequestNoContentLength(int id) {
    (void)id;
    return 0;
}


int APS5_VABI sceHttp2SetResolveRetry(int id, int32_t retry) {
    (void)id;
    (void)retry;
    return 0;
}

int APS5_VABI sceHttp2WebSocketCreateRequest(std::int32_t templateId, const char* url, Http2WebSocketTextCallback textCallback, Http2WebSocketDataCallback dataCallback, Http2WebSocketCloseCallback closeCallback, Http2WebSocketErrorCallback errorCallback, void* receiveBuffer, std::size_t receiveBufferSize, void* userData, const void* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2WebSocketCloseAsync(std::int32_t requestId, const std::uint16_t* status, const char* reason, std::uint8_t flags, const Http2AsyncOption* asyncOption, const void* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2WebSocketSendTextMessageAsync(std::int32_t requestId, const char* text, std::uint8_t flags, const Http2AsyncOption* asyncOption, const void* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2WebSocketSendDataMessageAsync(std::int32_t requestId, const void* data, std::size_t dataSize, std::uint8_t flags, const Http2AsyncOption* asyncOption, const void* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2WebSocketSetPingInterval(std::int32_t requestId, std::uint32_t intervalMicroseconds) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2WebSocketSetPingTimeout(std::int32_t requestId, std::uint32_t timeoutMicroseconds) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceHttp2AddCookie(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2AuthCacheFlush(int lib_http2_ctx_id) {
    return FlushContextCache(lib_http2_ctx_id);
}

int APS5_VABI sceHttp2CookieExport(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2CookieImport(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2GetAuthEnabled(int id, int* is_enable) {
    return GetFlag(__func__, id, &Http2Handle::authEnabled, is_enable);
}

int APS5_VABI sceHttp2GetAutoRedirect(int id, int* enable) {
    return GetFlag(__func__, id, &Http2Handle::autoRedirect, enable);
}

int APS5_VABI sceHttp2GetCookie(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2GetCookieBox(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2GetCookieStats(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2RedirectCacheFlush(int lib_http2_ctx_id) {
    return FlushContextCache(lib_http2_ctx_id);
}

int APS5_VABI sceHttp2RemoveRequestHeader(int id, const char* name) {
    std::lock_guard lock(g_handlesMutex);
    Http2Handle* handle = FindAnyHandle(id);
    if (handle == nullptr) return ERROR_INVALID_ID;
    if (name == nullptr) NotImplemented_nid_no_patch(__func__);
    EraseHeader(*handle, name);
    return 0;
}

int APS5_VABI sceHttp2SetAuthInfoCallback(int id, Http2AuthInfoCallback cb_func, void* user_arg) {
    std::lock_guard lock(g_handlesMutex);
    Http2Handle* handle = FindAnyHandle(id);
    if (handle == nullptr) return ERROR_INVALID_ID;
    handle->authInfoCallback = cb_func;
    handle->authInfoArgument = user_arg;
    return 0;
}

int APS5_VABI sceHttp2SetCookieMaxNum(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetCookieMaxNumPerDomain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetCookieMaxSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetCookieRecvCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetCookieSendCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetPreSendCallback(int id, void* cb_func, void* user_arg) {
    std::lock_guard lock(g_handlesMutex);
    Http2Handle* handle = FindAnyHandle(id);
    if (handle == nullptr) return ERROR_INVALID_ID;
    handle->preSendCallback = cb_func;
    handle->preSendArgument = user_arg;
    return 0;
}

}
