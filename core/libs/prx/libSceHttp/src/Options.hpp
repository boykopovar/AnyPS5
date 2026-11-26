#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_OPTIONS_HPP

#include "SceTypes.hpp"
#include "prx/libSceHttp/src/Cookies.hpp"
#include <optional>
#include <string>

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
    int proxyHttpConf = 0;
    int proxyWlanConf = 0;
    std::uint16_t proxyPort = 0;
    std::string proxyHost;
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
bool SetHttpProxy(int id, int httpConf, int wlanConf, std::string host, std::uint16_t port);
bool SetHttpEpollId(int id, int epollId);
bool SetHttpContextAcceptEncodingGzip(int contextId, bool enable);
bool SetHttpEpollBinding(int id, HttpEpollHandle epoll, void* argument);

#endif
