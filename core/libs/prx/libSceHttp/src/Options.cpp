#include "prx/libSceHttp/include/Http.hpp"
#include "prx/libSceHttp/src/Options.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceHttp/src/GuestAccess.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace {

constexpr std::uint32_t SocketCreationCallbackSdkVersion = 0x01500000u;

std::uint32_t CompiledSdkVersion() {
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t sdkVersion;
    std::memcpy(&sdkVersion, process + 16, sizeof(sdkVersion));
    return sdkVersion;
}

constexpr std::uint32_t PrivateSslFlags = 0x2dffu;
constexpr std::uint32_t ResolveTimeoutMinimumSdkVersion = 0x01700000u;
constexpr std::uint32_t MaximumRejectedResolveTimeoutUs = 999999u;

std::mutex g_optionsMutex;
std::unordered_map<int, bool> g_contextAcceptEncodingGzip;
std::unordered_map<int, HttpOptions> g_objectOptions;

}

void AddHttpContext(int contextId) {
    std::lock_guard lock(g_optionsMutex);
    g_contextAcceptEncodingGzip[contextId] = false;
}

void RemoveHttpContext(int contextId) {
    std::unique_lock lock(g_optionsMutex);
    g_contextAcceptEncodingGzip.erase(contextId);
    if (g_contextAcceptEncodingGzip.empty()) g_objectOptions.clear();
    lock.unlock();
    RemoveHttpCookieJar(contextId);
}

bool HttpInitialised() {
    std::lock_guard lock(g_optionsMutex);
    return !g_contextAcceptEncodingGzip.empty();
}

bool IsHttpContext(int contextId) {
    std::lock_guard lock(g_optionsMutex);
    return g_contextAcceptEncodingGzip.contains(contextId);
}

void AddHttpTemplate(int templateId, int contextId) {
    std::lock_guard lock(g_optionsMutex);
    HttpOptions options;
    if (const auto context = g_contextAcceptEncodingGzip.find(contextId); context != g_contextAcceptEncodingGzip.end()) options.acceptEncodingGzip = context->second;
    g_objectOptions[templateId] = options;
}

void AddHttpChild(int id, int parentId, bool request) {
    std::lock_guard lock(g_optionsMutex);
    const auto parent = g_objectOptions.find(parentId);
    HttpOptions options = parent != g_objectOptions.end() ? parent->second : HttpOptions{};
    options.request = request;
    g_objectOptions[id] = options;
}

void RemoveHttpObject(int id) {
    std::lock_guard lock(g_optionsMutex);
    g_objectOptions.erase(id);
}

std::optional<HttpOptions> FindHttpOptions(int id) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end()) return std::nullopt;
    return found->second;
}

bool SetHttpOption(int id, bool HttpOptions::*option, bool enable) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end()) return false;
    found->second.*option = enable;
    return true;
}

template <class T>
bool SetHttpValue(int id, T HttpOptions::*option, T value) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end()) return false;
    found->second.*option = value;
    return true;
}

template bool SetHttpValue<std::uint32_t>(int, std::uint32_t HttpOptions::*, std::uint32_t);
template bool SetHttpValue<std::uint64_t>(int, std::uint64_t HttpOptions::*, std::uint64_t);
template bool SetHttpValue<int>(int, int HttpOptions::*, int);

bool SetHttpRequestContentLength(int id, std::uint64_t length) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end() || !found->second.request) return false;
    found->second.requestContentLength = length;
    return true;
}

bool SetHttpCookieEnabled(int id, int enable) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end()) return false;
    found->second.cookieEnabled = enable;
    return true;
}

bool SetHttpEpollId(int id, int epollId) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end()) return false;
    found->second.epollId = epollId;
    return true;
}

bool SetHttpContextAcceptEncodingGzip(int contextId, bool enable) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_contextAcceptEncodingGzip.find(contextId);
    if (found == g_contextAcceptEncodingGzip.end()) return false;
    found->second = enable;
    return true;
}

