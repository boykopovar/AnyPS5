#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static constexpr int ERROR_NETWORK = static_cast<int>(0x80436063);
static std::atomic<int> g_nextHandle{1};
static std::mutex g_poolsMutex;
static std::map<int, size_t> g_pools;

struct Http2MemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

static std::mutex g_completionsMutex;
static std::unordered_map<int, std::deque<Http2AsyncResult>> g_completions;

static void CompleteAsync(const char* function, int req_id, const Http2AsyncOption* kqueue_option, const void* option) {
    if (kqueue_option == nullptr) throw std::runtime_error(std::string(function) + ": null kqueue_option");
    if (option != nullptr) throw std::runtime_error(std::string(function) + ": unexpected option");
    {
        std::lock_guard lock(g_completionsMutex);
        Http2AsyncResult completion{};
        completion.req_id = req_id;
        completion.result = ERROR_NETWORK;
        g_completions[req_id].push_back(completion);
    }
    if (EqueueTriggerEvent_nid_postfix(kqueue_option->equeue, static_cast<uintptr_t>(kqueue_option->user_event_id), EVFILT_USER, kqueue_option->user_data) != 0) {
        std::lock_guard lock(g_completionsMutex);
        g_completions[req_id].pop_back();
        throw std::runtime_error(std::string(function) + ": failed to trigger equeue event");
    }
}

extern "C" {

int APS5_VABI sceHttp2AddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    (void)id;
    (void)name;
    (void)value;
    (void)mode;
    return 0;
}

int APS5_VABI sceHttp2CreateRequestWithURL(int tmpl_id, const char* method, const char* url, uint64_t content_length) {
    (void)tmpl_id;
    (void)method;
    (void)url;
    (void)content_length;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttp2CreateTemplate(int lib_http2_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)lib_http2_ctx_id;
    (void)user_agent;
    (void)http_ver;
    (void)is_auto_proxy_conf;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttp2DeleteRequest(int req_id) {
    std::lock_guard lock(g_completionsMutex);
    g_completions.erase(req_id);
    return 0;
}

int APS5_VABI sceHttp2DeleteTemplate(int tmpl_id) {
    (void)tmpl_id;
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
    std::lock_guard lock(g_poolsMutex);
    g_pools[id] = pool_size;
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
    CompleteAsync(__func__, req_id, kqueue_option, option);
    return 0;
}

int APS5_VABI sceHttp2SetAuthEnabled(int id, int is_enable) {
    (void)id;
    (void)is_enable;
    return 0;
}

int APS5_VABI sceHttp2SetAutoRedirect(int id, int enable) {
    (void)id;
    (void)enable;
    return 0;
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
    (void)id;
    (void)ssl_flags;
    return 0;
}

int APS5_VABI sceHttp2SslEnableOption(int id, uint32_t ssl_flags) {
    (void)id;
    (void)ssl_flags;
    return 0;
}

int APS5_VABI sceHttp2SetMinSslVersion(int id, uint32_t ssl_version) {
    (void)id;
    (void)ssl_version;
    return 0;
}

int APS5_VABI sceHttp2Term(int lib_http2_ctx_id) {
    std::lock_guard lock(g_poolsMutex);
    g_pools.erase(lib_http2_ctx_id);
    return 0;
}

int APS5_VABI sceHttp2WaitAsync(int req_id, Http2AsyncResult* result, uint32_t* timeout, void* option) {
    (void)timeout;
    if (result == nullptr) throw std::runtime_error("sceHttp2WaitAsync: null result");
    if (option != nullptr) throw std::runtime_error("sceHttp2WaitAsync: unexpected option");
    std::lock_guard lock(g_completionsMutex);
    const auto pending = g_completions.find(req_id);
    if (pending == g_completions.end() || pending->second.empty()) throw std::runtime_error("sceHttp2WaitAsync: waiting for an operation that has not completed");
    *result = pending->second.front();
    pending->second.pop_front();
    return 0;
}

int APS5_VABI sceHttp2AbortRequest(int req_id) {
    (void)req_id;
    return 0;
}

int APS5_VABI sceHttp2GetMemoryPoolStats(int lib_http2_ctx_id, Http2MemoryPoolStats* stats) {
    if (stats == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(g_poolsMutex);
    const auto pool = g_pools.find(lib_http2_ctx_id);
    if (pool == g_pools.end()) throw std::invalid_argument("sceHttp2GetMemoryPoolStats: unknown context");
    *stats = {pool->second, 0, 0, 0};
    return 0;
}

int APS5_VABI sceHttp2CookieFlush(int id) {
    (void)id;
    return 0;
}

int APS5_VABI sceHttp2CreateCookieBox(int lib_http2_ctx_id) {
    (void)lib_http2_ctx_id;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
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

int APS5_VABI sceHttp2WebSocketCreateRequest() {
    throw std::runtime_error("sceHttp2WebSocketCreateRequest: unknown signature");
}

int APS5_VABI sceHttp2WebSocketCloseAsync() {
    throw std::runtime_error("sceHttp2WebSocketCloseAsync: unknown signature");
}

int APS5_VABI sceHttp2WebSocketSendTextMessageAsync() {
    throw std::runtime_error("sceHttp2WebSocketSendTextMessageAsync: unknown signature");
}

int APS5_VABI sceHttp2WebSocketSendDataMessageAsync() {
    throw std::runtime_error("sceHttp2WebSocketSendDataMessageAsync: unknown signature");
}

}
