#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP

#include "SceTypes.hpp"
#include "prx/libSceHttp/src/Cookies.hpp"
#include <cstdint>
#include <optional>

struct HttpOptions {
    bool autoRedirect = true;
    bool authEnabled = false;
    int cookieEnabled = 0;
    bool nonblock = false;
    bool acceptEncodingGzip = false;
    bool delayBuildRequest = false;
    int epollId = -1;
    HttpEpollHandle epoll = nullptr;
    void* epollArgument = nullptr;
    bool request = false;
    std::uint32_t sendTimeoutUs = 0;
    std::uint32_t recvTimeoutUs = 0;
    std::uint32_t resolveTimeoutUs = 0;
    std::uint32_t recvBlockSize = 0;
    std::uint64_t responseHeaderMaxSize = 0;
    std::uint64_t requestContentLength = 0;
    int resolveRetry = 0;
};

void AddHttpContext(int contextId);
void RemoveHttpContext(int contextId);
bool HttpInitialised();
bool IsHttpContext(int contextId);
void AddHttpTemplate(int templateId, int contextId);
void AddHttpChild(int id, int parentId, bool request = false);
void RemoveHttpObject(int id);
std::optional<HttpOptions> FindHttpOptions(int id);
bool SetHttpOption(int id, bool HttpOptions::*option, bool enable);
template <class T>
bool SetHttpValue(int id, T HttpOptions::*option, T value);
bool SetHttpRequestContentLength(int id, std::uint64_t length);
bool SetHttpCookieEnabled(int id, int enable);
bool SetHttpEpollId(int id, int epollId);
bool SetHttpContextAcceptEncodingGzip(int contextId, bool enable);
bool SetHttpEpollBinding(int id, HttpEpollHandle epoll, void* argument);

#endif
