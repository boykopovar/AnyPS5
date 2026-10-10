#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>

// No network is emulated: contexts, templates and requests can be created, but any request
// that would touch the network fails with the library's network error.
static constexpr int ERROR_NETWORK = static_cast<int>(0x80435001);
static std::atomic<int> g_nextHandle{1};
static std::mutex g_poolsMutex;
static std::map<int, uint64_t> g_pools;

namespace {

constexpr int ERROR_NOT_FOUND = static_cast<int>(0x8095F004);
constexpr int ERROR_INVALID_ARG = static_cast<int>(0x8095177A);

struct SslData {
    char* ptr;
    size_t size;
};

struct SslMemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

struct SslCaCerts {
    SslData* certs;
    size_t num;
    void* pool;
};

}

extern "C" {

int APS5_VABI sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ssl_ctx_id;
    if (!ca_certs) return ERROR_INVALID_ARG;
    *static_cast<SslCaCerts*>(ca_certs) = {};
    return 0;
}

int APS5_VABI sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
    (void)ssl_ctx_id;
    if (!ca_certs) return ERROR_INVALID_ARG;
    *static_cast<SslCaCerts*>(ca_certs) = {};
    return ERROR_NOT_FOUND;
}

int APS5_VABI sceSslInit_nid_postfix(uint64_t pool_size) {
    const int id = g_nextHandle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_poolsMutex);
    g_pools[id] = pool_size;
    return id;
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
    std::lock_guard lock(g_poolsMutex);
    g_pools.erase(ssl_ctx_id);
    return 0;
}

int APS5_VABI sceSslClose() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetSerialNumber() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslLoadCert() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetMemoryPoolStats(int ssl_ctx_id, SslMemoryPoolStats* stats) {
    if (stats == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(g_poolsMutex);
    const auto pool = g_pools.find(ssl_ctx_id);
    if (pool == g_pools.end()) throw std::invalid_argument("sceSslGetMemoryPoolStats: unknown context");
    *stats = {static_cast<size_t>(pool->second), 0, 0, 0};
    return 0;
}

int APS5_VABI sceSslFreeSslCertName(void) {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetIssuerName(void) {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetNameEntryCount(void) {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetNameEntryInfo(void) {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetPem(void) {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceSslGetSubjectName(void) {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

}

