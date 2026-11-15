#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP

#include "SceTypes.hpp"
#include "prx/libSceHttp/src/Cookies.hpp"
#include <optional>

struct HttpOptions {
    bool autoRedirect = true;
    bool authEnabled = false;
    int cookieEnabled = 0;
    bool nonblock = false;
    bool acceptEncodingGzip = false;
    bool delayBuildRequest = false;
    HttpEpollHandle epoll = nullptr;
    void* epollArgument = nullptr;
};

void AddHttpContext(int contextId);
void RemoveHttpContext(int contextId);
bool HttpInitialised();
bool IsHttpContext(int contextId);
void AddHttpTemplate(int templateId, int contextId);
void AddHttpChild(int id, int parentId);
void RemoveHttpObject(int id);
std::optional<HttpOptions> FindHttpOptions(int id);
bool SetHttpOption(int id, bool HttpOptions::*option, bool enable);
bool SetHttpCookieEnabled(int id, int enable);
bool SetHttpContextAcceptEncodingGzip(int contextId, bool enable);
bool SetHttpEpollBinding(int id, HttpEpollHandle epoll, void* argument);

#endif
