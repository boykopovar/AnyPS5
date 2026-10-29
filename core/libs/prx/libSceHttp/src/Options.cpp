#include "prx/libSceHttp/include/Http.hpp"
#include "prx/libSceHttp/src/Options.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace {

constexpr std::uint32_t PrivateSslFlags = 0x2dffu;

std::mutex g_optionsMutex;
std::unordered_map<int, bool> g_contextAcceptEncodingGzip;
std::unordered_map<int, HttpOptions> g_objectOptions;

}

void AddHttpContext(int contextId) {
    std::lock_guard lock(g_optionsMutex);
    g_contextAcceptEncodingGzip[contextId] = false;
}

void RemoveHttpContext(int contextId) {
    std::lock_guard lock(g_optionsMutex);
    g_contextAcceptEncodingGzip.erase(contextId);
    if (g_contextAcceptEncodingGzip.empty()) g_objectOptions.clear();
}

bool HttpInitialised() {
    std::lock_guard lock(g_optionsMutex);
    return !g_contextAcceptEncodingGzip.empty();
}

void AddHttpTemplate(int templateId, int contextId) {
    std::lock_guard lock(g_optionsMutex);
    HttpOptions options;
    if (const auto context = g_contextAcceptEncodingGzip.find(contextId); context != g_contextAcceptEncodingGzip.end()) options.acceptEncodingGzip = context->second;
    g_objectOptions[templateId] = options;
}

void AddHttpChild(int id, int parentId) {
    std::lock_guard lock(g_optionsMutex);
    const auto parent = g_objectOptions.find(parentId);
    g_objectOptions[id] = parent != g_objectOptions.end() ? parent->second : HttpOptions{};
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
    *enable = options->autoRedirect ? 1 : 0;
    return 0;
}

int APS5_VABI sceHttpGetNonblock(int id, int* enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!enable) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(id);
    if (!options) return ERROR_INVALID_ID;
    *enable = options->nonblock ? 1 : 0;
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
    *enable = options->acceptEncodingGzip ? 1 : 0;
    return 0;
}

int APS5_VABI sceHttpSetAcceptEncodingGZIPEnabled(int id, int enable) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    return SetHttpOption(id, &HttpOptions::acceptEncodingGzip, enable != 0) ? 0 : ERROR_INVALID_ID;
}

int APS5_VABI sceHttpGetEpoll(int id, HttpEpollHandle* epoll, void** argument) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!epoll) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(id);
    if (!options) return ERROR_INVALID_ID;
    *epoll = options->epoll;
    if (argument) *argument = options->epollArgument;
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

}
