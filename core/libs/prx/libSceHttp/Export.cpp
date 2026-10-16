#include <cstdint>
#include <cstddef>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static std::atomic<int> g_nextHandle{1};
using HttpRequestStatusCallback = void (APS5_VABI *)(int, int, void*);
struct RequestStatusRegistration {
    HttpRequestStatusCallback callback;
    void* argument;
};
static std::mutex g_statusMutex;
static std::map<int, RequestStatusRegistration> g_statusCallbacks;
struct HttpsCaList {
    void* certs;
    int certsNum;
};

#ifdef _WIN32
struct HostMutex {
    SRWLOCK native = SRWLOCK_INIT;

    HostMutex() = default;
    HostMutex(const HostMutex&) = delete;
    HostMutex& operator=(const HostMutex&) = delete;

    void lock() { AcquireSRWLockExclusive(&native); }
    void unlock() { ReleaseSRWLockExclusive(&native); }
};
#else
using HostMutex = std::mutex;
#endif

class HostCondition {
public:
    HostCondition() = default;
    HostCondition(const HostCondition&) = delete;
    HostCondition& operator=(const HostCondition&) = delete;

    void NotifyAll() {
#ifdef _WIN32
        WakeAllConditionVariable(&native);
#else
        native.notify_all();
#endif
    }

    void Wait(std::unique_lock<HostMutex>& lock) {
#ifdef _WIN32
        if (!SleepConditionVariableSRW(&native, &lock.mutex()->native, INFINITE, 0))
            throw std::runtime_error("libSceHttp: native condition wait failed");
#else
        native.wait(lock);
#endif
    }

    void WaitUntil(std::unique_lock<HostMutex>& lock, std::chrono::steady_clock::time_point deadline) {
#ifdef _WIN32
        const auto now = std::chrono::steady_clock::now();
        if (deadline <= now) return;
        const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
        if (!SleepConditionVariableSRW(&native, &lock.mutex()->native, static_cast<DWORD>(milliseconds), 0) &&
            GetLastError() != ERROR_TIMEOUT)
            throw std::runtime_error("libSceHttp: native condition wait failed");
#else
        native.wait_until(lock, deadline);
#endif
    }

private:
#ifdef _WIN32
    CONDITION_VARIABLE native = CONDITION_VARIABLE_INIT;
#else
    std::condition_variable native;
#endif
};

struct HttpEpoll {
    HostMutex mutex;
    HostCondition wake;
    bool aborted = false;
    bool destroyed = false;
};

static std::mutex g_stateMutex;
static std::unordered_set<int> g_contexts;
static std::unordered_map<HttpEpollHandle, std::shared_ptr<HttpEpoll>> g_epolls;

static std::shared_ptr<HttpEpoll> FindEpoll(HttpEpollHandle eh) {
    const auto it = g_epolls.find(eh);
    return it == g_epolls.end() ? nullptr : it->second;
}

static void MarkDestroyed(HttpEpoll& epoll) {
    std::lock_guard lock(epoll.mutex);
    epoll.destroyed = true;
    epoll.wake.NotifyAll();
}

extern "C" {

int APS5_VABI sceHttpAbortRequest(int request_id) {
    (void)request_id;
    return 0;
}

int APS5_VABI sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    (void)id;
    (void)name;
    (void)value;
    (void)mode;
    return 0;
}

