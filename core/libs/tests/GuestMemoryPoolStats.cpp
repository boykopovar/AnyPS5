#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>

struct PoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

struct NetPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
};

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::size_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslGetMemoryPoolStats(int, void*);
int APS5_VABI sceHttp2Init(int, int, std::size_t, int);
int APS5_VABI sceHttp2Term(int);
int APS5_VABI sceHttp2GetMemoryPoolStats(int, void*);
int APS5_VABI sceNetPoolCreate(const char*, int, int);
int APS5_VABI sceNetPoolDestroy(int);
int APS5_VABI sceNetGetMemoryPoolStats(int, void*);
}

static void Require(bool value) { if (!value) std::abort(); }

using StatsCall = int(APS5_VABI*)(int, void*);

static void CheckRejected(StatsCall call, int id, int expected) {
    std::array<unsigned char, 0x40> buffer;
    buffer.fill(0x5a);
    Require(call(id, buffer.data()) == expected);
    for (std::size_t i = 0; i < sizeof(PoolStats); ++i) Require(buffer[i] == 0);
    for (std::size_t i = sizeof(PoolStats); i < buffer.size(); ++i) Require(buffer[i] == 0x5a);
}

int main() {
    constexpr int sslInvalidId = static_cast<int>(0x8095F006);
    constexpr int sslInvalidValue = static_cast<int>(0x8095F007);
    constexpr int http2InvalidId = static_cast<int>(0x817B1100);
    constexpr int http2NullPointer = static_cast<int>(0x817B1225);
    constexpr int netBadDescriptor = static_cast<int>(0x80410109);
    constexpr int netInvalid = static_cast<int>(0x80410116);
    static_assert(sizeof(PoolStats) == 0x20 && sizeof(NetPoolStats) == 0x18);

    const int ssl = sceSslInit_nid_postfix(0x40000);
    Require(ssl > 0);
    PoolStats stats{};
    Require(sceSslGetMemoryPoolStats(ssl, &stats) == 0 && stats.pool_size == 0x40000);
    Require(sceSslGetMemoryPoolStats(ssl, nullptr) == sslInvalidValue);
    Require(sceSslGetMemoryPoolStats(12345, nullptr) == sslInvalidValue);
    CheckRejected(sceSslGetMemoryPoolStats, 12345, sslInvalidId);
    CheckRejected(sceSslGetMemoryPoolStats, -1, sslInvalidId);

    const int http2 = sceHttp2Init(1, ssl, 0x40000, 1);
    Require(http2 > 0);
    bool http2StatsThrow = false;
    const PoolStats previous = stats;
    try { sceHttp2GetMemoryPoolStats(http2, &stats); }
    catch (const std::exception&) { http2StatsThrow = true; }
    Require(http2StatsThrow && stats.pool_size == previous.pool_size && stats.max_inuse_size == previous.max_inuse_size &&
        stats.current_inuse_size == previous.current_inuse_size && stats.reserved == previous.reserved);
    Require(sceHttp2GetMemoryPoolStats(http2, nullptr) == http2NullPointer);
    Require(sceHttp2GetMemoryPoolStats(12345, nullptr) == http2NullPointer);
    CheckRejected(sceHttp2GetMemoryPoolStats, 12345, http2InvalidId);
    CheckRejected(sceHttp2GetMemoryPoolStats, -1, http2InvalidId);
    Require(sceHttp2Term(http2) == 0);
    CheckRejected(sceHttp2GetMemoryPoolStats, http2, http2InvalidId);

    Require(sceSslTerm_nid_postfix(ssl) == 0);
    CheckRejected(sceSslGetMemoryPoolStats, ssl, sslInvalidId);

    const int pool = sceNetPoolCreate("stats", 0x20000, 0);
    Require(pool > 0);
    NetPoolStats netStats{};
    Require(sceNetGetMemoryPoolStats(pool, &netStats) == 0 && netStats.pool_size == 0x20000);
    Require(sceNetGetMemoryPoolStats(pool, nullptr) == netInvalid);
    Require(sceNetGetMemoryPoolStats(12345, nullptr) == netBadDescriptor);
    std::array<unsigned char, 0x20> untouched;
    untouched.fill(0x5a);
    auto buffer = untouched;
    Require(sceNetGetMemoryPoolStats(12345, buffer.data()) == netBadDescriptor && buffer == untouched);
    Require(sceNetPoolDestroy(pool) == 0);
    Require(sceNetGetMemoryPoolStats(pool, nullptr) == netBadDescriptor);
    return 0;
}