bool SetHttpEpollBinding(int id, HttpEpollHandle epoll, void* argument) {
    std::lock_guard lock(g_optionsMutex);
    const auto found = g_objectOptions.find(id);
    if (found == g_objectOptions.end()) return false;
    found->second.epoll = epoll;
    found->second.epollArgument = argument;
    return true;
}

extern "C" {

int APS5_VABI sceHttpGetAutoRedirect(int id, int* enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!enable) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(id);
    if (!options) return ERROR_INVALID_ID;
    if (!HttpGuest::Store(enable, options->autoRedirect ? 1 : 0)) return ERROR_INVALID_VALUE;
    return 0;
}

int APS5_VABI sceHttpGetNonblock(int id, int* enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!enable) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(id);
    if (!options) return ERROR_INVALID_ID;
    if (!HttpGuest::Store(enable, options->nonblock ? 1 : 0)) return ERROR_INVALID_VALUE;
    return 0;
}

int APS5_VABI sceHttpTryGetNonblock(int id, int* enable) {
    return sceHttpGetNonblock(id, enable);
}

int APS5_VABI sceHttpGetAcceptEncodingGZIPEnabled(int id, int* enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!enable) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(id);
    if (!options) return ERROR_INVALID_ID;
    if (!HttpGuest::Store(enable, options->acceptEncodingGzip ? 1 : 0)) return ERROR_INVALID_VALUE;
    return 0;
}

int APS5_VABI sceHttpSetAcceptEncodingGZIPEnabled(int id, int enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpOption(id, &HttpOptions::acceptEncodingGzip, enable != 0) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetSendTimeOut(int id, std::uint32_t usec) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpValue(id, &HttpOptions::sendTimeoutUs, usec) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetRecvTimeOut(int id, std::uint32_t usec) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpValue(id, &HttpOptions::recvTimeoutUs, usec) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetResolveTimeOut(int id, std::uint32_t usec) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (CompiledSdkVersion() >= ResolveTimeoutMinimumSdkVersion && usec <= MaximumRejectedResolveTimeoutUs) return ERROR_INVALID_VALUE;
    return SetHttpValue(id, &HttpOptions::resolveTimeoutUs, usec) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetResolveRetry(int id, std::int32_t retry) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (retry < 0) return ERROR_INVALID_VALUE;
    return SetHttpValue(id, &HttpOptions::resolveRetry, retry) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetRecvBlockSize(int id, std::uint32_t blockSize) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpValue(id, &HttpOptions::recvBlockSize, blockSize) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetResponseHeaderMaxSize(int id, std::uint64_t headerSize) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpValue(id, &HttpOptions::responseHeaderMaxSize, headerSize) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpSetRequestContentLength(int id, std::uint64_t contentLength) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpRequestContentLength(id, contentLength) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpGetEpoll(int id, HttpEpollHandle* epoll, void** argument) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!epoll) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(id);
    if (!options) return ERROR_INVALID_ID;
    if (!HttpGuest::Store(epoll, options->epoll)) return ERROR_INVALID_VALUE;
    if (argument && !HttpGuest::Store(argument, options->epollArgument)) return ERROR_INVALID_VALUE;
    return 0;
}

int APS5_VABI sceHttpCacheRedirectedConnectionEnabled(int id, int enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    (void)enable;
    return FindHttpOptions(id) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpsEnableOptionPrivate(int id, std::uint32_t sslFlags) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if ((sslFlags & ~PrivateSslFlags) != 0) return ERROR_INVALID_VALUE;
    return FindHttpOptions(id) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpsDisableOptionPrivate(int id, std::uint32_t sslFlags) {
    return sceHttpsEnableOptionPrivate(id, sslFlags);
}

int APS5_VABI sceHttpSetSocketCreationCallback(std::int32_t id, HttpSocketCreationCallback callback, void* userArg) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    (void)callback;
    (void)userArg;
    if (!FindHttpOptions(id)) return ERROR_INVALID_ID;
    return CompiledSdkVersion() < SocketCreationCallbackSdkVersion ? ERROR_INVALID_VALUE : 0;
}

}
