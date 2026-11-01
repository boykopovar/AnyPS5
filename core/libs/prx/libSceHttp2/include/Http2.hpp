#pragma once

#include "SceTypes.hpp"

struct Http2MemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

struct Http2CookieStats {
    std::size_t currentInuseSize;
    std::uint32_t currentInuseNum;
    std::size_t maxInuseSize;
    std::uint32_t maxInuseNum;
    std::uint32_t removedNum;
};

using Http2WebSocketTextCallback = void (APS5_VABI*)(std::int32_t requestId, const char* text, std::uint32_t flags, void* userData);

using Http2WebSocketDataCallback = void (APS5_VABI*)(std::int32_t requestId, const void* data, std::size_t dataSize, std::uint32_t flags, void* userData);

using Http2WebSocketCloseCallback = void (APS5_VABI*)(std::int32_t requestId, const std::uint16_t* status, const char* reason, std::uint32_t flags, void* userData);

using Http2WebSocketErrorCallback = void (APS5_VABI*)(std::int32_t requestId, std::int32_t error, void* userData);

using Http2AuthInfoCallback = int (APS5_VABI*)(int request, int authType, const char* realm, char* username, char* password, int isNeedEntity, std::uint8_t** entityBody, std::size_t* entitySize, int* isSave, void* userArg);

extern "C" {

int APS5_VABI sceHttp2AddRequestHeader(int id, const char* name, const char* value, uint32_t mode);
int APS5_VABI sceHttp2CreateRequestWithURL(int tmplId, const char* method, const char* url, uint64_t contentLength);
int APS5_VABI sceHttp2CreateTemplate(int libHttp2CtxId, const char* userAgent, int httpVer, int isAutoProxyConf);
int APS5_VABI sceHttp2DeleteRequest(int reqId);
int APS5_VABI sceHttp2DeleteTemplate(int tmplId);
int APS5_VABI sceHttp2GetAllResponseHeaders(int reqId, char** header, size_t* headerSize);
int APS5_VABI sceHttp2GetResponseContentLength(int reqId, int* result, uint64_t* contentLength);
int APS5_VABI sceHttp2GetStatusCode(int reqId, int* statusCode);
int APS5_VABI sceHttp2Init(int libnetMemId, int libsslCtxId, size_t poolSize, int maxConcurrentRequest);
int APS5_VABI sceHttp2ReadData(int reqId, void* data, size_t size);
int APS5_VABI sceHttp2ReadDataAsync(int reqId, void* data, size_t size, void* kqueueOption, void* option);
int APS5_VABI sceHttp2SendRequest(int reqId, const void* postData, size_t size);
int APS5_VABI sceHttp2SendRequestAsync(int reqId, const void* postData, size_t size, Http2AsyncOption* kqueueOption, void* option);
int APS5_VABI sceHttp2SetAuthEnabled(int id, int isEnable);
int APS5_VABI sceHttp2SetAutoRedirect(int id, int enable);
int APS5_VABI sceHttp2SetConnectionWaitTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttp2SetConnectTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttp2SetInflateGZIPEnabled(int id, int enable);
int APS5_VABI sceHttp2SetRecvTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttp2SetRedirectCallback(int id, void* cbFunc, void* userArg);
int APS5_VABI sceHttp2SetRequestContentLength(int id, uint64_t contentLength);
int APS5_VABI sceHttp2SetResolveTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttp2SetSendTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttp2SetSslCallback(int id, void* cbFunc, void* userArg);
int APS5_VABI sceHttp2SetTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttp2SslDisableOption(int id, uint32_t sslFlags);
int APS5_VABI sceHttp2SslEnableOption(int id, uint32_t sslFlags);
int APS5_VABI sceHttp2SetMinSslVersion(int id, uint32_t sslVersion);
int APS5_VABI sceHttp2Term(int libHttp2CtxId);
int APS5_VABI sceHttp2WaitAsync(int reqId, Http2AsyncResult* result, uint32_t* timeout, void* option);
int APS5_VABI sceHttp2AbortRequest(int reqId);
int APS5_VABI sceHttp2GetMemoryPoolStats(int libHttp2CtxId, Http2MemoryPoolStats* stats);
int APS5_VABI sceHttp2CookieFlush(int id);
int APS5_VABI sceHttp2CreateCookieBox(std::size_t poolSize);
int APS5_VABI sceHttp2DeleteCookieBox(int cookieBoxId);
int APS5_VABI sceHttp2SetCookieBox(int id, int cookieBoxId);
int APS5_VABI sceHttp2SetRequestNoContentLength(int id);
int APS5_VABI sceHttp2SetResolveRetry(int id, int32_t retry);
int APS5_VABI sceHttp2WebSocketCreateRequest(std::int32_t templateId, const char* url, Http2WebSocketTextCallback textCallback, Http2WebSocketDataCallback dataCallback, Http2WebSocketCloseCallback closeCallback, Http2WebSocketErrorCallback errorCallback, void* receiveBuffer, std::size_t receiveBufferSize, void* userData, const void* option);
int APS5_VABI sceHttp2WebSocketCloseAsync(std::int32_t requestId, const std::uint16_t* status, const char* reason, std::uint8_t flags, const Http2AsyncOption* asyncOption, const void* option);
int APS5_VABI sceHttp2WebSocketSendTextMessageAsync(std::int32_t requestId, const char* text, std::uint8_t flags, const Http2AsyncOption* asyncOption, const void* option);
int APS5_VABI sceHttp2WebSocketSendDataMessageAsync(std::int32_t requestId, const void* data, std::size_t dataSize, std::uint8_t flags, const Http2AsyncOption* asyncOption, const void* option);
int APS5_VABI sceHttp2WebSocketSetPingInterval(std::int32_t requestId, std::uint32_t intervalMicroseconds);
int APS5_VABI sceHttp2WebSocketSetPingTimeout(std::int32_t requestId, std::uint32_t timeoutMicroseconds);
int APS5_VABI sceHttp2AddCookie(int cookieBoxId, const char* url, const char* cookie, std::size_t cookieLength);
int APS5_VABI sceHttp2AuthCacheFlush(int libHttp2CtxId);
int APS5_VABI sceHttp2CookieExport(void);
int APS5_VABI sceHttp2CookieImport(void);
int APS5_VABI sceHttp2GetAuthEnabled(int id, int* isEnable);
int APS5_VABI sceHttp2GetAutoRedirect(int id, int* enable);
int APS5_VABI sceHttp2GetCookie(int cookieBoxId, const char* url, char* cookie, std::size_t* required, std::size_t prepared);
int APS5_VABI sceHttp2GetCookieBox(void);
int APS5_VABI sceHttp2GetCookieStats(int cookieBoxId, Http2CookieStats* stats);
int APS5_VABI sceHttp2RedirectCacheFlush(int libHttp2CtxId);
int APS5_VABI sceHttp2RemoveRequestHeader(int id, const char* name);
int APS5_VABI sceHttp2SetAuthInfoCallback(int id, Http2AuthInfoCallback cbFunc, void* userArg);
int APS5_VABI sceHttp2SetCookieMaxNum(int cookieBoxId, std::uint32_t num);
int APS5_VABI sceHttp2SetCookieMaxNumPerDomain(int cookieBoxId, std::uint32_t num);
int APS5_VABI sceHttp2SetCookieMaxSize(int cookieBoxId, std::uint32_t size);
int APS5_VABI sceHttp2SetCookieRecvCallback(void);
int APS5_VABI sceHttp2SetCookieSendCallback(void);
int APS5_VABI sceHttp2SetPreSendCallback(int id, void* cbFunc, void* userArg);

}
