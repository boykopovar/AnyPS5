#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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

static_assert(sizeof(HttpCacheHeader) == 16);
static_assert(sizeof(HttpCacheRequest) == 32);
static_assert(offsetof(HttpCacheRequest, url) == 8);
static_assert(offsetof(HttpCacheRequest, headers) == 24);
static_assert(sizeof(HttpCacheResponseHeader) == 24);
static_assert(offsetof(HttpCacheResponseHeader, headers) == 16);
static_assert(sizeof(HttpCacheResponse) == 32);
static_assert(offsetof(HttpCacheResponse, contentLength) == 24);
static_assert(sizeof(HttpCacheQuotaEntry) == 16);
static_assert(sizeof(HttpCacheQuota) == 16);

extern "C" {

int APS5_VABI sceHttpCacheClear(std::int32_t cacheId, std::int32_t userId, const char* tag) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheClearAll(std::int32_t cacheId, std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheCompleteRequest(std::int32_t requestId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheCreateRequest(std::int32_t cacheId, std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheCreateRequestWithTag(std::int32_t cacheId, std::int32_t userId, const char* tag) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheDeleteRequest(std::int32_t requestId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheInit(std::size_t poolSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheReadData(std::int32_t requestId, std::size_t size, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRetrieve(std::int32_t requestId, const HttpCacheRequest* request, HttpCacheResponse* response, HttpCacheRequest* nextRequest, std::int32_t* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRetrieveWithMemoryPool(std::int32_t requestId, const HttpCacheRequest* request, HttpCacheResponse* response, HttpCacheRequest* nextRequest, std::int32_t* result, void* pool, std::size_t* requiredSize, std::size_t poolSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRevalidate(std::int32_t requestId, const HttpCacheResponseHeader* header, HttpCacheResponse* response, std::int32_t* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRevalidateWithMemoryPool(std::int32_t requestId, const HttpCacheResponseHeader* header, HttpCacheResponse* response, std::int32_t* result, void* pool, std::size_t* requiredSize, std::size_t poolSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSetCacheSharing(std::uint8_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSetData(std::int32_t requestId, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSetQuota(std::int32_t cacheId, const HttpCacheQuota* quota) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSetResponseHeader(std::int32_t requestId, const HttpCacheResponseHeader* header) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemClearAll(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemInit(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemSendStatistics() {
    return 0;
}

int APS5_VABI sceHttpCacheSystemShutdown() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemTerm() {
    return 0;
}

int APS5_VABI sceHttpCacheTerm(std::int32_t cacheId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI sceHttpCacheClearAllUserTags() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheClearCacheInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheDebugSetUrl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRegisterCacheInfoUrl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRemoveTag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