int APS5_VABI sceHttpCreateConnection(int tmpl_id, const char* server_name, const char* scheme, uint16_t port, int enable_keep_alive) {
    (void)tmpl_id;
    (void)server_name;
    (void)scheme;
    (void)port;
    (void)enable_keep_alive;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpCreateConnectionWithURL(int tmpl_id, const char* url, int enable_keep_alive) {
    (void)tmpl_id;
    (void)url;
    (void)enable_keep_alive;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpCreateEpoll(int http_ctx_id, HttpEpollHandle* eh) {
    std::lock_guard lock(g_stateMutex);
    if (g_contexts.empty()) return ERROR_BEFORE_INIT;
    if (!g_contexts.contains(http_ctx_id)) return ERROR_INVALID_ID;
    if (!eh) return ERROR_INVALID_VALUE;
    auto epoll = std::make_shared<HttpEpoll>();
    *eh = epoll.get();
    g_epolls.emplace(epoll.get(), std::move(epoll));
    return 0;
}

int APS5_VABI sceHttpCreateRequest(int conn_id, int method, const char* path, uint64_t content_length) {
    (void)conn_id;
    (void)method;
    (void)path;
    (void)content_length;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpCreateRequestWithURL2(int conn_id, const char* method, const char* url, uint64_t content_length) {
    (void)conn_id;
    (void)method;
    (void)url;
    (void)content_length;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpCreateTemplate(int http_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)http_ctx_id;
    (void)user_agent;
    (void)http_ver;
    (void)is_auto_proxy_conf;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpDeleteConnection(int conn_id) {
    (void)conn_id;
    return 0;
}

int APS5_VABI sceHttpDeleteRequest(int req_id) {
    std::lock_guard lock(g_statusMutex);
    g_statusCallbacks.erase(req_id);
    return 0;
}

int APS5_VABI sceHttpDeleteTemplate(int tmpl_id) {
    (void)tmpl_id;
    return 0;
}

int APS5_VABI sceHttpDestroyEpoll(int http_ctx_id, HttpEpollHandle eh) {
    std::shared_ptr<HttpEpoll> epoll;
    {
        std::lock_guard lock(g_stateMutex);
        if (g_contexts.empty()) return ERROR_BEFORE_INIT;
        if (!g_contexts.contains(http_ctx_id)) return ERROR_INVALID_ID;
        if (!eh) return ERROR_INVALID_VALUE;
        epoll = FindEpoll(eh);
        if (!epoll) return ERROR_INVALID_ID;
        g_epolls.erase(eh);
    }
    MarkDestroyed(*epoll);
    return 0;
}

int APS5_VABI sceHttpGetAllResponseHeaders(int request_id, char** header, size_t* header_size) {
    (void)request_id;
    (void)header;
    (void)header_size;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttpGetResponseContentLength(int request_id, int* result, uint64_t* content_length) {
    (void)request_id;
    (void)result;
    (void)content_length;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttpGetStatusCode(int request_id, int* status_code) {
    (void)request_id;
    (void)status_code;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttpInit_nid_postfix(int memid, int ssl_ctx_id, uint64_t pool_size) {
    (void)memid;
    (void)ssl_ctx_id;
    (void)pool_size;
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_stateMutex);
    g_contexts.insert(id);
    return id;
}

int APS5_VABI sceHttpsDisableOption(int id, uint32_t ssl_flags) {
    (void)id;
    (void)ssl_flags;
    return 0;
}

int APS5_VABI sceHttpSendRequest(int request_id, const void* post_data, size_t size) {
    (void)request_id;
    (void)post_data;
    (void)size;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttpSetAuthEnabled(int id, int enable) {
    (void)id;
    (void)enable;
    return 0;
}

int APS5_VABI sceHttpSetAuthInfoCallback(int id, HttpAuthInfoCallback callback, void* userArg) {
    (void)id;
    (void)callback;
    (void)userArg;
    return 0;
}

int APS5_VABI sceHttpSetCookieEnabled(int id, int enable) {
    (void)id;
    if (static_cast<uint32_t>(enable) > 1) return ERROR_INVALID_VALUE;
    return 0;
}

int APS5_VABI sceHttpSetAutoRedirect(int id, int enable) {
    (void)id;
    (void)enable;
    return 0;
}

int APS5_VABI sceHttpSetConnectTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttpSetEpoll(int id, HttpEpollHandle eh, void* user_arg) {
    (void)id;
    (void)eh;
    (void)user_arg;
    return 0;
}

int APS5_VABI sceHttpSetNonblock(int id, int enable) {
    (void)id;
    (void)enable;
    return 0;
}

int APS5_VABI sceHttpSetRecvTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttpSetRecvBlockSize(int id, uint32_t block_size) {
    (void)id;
    (void)block_size;
    return 0;
}

int APS5_VABI sceHttpSetResponseHeaderMaxSize(int id, uint64_t header_size) {
    (void)id;
    (void)header_size;
    return 0;
}

int APS5_VABI sceHttpSetRequestContentLength(int request_id, uint64_t content_length) {
    (void)request_id;
    (void)content_length;
    return 0;
}

int APS5_VABI sceHttpSetResolveRetry(int id, int32_t retry) {
    (void)id;
    (void)retry;
    return 0;
}

int APS5_VABI sceHttpSetResolveTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttpSetSendTimeOut(int id, uint32_t usec) {
    (void)id;
    (void)usec;
    return 0;
}

int APS5_VABI sceHttpsSetMinSslVersion(int id, uint32_t ssl_version) {
    (void)id;
    (void)ssl_version;
    return 0;
}

int APS5_VABI sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* user_arg) {
    (void)id;
    (void)cbfunc;
    (void)user_arg;
    return 0;
}

int APS5_VABI sceHttpTerm_nid_postfix(int http_ctx_id) {
    std::vector<std::shared_ptr<HttpEpoll>> epolls;
    {
        std::lock_guard lock(g_stateMutex);
        if (g_contexts.empty()) return ERROR_BEFORE_INIT;
        if (g_contexts.erase(http_ctx_id) == 0) return ERROR_INVALID_ID;
        if (!g_contexts.empty()) return 0;
        for (auto& [handle, epoll] : g_epolls) epolls.push_back(std::move(epoll));
        g_epolls.clear();
    }
    for (const auto& epoll : epolls) MarkDestroyed(*epoll);
    return 0;
}

int APS5_VABI sceHttpUnsetEpoll(int id) {
    (void)id;
    return 0;
}

int APS5_VABI sceHttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents, int timeout) {
    std::shared_ptr<HttpEpoll> epoll;
    {
        std::lock_guard lock(g_stateMutex);
        if (g_contexts.empty()) return ERROR_BEFORE_INIT;
        if (!eh || !nbev || maxevents <= 0) return ERROR_INVALID_VALUE;
        epoll = FindEpoll(eh);
        if (!epoll) return ERROR_INVALID_ID;
    }
    std::unique_lock lock(epoll->mutex);
    const auto woken = [&] { return epoll->aborted || epoll->destroyed; };
    if (timeout < 0) {
        while (!woken()) epoll->wake.Wait(lock);
    } else {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(timeout);
        while (!woken() && std::chrono::steady_clock::now() < deadline) epoll->wake.WaitUntil(lock, deadline);
    }
    return epoll->aborted ? ERROR_ABORTED : 0;
}

int APS5_VABI sceHttpCreateRequestWithURL(int conn_id, int method, const char* url, uint64_t content_length) {
    (void)conn_id;
    (void)method;
    (void)url;
    (void)content_length;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpReadData(int request_id, void* data, size_t size) {
    (void)request_id;
    (void)data;
    (void)size;
    return ERROR_NETWORK;
}

int APS5_VABI sceHttpRedirectCacheFlush(int http_ctx_id) {
    (void)http_ctx_id;
    return 0;
}

int APS5_VABI sceHttpSetChunkedTransferEnabled(int id, int enable) {
    (void)id;
    (void)enable;
    return 0;
}


int APS5_VABI sceHttpSetInflateGZIPEnabled(int id, int enable) {
    (void)id;
    if (static_cast<uint32_t>(enable) > 1) return ERROR_INVALID_VALUE;
    return 0;
}

int APS5_VABI sceHttpSetRequestStatusCallback(int id, HttpRequestStatusCallback callback, void* argument) {
    if (id <= 0 || id >= g_nextHandle.load(std::memory_order_relaxed)) return static_cast<int>(0x80431100u);
    std::lock_guard lock(g_statusMutex);
    if (callback) g_statusCallbacks[id] = {callback, argument};
    else g_statusCallbacks.erase(id);
    return 0;
}

int APS5_VABI sceHttpParseResponseHeader(const char* header, std::size_t headerLen, const char* fieldStr,
                                       const char** fieldValue, std::size_t* valueLen) {
    constexpr int invalidResponse = static_cast<int>(0x80432060);
    constexpr int invalidValue = static_cast<int>(0x804321FE);
    constexpr int notFound = static_cast<int>(0x80432025);
    if (!header) return invalidResponse;
    if (!fieldStr || !fieldValue || !valueLen) return invalidValue;

    std::size_t fieldLen = 0;
    while (fieldLen < 0xfff && fieldStr[fieldLen] != '\0') ++fieldLen;
    const std::string_view input(header, headerLen);
    const auto isSpace = [](char c) { return c == ' ' || (c >= '\t' && c <= '\r'); };
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    std::size_t start = 0;
    bool found = false;
    while (start < headerLen) {
        if (!isSpace(input[start]) && fieldLen < headerLen - start && input[start + fieldLen] == ':') {
            std::size_t i = 0;
            while (i < fieldLen && lower(input[start + i]) == lower(fieldStr[i])) ++i;
            if (i == fieldLen) {
                start += fieldLen + 1;
                found = true;
                break;
            }
        }
        const auto newline = input.find('\n', start);
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    if (!found) return notFound;

    while (start < headerLen && isSpace(input[start])) {
        if (input[start++] == '\n') break;
    }
    std::size_t end = headerLen;
    std::size_t consumed = headerLen;
    std::size_t scan = start;
    while (scan < headerLen) {
        const auto newline = input.find('\n', scan);
        if (newline == std::string_view::npos) break;
        const auto next = newline + 1;
        if (next < headerLen && (input[next] == ' ' || input[next] == '\t')) {
            scan = next;
            continue;
        }
        end = newline > start && input[newline - 1] == '\r' ? newline - 1 : newline;
        consumed = next;
        break;
    }
    if (end == start) {
        *fieldValue = nullptr;
        *valueLen = 0;
        return 0;
    }
    if (consumed > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::out_of_range("sceHttpParseResponseHeader: consumed byte count exceeds int");
    }
    *fieldValue = header + start;
    *valueLen = end - start;
    return static_cast<int>(consumed);
}

int APS5_VABI sceHttpCreateRequest2(int conn_id, const char* method, const char* path, uint64_t content_length) {
    (void)conn_id;
    (void)method;
    (void)path;
    (void)content_length;
    return g_nextHandle.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceHttpsEnableOption(int id, uint32_t ssl_flags) {
    (void)id;
    (void)ssl_flags;
    return 0;
}

int APS5_VABI sceHttpsLoadCert(int http_ctx_id, int num, void* ca_list, void* cert, void* key) {
    (void)http_ctx_id;
    (void)num;
    (void)ca_list;
    (void)cert;
    (void)key;
    return 0;
}

int APS5_VABI sceHttpsUnloadCert(int http_ctx_id) {
    (void)http_ctx_id;
    return 0;
}

int APS5_VABI sceHttpGetLastErrno(int request_id, int* errno_out) {
    (void)request_id;
    if (errno_out == nullptr) {
        return ERROR_INVALID_VALUE;
    }
    *errno_out = 0;
    return 0;
}

int APS5_VABI sceHttpsGetSslError(int id, int* err_num, uint32_t* detail) {
    (void)id;
    if (err_num == nullptr || detail == nullptr) {
        return ERROR_INVALID_VALUE;
    }
    *err_num = 0;
    *detail = 0;
    return 0;
}

int APS5_VABI sceHttpSetCookieRecvCallback(int id, HttpCookieRecvCallback cbfunc, void* user_arg) {
    (void)id;
    (void)cbfunc;
    (void)user_arg;
    return 0;
}

int APS5_VABI sceHttpsSetSslVersion(int id, int ssl_version) {
    (void)id;
    (void)ssl_version;
    return 0;
}

int APS5_VABI sceHttpSetRedirectCallback(int id, HttpRedirectCallback cbfunc, void* user_arg) {
    (void)id;
    (void)cbfunc;
    (void)user_arg;
    return 0;
}

int APS5_VABI sceHttpAbortWaitRequest(HttpEpollHandle eh) {
    std::shared_ptr<HttpEpoll> epoll;
    {
        std::lock_guard lock(g_stateMutex);
        if (g_contexts.empty()) return ERROR_BEFORE_INIT;
        if (!eh) return ERROR_INVALID_VALUE;
        epoll = FindEpoll(eh);
        if (!epoll) return ERROR_INVALID_ID;
    }
    std::lock_guard lock(epoll->mutex);
    epoll->aborted = true;
    epoll->wake.NotifyAll();
    return 0;
}

int APS5_VABI sceHttpsGetCaList(int http_ctx_id, HttpsCaList* ca_list) {
    (void)http_ctx_id;
    if (!ca_list) return ERROR_INVALID_VALUE;
    ca_list->certs = nullptr;
    ca_list->certsNum = 0;
    return 0;
}

int APS5_VABI sceHttpsFreeCaList(int http_ctx_id, HttpsCaList* ca_list) {
    (void)http_ctx_id;
    if (!ca_list) return ERROR_INVALID_VALUE;
    ca_list->certs = nullptr;
    ca_list->certsNum = 0;
    return 0;
}
}
