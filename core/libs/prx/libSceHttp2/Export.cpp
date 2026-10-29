#include "prx/libSceHttp2/include/Http2.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "Transport.hpp"
#include "Cookies.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <mutex>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

static constexpr int ERROR_INVALID_ID = static_cast<int>(0x817B1100);
static constexpr int ERROR_NULL_POINTER = static_cast<int>(0x817B1225);
static constexpr int ERROR_NOT_FOUND = static_cast<int>(0x817B1025);
static constexpr int ERROR_INVALID_VALUE = static_cast<int>(0x817B11FE);
static constexpr int ERROR_HTTP_INVALID_VALUE = static_cast<int>(0x804311FE);

enum class Http2Kind : std::uint8_t { Context, Template, Request, CookieBox };

struct Http2Handle {
    Http2Kind kind;
    int owner;
    std::size_t poolSize;
    std::shared_ptr<void> cookieMemory;
    int sslContext = 0;
    int version = 0;
    Http2::Settings settings;
    std::string method, url;
    std::shared_ptr<Http2::Request> transport;
    std::shared_ptr<Http2::Capacity> capacity;
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

static void ReleaseOwnedHandles(int owner) {
    for (;;) {
        auto child = g_handles.end();
        for (auto entry = g_handles.begin(); entry != g_handles.end(); ++entry) {
            if (entry->second.owner == owner) { child = entry; break; }
        }
        if (child == g_handles.end()) return;
        const int id = child->first;
        ReleaseOwnedHandles(id);
        auto& handle = g_handles.at(id);
        if (handle.transport) handle.transport->Stop();
        g_handles.erase(id);
    }
}

bool Http2CookieBoxExists(int cookieBoxId) {
    std::lock_guard lock(g_handlesMutex);
    return FindHandle(cookieBoxId, Http2Kind::CookieBox) != nullptr;
}

static bool HasSettings(const Http2Handle& handle) {
    return handle.kind == Http2Kind::Template || handle.kind == Http2Kind::Request;
}

template<class TUpdate>
static int Update(int id, const char* function, TUpdate update, bool otherKindsAreInvalid = false) {
    std::lock_guard lock(g_handlesMutex);
    auto* handle = FindAnyHandle(id);
    if (!handle) return ERROR_INVALID_ID;
    if (!HasSettings(*handle)) {
        if (otherKindsAreInvalid) return ERROR_INVALID_ID;
        NotImplemented_nid_no_patch(function);
    }
    if (handle->transport) NotImplemented_nid_no_patch(function);
    if constexpr (std::is_void_v<std::invoke_result_t<TUpdate, Http2::Settings&>>) {
        update(handle->settings);
        return 0;
    } else {
        return update(handle->settings);
    }
}

template<class TRead>
static int Read(int id, TRead read) {
    std::lock_guard lock(g_handlesMutex);
    const auto* handle = FindAnyHandle(id);
    if (!handle || !HasSettings(*handle)) return ERROR_INVALID_ID;
    return read(handle->settings);
}

static bool IsConnectionHeader(const char* name) {
    static constexpr char Connection[] = "connection";
    for (std::size_t i = 0; i < sizeof(Connection); ++i) {
        const char c = name[i] >= 'A' && name[i] <= 'Z' ? static_cast<char>(name[i] - 'A' + 'a') : name[i];
        if (c != Connection[i]) return false;
    }
    return true;
}

static std::shared_ptr<Http2::Request> Transport(int id, bool create) {
    std::lock_guard lock(g_handlesMutex);
    auto* handle = FindHandle(id, Http2Kind::Request);
    if (!handle) return nullptr;
    if (create && !handle->transport) {
        if (handle->version != 3) NotImplemented_nid_no_patch("sceHttp2SendRequest: HTTP/1.x transport is not implemented");
        handle->transport = Http2::CreateRequest_nid_no_patch(handle->sslContext, handle->capacity, handle->settings, handle->method, handle->url);
    }
    if (!handle->transport) NotImplemented_nid_no_patch("sceHttp2: request has not been sent");
    return handle->transport;
}


extern "C" {

int APS5_VABI sceHttp2AddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    if (mode > 1) return ERROR_INVALID_VALUE;
    if (name && IsConnectionHeader(name)) return ERROR_HTTP_INVALID_VALUE;
    return Update(id, __func__, [&](auto& settings) { return Http2::AddHeader(settings, name, value, mode); }, true);
}

int APS5_VABI sceHttp2CreateRequestWithURL(int tmpl_id, const char* method, const char* url, uint64_t content_length) {
    std::lock_guard lock(g_handlesMutex);
    const auto* parent = FindHandle(tmpl_id, Http2Kind::Template);
    if (!parent) return ERROR_INVALID_ID;
    if (!method || !url) NotImplemented_nid_no_patch(__func__);
    Http2Handle handle{Http2Kind::Request, tmpl_id, 0};
    handle.sslContext = parent->sslContext;
    handle.capacity = parent->capacity;
    handle.version = parent->version;
    handle.settings = parent->settings;
    handle.settings.contentLength = content_length;
    handle.method = method;
    handle.url = url;
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    g_handles.emplace(id, std::move(handle));
    return id;
}

int APS5_VABI sceHttp2CreateTemplate(int lib_http2_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    std::lock_guard lock(g_handlesMutex);
    const auto* parent = FindHandle(lib_http2_ctx_id, Http2Kind::Context);
    if (!parent) return ERROR_INVALID_ID;
    if (!user_agent) NotImplemented_nid_no_patch(__func__);
    if ((is_auto_proxy_conf != 0 && is_auto_proxy_conf != 1) || http_ver < 1 || http_ver > 3) NotImplemented_nid_no_patch(__func__);
    Http2Handle handle{Http2Kind::Template, lib_http2_ctx_id, 0};
    handle.sslContext = parent->sslContext;
    handle.capacity = parent->capacity;
    handle.version = http_ver;
    handle.settings = parent->settings;
    handle.settings.userAgent = user_agent;
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    g_handles.emplace(id, std::move(handle));
    return id;
}

int APS5_VABI sceHttp2DeleteRequest(int req_id) {
    std::lock_guard lock(g_handlesMutex);
    auto* handle = FindHandle(req_id, Http2Kind::Request);
    if (!handle) return ERROR_INVALID_ID;
    if (handle->transport) handle->transport->Stop();
    g_handles.erase(req_id);
    return 0;
}

int APS5_VABI sceHttp2DeleteTemplate(int tmpl_id) {
    std::lock_guard lock(g_handlesMutex);
    if (!FindHandle(tmpl_id, Http2Kind::Template)) return ERROR_INVALID_ID;
    ReleaseOwnedHandles(tmpl_id);
    g_handles.erase(tmpl_id);
    return 0;
}

int APS5_VABI sceHttp2GetAllResponseHeaders(int req_id, char** header, size_t* header_size) {
    if (!Transport(req_id, false)) return ERROR_INVALID_ID;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2GetResponseContentLength(int req_id, int* result, uint64_t* content_length) {
    const auto request = Transport(req_id, false);
    return request ? request->ContentLength(result, content_length) : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2GetStatusCode(int req_id, int* status_code) {
    const auto request = Transport(req_id, false);
    return request ? request->Status(status_code) : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2Init(int libnet_mem_id, int libssl_ctx_id, size_t pool_size, int max_concurrent_request) {
    if (max_concurrent_request == 0) return ERROR_INVALID_VALUE;
    if (max_concurrent_request < 0) NotImplemented_nid_no_patch(__func__);
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_handlesMutex);
    Http2Handle handle{Http2Kind::Context, 0, pool_size};
    handle.sslContext = libssl_ctx_id;
    handle.capacity = Http2::CreateCapacity_nid_no_patch(max_concurrent_request);
    g_handles.emplace(id, std::move(handle));
    return id;
}

int APS5_VABI sceHttp2ReadData(int req_id, void* data, size_t size) {
    const auto request = Transport(req_id, false);
    return request ? request->Read(data, size) : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2ReadDataAsync(int req_id, void* data, size_t size, void* kqueue_option, void* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SendRequest(int req_id, const void* post_data, size_t size) {
    const auto request = Transport(req_id, true);
    return request ? request->Send(post_data, size) : ERROR_INVALID_ID;
}

int APS5_VABI sceHttp2SendRequestAsync(int req_id, const void* post_data, size_t size, Http2AsyncOption* kqueue_option, void* option) {
    std::lock_guard lock(g_handlesMutex);
    if (!FindHandle(req_id, Http2Kind::Request)) return ERROR_INVALID_ID;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetAuthEnabled(int id, int is_enable) {
    return Update(id, __func__, [&](auto& settings) { settings.auth = is_enable != 0; }, true);
}

int APS5_VABI sceHttp2SetAutoRedirect(int id, int enable) {
    return Update(id, __func__, [&](auto& settings) { settings.redirect = enable; }, true);
}

int APS5_VABI sceHttp2SetConnectionWaitTimeOut(int id, uint32_t usec) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2SetConnectTimeOut(int id, uint32_t usec) {
    return Update(id, __func__, [&](auto& settings) {
        if (!usec) NotImplemented_nid_no_patch("sceHttp2SetConnectTimeOut");
        settings.connectTimeout = usec;
    });
}

int APS5_VABI sceHttp2SetInflateGZIPEnabled(int id, int enable) {
    return Update(id, __func__, [&](auto&) { if (enable != 0) NotImplemented_nid_no_patch("sceHttp2SetInflateGZIPEnabled"); });
}

int APS5_VABI sceHttp2SetRecvTimeOut(int id, uint32_t usec) {
    return Update(id, __func__, [&](auto& settings) {
        if (!usec) NotImplemented_nid_no_patch("sceHttp2SetRecvTimeOut");
        settings.recvTimeout = usec;
    });
}

int APS5_VABI sceHttp2SetRedirectCallback(int id, void* cb_func, void* user_arg) {
    return Update(id, __func__, [&](auto&) { if (cb_func || user_arg) NotImplemented_nid_no_patch("sceHttp2SetRedirectCallback"); });
}

int APS5_VABI sceHttp2SetRequestContentLength(int id, uint64_t content_length) {
    return Update(id, __func__, [&](auto& settings) { settings.contentLength = content_length; settings.noContentLength = false; });
}

int APS5_VABI sceHttp2SetResolveTimeOut(int id, uint32_t usec) {
    return Update(id, __func__, [&](auto& settings) {
        if (!usec) NotImplemented_nid_no_patch("sceHttp2SetResolveTimeOut");
        settings.resolveTimeout = usec;
    });
}

int APS5_VABI sceHttp2SetSendTimeOut(int id, uint32_t usec) {
    return Update(id, __func__, [&](auto& settings) {
        if (!usec) NotImplemented_nid_no_patch("sceHttp2SetSendTimeOut");
        settings.sendTimeout = usec;
    });
}

int APS5_VABI sceHttp2SetSslCallback(int id, void* cb_func, void* user_arg) {
    return Update(id, __func__, [&](auto&) { if (cb_func || user_arg) NotImplemented_nid_no_patch("sceHttp2SetSslCallback"); });
}

int APS5_VABI sceHttp2SetTimeOut(int id, uint32_t usec) {
    return Update(id, __func__, [&](auto& settings) {
        if (!usec) NotImplemented_nid_no_patch("sceHttp2SetTimeOut");
        settings.timeout = usec;
    });
}

int APS5_VABI sceHttp2SslDisableOption(int id, uint32_t ssl_flags) {
    return Update(id, __func__, [&](auto& settings) {
        if (ssl_flags & ~0x3du) NotImplemented_nid_no_patch("sceHttp2SslDisableOption");
        settings.verifyFlags &= ~ssl_flags;
    });
}

int APS5_VABI sceHttp2SslEnableOption(int id, uint32_t ssl_flags) {
    return Update(id, __func__, [&](auto& settings) {
        if (ssl_flags & ~0x3du) NotImplemented_nid_no_patch("sceHttp2SslEnableOption");
        settings.verifyFlags |= ssl_flags;
    });
}

int APS5_VABI sceHttp2SetMinSslVersion(int id, uint32_t ssl_version) {
    return Update(id, __func__, [&](auto&) { NotImplemented_nid_no_patch("sceHttp2SetMinSslVersion"); });
}

int APS5_VABI sceHttp2Term(int lib_http2_ctx_id) {
    std::lock_guard lock(g_handlesMutex);
    if (!FindHandle(lib_http2_ctx_id, Http2Kind::Context)) return ERROR_INVALID_ID;
    ReleaseOwnedHandles(lib_http2_ctx_id);
    g_handles.erase(lib_http2_ctx_id);
    return 0;
}

int APS5_VABI sceHttp2WaitAsync(int req_id, Http2AsyncResult* result, uint32_t* timeout, void* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2AbortRequest(int req_id) {
    const auto request = Transport(req_id, false);
    if (!request) return ERROR_INVALID_ID;
    request->Cancel();
    return 0;
}

int APS5_VABI sceHttp2GetMemoryPoolStats(int lib_http2_ctx_id, Http2MemoryPoolStats* stats) {
    if (stats == nullptr) return ERROR_NULL_POINTER;
    std::lock_guard lock(g_handlesMutex);
    const auto* context = FindHandle(lib_http2_ctx_id, Http2Kind::Context);
    if (!context) {
        std::memset(stats, 0, sizeof(*stats));
        return ERROR_INVALID_ID;
    }
    *stats = {context->poolSize, 0, 0, 0};
    return 0;
}

int APS5_VABI sceHttp2CookieFlush(int id) {
    Http2FlushCookies(id);
    return 0;
}

int APS5_VABI sceHttp2CreateCookieBox(std::size_t poolSize) {
    if (poolSize < 0x10000) return static_cast<int>(0x817b121du);
    const auto mappedSize = (poolSize + 0x3fff) & ~std::size_t{0x3fff};
    void* address = nullptr;
    if (const int result = sceKernelMapNamedFlexibleMemory(&address, mappedSize, 3, 0, "SceHttp2CookieBox"); result != 0) return result;
    std::shared_ptr<void> memory(address, [mappedSize](void* mapped) { sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapped), mappedSize); });
    std::lock_guard lock(g_handlesMutex);
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    g_handles.emplace(id, Http2Handle{Http2Kind::CookieBox, 0, poolSize, std::move(memory)});
    return id;
}

int APS5_VABI sceHttp2DeleteCookieBox(int cookie_box_id) {
    {
        std::lock_guard lock(g_handlesMutex);
        if (!FindHandle(cookie_box_id, Http2Kind::CookieBox)) return ERROR_INVALID_ID;
        g_handles.erase(cookie_box_id);
    }
    Http2ForgetCookies(cookie_box_id);
    return 0;
}

int APS5_VABI sceHttp2SetCookieBox(int id, int cookie_box_id) {
    return Update(id, __func__, [&](auto&) { if (cookie_box_id != 0) NotImplemented_nid_no_patch("sceHttp2SetCookieBox"); });
}

int APS5_VABI sceHttp2SetRequestNoContentLength(int id) {
    return Update(id, __func__, [&](auto& settings) { settings.noContentLength = true; });
}

int APS5_VABI sceHttp2SetResolveRetry(int id, int32_t retry) {
    return Update(id, __func__, [&](auto&) { if (retry != 0) NotImplemented_nid_no_patch("sceHttp2SetResolveRetry"); });
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
int APS5_VABI sceHttp2AuthCacheFlush(int lib_http2_ctx_id) {
    std::lock_guard lock(g_handlesMutex);
    return FindHandle(lib_http2_ctx_id, Http2Kind::Context) ? 0 : ERROR_INVALID_ID;
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
    return Read(id, [&](const auto& settings) {
        if (!is_enable) throw std::invalid_argument("sceHttp2GetAuthEnabled: null out-pointer");
        *is_enable = settings.auth ? 1 : 0;
        return 0;
    });
}

int APS5_VABI sceHttp2GetAutoRedirect(int id, int* enable) {
    return Read(id, [&](const auto& settings) {
        if (!enable) return ERROR_INVALID_VALUE;
        *enable = settings.redirect;
        return 0;
    });
}

int APS5_VABI sceHttp2GetCookieBox(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttp2RedirectCacheFlush(int lib_http2_ctx_id) {
    (void)lib_http2_ctx_id;
    return 0;
}

int APS5_VABI sceHttp2RemoveRequestHeader(int id, const char* name) {
    if (!name) return ERROR_NOT_FOUND;
    std::string key = name;
    for (char& c : key) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return Update(id, __func__, [&](auto& settings) {
        return std::erase_if(settings.headers, [&](const auto& header) { return header.first == key; }) ? 0 : ERROR_NOT_FOUND;
    }, true);
}

int APS5_VABI sceHttp2SetAuthInfoCallback(int id, Http2AuthInfoCallback cb_func, void* user_arg) {
    return Update(id, __func__, [&](auto& settings) {
        settings.authInfoCallback = reinterpret_cast<void*>(cb_func);
        settings.authInfoArgument = user_arg;
    }, true);
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
    return Update(id, __func__, [&](auto& settings) {
        settings.preSendCallback = cb_func;
        settings.preSendArgument = user_arg;
    }, true);
}

}
