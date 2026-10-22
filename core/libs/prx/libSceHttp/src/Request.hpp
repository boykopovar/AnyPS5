#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_REQUEST_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_REQUEST_HPP

#include <cstddef>
#include <cstdint>

void AddHttpTransportContext(int contextId, int memId, int sslContextId);
void RemoveHttpTransportContext(int contextId);
int AddHttpTransportTemplate(int templateId, int contextId, const char* userAgent, int httpVersion);
int AddHttpTransportConnection(int connectionId, int templateId, const char* serverName, const char* scheme,
    std::uint16_t port, bool keepAlive);
int AddHttpTransportConnectionWithUrl(int connectionId, int templateId, const char* url, bool keepAlive);
int AddHttpTransportRequest(int requestId, int connectionId, int method, const char* path, std::uint64_t contentLength);
int AddHttpTransportRequestWithMethod(int requestId, int connectionId, const char* method, const char* url,
    std::uint64_t contentLength);
int AddHttpTransportRequestWithMethodAndPath(int requestId, int connectionId, const char* method, const char* path,
    std::uint64_t contentLength);
void RemoveHttpTransportTemplate(int templateId);
void RemoveHttpTransportConnection(int connectionId);
void RemoveHttpTransportRequest(int requestId);
int AddHttpRequestHeader(int requestId, const char* name, const char* value, std::uint32_t mode);
int AddHttpRequestHeaderRaw(int requestId, const char* header, int mode);
int SetHttpRequestContentLength(int requestId, std::uint64_t contentLength);
bool SetHttpTransportTimeout(int id, int kind, std::uint32_t usec);
bool SetHttpTransportSslCallback(int id, HttpsCallback callback, void* argument);
bool SetHttpTransportResponseHeaderMaxSize(int id, std::uint64_t size);
int SendHttpRequest(int requestId, const void* data, std::size_t size);
int GetHttpRequestStatusCode(int requestId, int* statusCode);
int GetHttpRequestContentLength(int requestId, int* result, std::uint64_t* contentLength);
int GetHttpRequestAllResponseHeaders(int requestId, char** headers, std::size_t* size);
int ReadHttpRequestData(int requestId, void* buffer, std::size_t size);
int GetHttpRequestLastErrno(int requestId, int* error);

#endif
