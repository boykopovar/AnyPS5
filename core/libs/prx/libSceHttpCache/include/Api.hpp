#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct HttpCacheHeader {
    const char* name;
    const char* value;
};

struct HttpCacheRequest {
    std::uint32_t method;
    std::uint32_t reserved;
    const char* url;
    std::size_t headerCount;
    const HttpCacheHeader* headers;
};

struct HttpCacheResponseHeader {
    std::uint32_t statusCode;
    std::uint32_t reserved;
    std::size_t headerCount;
    const HttpCacheHeader* headers;
};

struct HttpCacheResponse {
    std::uint32_t statusCode;
    std::uint32_t reserved;
    std::size_t headerCount;
    const HttpCacheHeader* headers;
    std::uint64_t contentLength;
};

struct HttpCacheQuotaEntry {
    const char* name;
    std::uint64_t size;
};

struct HttpCacheQuota {
    std::size_t count;
    const HttpCacheQuotaEntry* entries;
};

extern "C" {

int APS5_VABI sceHttpCacheClear(std::int32_t cacheId, std::int32_t userId, const char* tag);
int APS5_VABI sceHttpCacheClearAll(std::int32_t cacheId, std::int32_t userId);
int APS5_VABI sceHttpCacheCompleteRequest(std::int32_t requestId);
int APS5_VABI sceHttpCacheCreateRequest(std::int32_t cacheId, std::int32_t userId);
int APS5_VABI sceHttpCacheCreateRequestWithTag(std::int32_t cacheId, std::int32_t userId, const char* tag);
int APS5_VABI sceHttpCacheDeleteRequest(std::int32_t requestId);
int APS5_VABI sceHttpCacheInit(std::size_t poolSize);
int APS5_VABI sceHttpCacheReadData(std::int32_t requestId, std::size_t size, void* data);
int APS5_VABI sceHttpCacheRetrieve(std::int32_t requestId, const HttpCacheRequest* request, HttpCacheResponse* response, HttpCacheRequest* nextRequest, std::int32_t* result);
int APS5_VABI sceHttpCacheRetrieveWithMemoryPool(std::int32_t requestId, const HttpCacheRequest* request, HttpCacheResponse* response, HttpCacheRequest* nextRequest, std::int32_t* result, void* pool, std::size_t* requiredSize, std::size_t poolSize);
int APS5_VABI sceHttpCacheRevalidate(std::int32_t requestId, const HttpCacheResponseHeader* header, HttpCacheResponse* response, std::int32_t* result);
int APS5_VABI sceHttpCacheRevalidateWithMemoryPool(std::int32_t requestId, const HttpCacheResponseHeader* header, HttpCacheResponse* response, std::int32_t* result, void* pool, std::size_t* requiredSize, std::size_t poolSize);
int APS5_VABI sceHttpCacheSetCacheSharing(std::uint8_t enabled);
int APS5_VABI sceHttpCacheSetData(std::int32_t requestId, const void* data, std::size_t size);
int APS5_VABI sceHttpCacheSetQuota(std::int32_t cacheId, const HttpCacheQuota* quota);
int APS5_VABI sceHttpCacheSetResponseHeader(std::int32_t requestId, const HttpCacheResponseHeader* header);
int APS5_VABI sceHttpCacheSystemClearAll(const char* path);
int APS5_VABI sceHttpCacheSystemInit(const char* path);
int APS5_VABI sceHttpCacheSystemSendStatistics();
int APS5_VABI sceHttpCacheSystemShutdown();
int APS5_VABI sceHttpCacheSystemTerm();
int APS5_VABI sceHttpCacheTerm(std::int32_t cacheId);
int APS5_VABI sceHttpCacheClearAllUserTags();
int APS5_VABI sceHttpCacheClearCacheInfo();
int APS5_VABI sceHttpCacheDebugSetUrl();
int APS5_VABI sceHttpCacheRegisterCacheInfoUrl();
int APS5_VABI sceHttpCacheRemoveTag();

}
