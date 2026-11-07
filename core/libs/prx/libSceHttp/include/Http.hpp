#pragma once

#include "SceTypes.hpp"

struct HttpConnectionStat;
struct HttpRequestStat;

struct HttpCookieStats {
    std::size_t usedBytes;
    std::uint32_t cookieCount;
    std::uint32_t reserved;
    std::size_t peakUsedBytes;
    std::uint32_t peakCookieCount;
    std::uint32_t evictedCookieCount;
};

struct HttpMemoryPoolStats {
    std::size_t poolSize;
    std::size_t peakUsedSize;
    std::size_t usedSize;
};

struct HttpRequestHeader {
    const char* name;
    const char* value;
};

struct HttpsCaList {
    void* certs;
    int certsNum;
};

using HttpCookieSendCallback = int (APS5_VABI *)(std::int32_t, const char*, const char*, void*);

using HttpSocketCreationCallback = void (APS5_VABI *)(std::int32_t, std::int32_t, void*);

using HttpRequestStatusCallback = void (APS5_VABI *)(int, int, void*);

extern "C" {

int APS5_VABI sceHttpAbortRequest(int requestId);
int APS5_VABI sceHttpAbortRequestForce(int requestId);
int APS5_VABI sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode);
int APS5_VABI sceHttpAuthCacheFlush(int httpCtxId);
int APS5_VABI sceHttpCookieFlush(int httpCtxId);
int APS5_VABI sceHttpCreateConnection(int tmplId, const char* serverName, const char* scheme, uint16_t port, int enableKeepAlive);
int APS5_VABI sceHttpCreateConnectionWithURL(int tmplId, const char* url, int enableKeepAlive);
int APS5_VABI sceHttpCreateEpoll(int httpCtxId, HttpEpollHandle* eh);
int APS5_VABI sceHttpCreateRequest(int connId, int method, const char* path, uint64_t contentLength);
int APS5_VABI sceHttpCreateRequestWithURL2(int connId, const char* method, const char* url, uint64_t contentLength);
int APS5_VABI sceHttpCreateTemplate(int httpCtxId, const char* userAgent, int httpVer, int isAutoProxyConf);
int APS5_VABI sceHttpDeleteConnection(int connId);
int APS5_VABI sceHttpDeleteRequest(int reqId);
int APS5_VABI sceHttpDeleteTemplate(int tmplId);
int APS5_VABI sceHttpDestroyEpoll(int httpCtxId, HttpEpollHandle eh);
int APS5_VABI sceHttpGetAllResponseHeaders(int requestId, char** header, size_t* headerSize);
int APS5_VABI sceHttpGetResponseContentLength(int requestId, int* result, uint64_t* contentLength);
int APS5_VABI sceHttpGetStatusCode(int requestId, int* statusCode);
int APS5_VABI sceHttpInit_nid_postfix(int memid, int sslCtxId, uint64_t poolSize);
int APS5_VABI sceHttpsDisableOption(int id, uint32_t sslFlags);
int APS5_VABI sceHttpSendRequest(int requestId, const void* postData, size_t size);
int APS5_VABI sceHttpSetAuthEnabled(int id, int enable);
int APS5_VABI sceHttpSetAuthInfoCallback(int id, HttpAuthInfoCallback callback, void* userArg);
int APS5_VABI sceHttpSetCookieEnabled(int id, int enable);
int APS5_VABI sceHttpSetAutoRedirect(int id, int enable);
int APS5_VABI sceHttpSetConnectTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttpSetEpoll(int id, HttpEpollHandle eh, void* userArg);
int APS5_VABI sceHttpSetNonblock(int id, int enable);
int APS5_VABI sceHttpTrySetNonblock(int id, int enable);
int APS5_VABI sceHttpSetRecvTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttpSetRecvBlockSize(int id, uint32_t blockSize);
int APS5_VABI sceHttpSetResponseHeaderMaxSize(int id, uint64_t headerSize);
int APS5_VABI sceHttpSetRequestContentLength(int requestId, uint64_t contentLength);
int APS5_VABI sceHttpSetResolveRetry(int id, int32_t retry);
int APS5_VABI sceHttpSetResolveTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttpSetSendTimeOut(int id, uint32_t usec);
int APS5_VABI sceHttpsSetMinSslVersion(int id, uint32_t sslVersion);
int APS5_VABI sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* userArg);
int APS5_VABI sceHttpTerm_nid_postfix(int httpCtxId);
int APS5_VABI sceHttpUnsetEpoll(int id);
int APS5_VABI sceHttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents, int timeout);
int APS5_VABI sceHttpCreateRequestWithURL(int connId, int method, const char* url, uint64_t contentLength);
int APS5_VABI sceHttpReadData(int requestId, void* data, size_t size);
int APS5_VABI sceHttpRedirectCacheFlush(int httpCtxId);
int APS5_VABI sceHttpSetChunkedTransferEnabled(int id, int enable);
int APS5_VABI sceHttpSetInflateGZIPEnabled(int id, int enable);
int APS5_VABI sceHttpSetRequestStatusCallback(int id, HttpRequestStatusCallback callback, void* argument);
int APS5_VABI sceHttpParseResponseHeader(const char* header, std::size_t headerLen, const char* fieldStr, const char** fieldValue, std::size_t* valueLen);
int APS5_VABI sceHttpCreateRequest2(int connId, const char* method, const char* path, uint64_t contentLength);
int APS5_VABI sceHttpsEnableOption(int id, uint32_t sslFlags);
int APS5_VABI sceHttpsLoadCert(int httpCtxId, int num, void* caList, void* cert, void* key);
int APS5_VABI sceHttpsUnloadCert(int httpCtxId);
int APS5_VABI sceHttpGetLastErrno(int requestId, int* errnoOut);
int APS5_VABI sceHttpsGetSslError(int id, int* errNum, uint32_t* detail);
int APS5_VABI sceHttpSetCookieRecvCallback(int id, HttpCookieRecvCallback cbfunc, void* userArg);
int APS5_VABI sceHttpsSetSslVersion(int id, int sslVersion);
int APS5_VABI sceHttpSetRedirectCallback(int id, HttpRedirectCallback cbfunc, void* userArg);
int APS5_VABI sceHttpAbortWaitRequest(HttpEpollHandle eh);
int APS5_VABI sceHttpsGetCaList(int httpCtxId, HttpsCaList* caList);
int APS5_VABI sceHttpsFreeCaList(int httpCtxId, HttpsCaList* caList);
int APS5_VABI sceHttpAuthCacheImport(int contextId, const void* buffer, std::size_t size);
int APS5_VABI sceHttpAuthCacheExport(int contextId, void* buffer, std::size_t size, std::size_t* requiredSize);
int APS5_VABI sceHttpDbgEnableProfile(void);
int APS5_VABI sceHttpDbgSetPrintf(void);
int APS5_VABI sceHttpDbgShowStat(void);
int APS5_VABI sceHttpGetMemoryPoolStats(int contextId, HttpMemoryPoolStats* stats);
int APS5_VABI sceHttpDbgShowConnectionStat(void);
int APS5_VABI sceHttpDbgShowRequestStat(void);
int APS5_VABI sceHttpDbgShowMemoryPoolStat(void);
int APS5_VABI sceHttpDbgGetConnectionStat(std::int32_t connectionId, HttpConnectionStat* stat);
int APS5_VABI sceHttpDbgGetRequestStat(std::int32_t requestId, HttpRequestStat* stat);
int APS5_VABI sceHttpAddRequestHeaderRaw(int id, const char* header, int mode);
int APS5_VABI sceHttpRemoveRequestHeader(int id, const char* name);
int APS5_VABI sceHttpGetAuthEnabled(int id, int* enable);
int APS5_VABI sceHttpSetEpollId(std::int32_t id, std::int32_t epollId, void* userArg);
int APS5_VABI sceHttpGetEpollId(std::int32_t id, std::int32_t* epollId, void** userArg);
int APS5_VABI sceHttpGetCookie(int contextId, const char* url, char* cookie, std::size_t* requiredSize, std::size_t size);
int APS5_VABI sceHttpGetCookieEnabled(int id, int* enable);
int APS5_VABI sceHttpAddCookie(int contextId, const char* url, const char* cookie, std::size_t size);
int APS5_VABI sceHttpSetCookieSendCallback(std::int32_t id, HttpCookieSendCallback callback, void* userArg);
int APS5_VABI sceHttpCookieExport(int contextId, void* buffer, std::size_t size, std::size_t* requiredSize);
int APS5_VABI sceHttpCookieImport(int contextId, const void* buffer, std::size_t size);
int APS5_VABI sceHttpSetCookieTotalMaxSize(int contextId, std::uint32_t size);
int APS5_VABI sceHttpSetCookieMaxSize(int contextId, std::uint32_t size);
int APS5_VABI sceHttpSetCookieMaxNum(int contextId, std::uint32_t count);
int APS5_VABI sceHttpSetCookieMaxNumPerDomain(int contextId, std::uint32_t count);
int APS5_VABI sceHttpGetCookieStats(int id, HttpCookieStats* stats);
int APS5_VABI sceHttpSetPriorityOption(int id, int priority);
int APS5_VABI sceHttpSetPolicyOption(int id, int policy);
int APS5_VABI sceHttpRequestGetAllHeaders(int requestId, HttpRequestHeader* headers, std::uint32_t* headerCount, char* contentLength, std::size_t* contentLengthSize);
int APS5_VABI sceHttpSetDefaultAcceptEncodingGZIPEnabled(int httpCtxId, int enable);
int APS5_VABI sceHttpSetProxy(int id, int, int, const char* hostname, std::uint16_t);
int APS5_VABI sceHttpSetSocketCreationCallback(std::int32_t id, HttpSocketCreationCallback callback, void* userArg);
int APS5_VABI sceHttpSetHttp09Enabled(int id, int enabled);
int APS5_VABI sceHttpGetRegisteredCtxIds(int contextId, int* netPoolId, int* sslContextId);
int APS5_VABI sceHttpGetConnectionStat(std::int32_t connectionId, HttpConnectionStat* stat);
int APS5_VABI sceHttpAddQuery(int id, const char* query);
int APS5_VABI sceHttpSetDelayBuildRequestEnabled(int id, int enabled);
int APS5_VABI sceHttpGetAutoRedirect(int id, int* enable);
int APS5_VABI sceHttpGetNonblock(int id, int* enable);
int APS5_VABI sceHttpTryGetNonblock(int id, int* enable);
int APS5_VABI sceHttpGetAcceptEncodingGZIPEnabled(int id, int* enable);
int APS5_VABI sceHttpSetAcceptEncodingGZIPEnabled(int id, int enable);
int APS5_VABI sceHttpGetEpoll(int id, HttpEpollHandle* epoll, void** argument);
int APS5_VABI sceHttpCacheRedirectedConnectionEnabled(int id, int enable);
int APS5_VABI sceHttpsEnableOptionPrivate(int id, std::uint32_t sslFlags);
int APS5_VABI sceHttpsDisableOptionPrivate(int id, std::uint32_t sslFlags);
int APS5_VABI sceHttpParseStatusLine(const char* statusLine, size_t lineLen, int32_t* httpMajorVer, int32_t* httpMinorVer, int32_t* responseCode, const char** reasonPhrase, size_t* phraseLen);
int APS5_VABI sceHttpUriBuild(char* out, size_t* requiredSize, size_t capacity, const SceHttpUriElement* element, uint32_t option);
int APS5_VABI sceHttpUriEscape(char* out, size_t* require, size_t prepare, const char* in);
int APS5_VABI sceHttpUriUnescape(char* out, size_t* requiredSize, size_t capacity, const char* input);
int APS5_VABI sceHttpUriMerge(char* mergedUrl, const char* url, const char* relativeUri, size_t* require, size_t prepare, uint32_t option);
int APS5_VABI sceHttpUriParse(SceHttpUriElement* out, const char* srcUrl, void* pool, size_t* require, size_t prepare);
int APS5_VABI sceHttpUriCopy(SceHttpUriElement* out, const SceHttpUriElement* src, void* pool, size_t prepare, size_t* require);
int APS5_VABI sceHttpUriSweepPath(char* dst, const char* src, size_t srcSize);

}
