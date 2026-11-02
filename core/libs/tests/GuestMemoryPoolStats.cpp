#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

template <typename TStats>
static void CheckRejected(int (APS5_VABI *call)(int, TStats*), int id, int expected) {
    struct {
        TStats stats;
        std::array<unsigned char, 0x40 - sizeof(TStats)> guard;
    } buffer;
    std::memset(&buffer, 0x5a, sizeof(buffer));
    Require(call(id, &buffer.stats) == expected);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&buffer.stats);
    for (std::size_t i = 0; i < sizeof(TStats); ++i) Require(bytes[i] == 0);
    for (const auto byte : buffer.guard) Require(byte == 0x5a);
}

int main() {
    constexpr int sslInvalidId = static_cast<int>(0x8095F006);
    constexpr int sslInvalidValue = static_cast<int>(0x8095F007);
    constexpr int http2InvalidId = static_cast<int>(0x817B1100);
    constexpr int http2NullPointer = static_cast<int>(0x817B1225);
    constexpr int netBadDescriptor = static_cast<int>(0x80410109);
    constexpr int netInvalid = static_cast<int>(0x80410116);
    static_assert(sizeof(SceSsl::MemoryPoolStats) == 0x20 && sizeof(Http2MemoryPoolStats) == 0x20 && sizeof(NetMemoryPoolStats) == 0x18);

    const int ssl = sceSslInit_nid_postfix(0x40000);
    Require(ssl > 0);
    SceSsl::MemoryPoolStats sslStats{};
    Require(sceSslGetMemoryPoolStats(ssl, &sslStats) == 0 && sslStats.poolSize == 0x40000);
    Require(sceSslGetMemoryPoolStats(ssl, nullptr) == sslInvalidValue);
    Require(sceSslGetMemoryPoolStats(12345, nullptr) == sslInvalidValue);
    CheckRejected(sceSslGetMemoryPoolStats, 12345, sslInvalidId);
    CheckRejected(sceSslGetMemoryPoolStats, -1, sslInvalidId);

    const int http2 = sceHttp2Init(1, ssl, 0x40000, 1);
    Require(http2 > 0);
    Http2MemoryPoolStats http2Stats{1, 2, 3, 4};
    Require(sceHttp2GetMemoryPoolStats(http2, &http2Stats) == 0);
    Require(http2Stats.pool_size == 0x40000 && http2Stats.max_inuse_size == 0 && http2Stats.current_inuse_size == 0 && http2Stats.reserved == 0);
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
    NetMemoryPoolStats netStats{};
    Require(sceNetGetMemoryPoolStats(pool, &netStats) == 0 && netStats.pool_size == 0x20000);
    Require(sceNetGetMemoryPoolStats(pool, nullptr) == netInvalid);
    Require(sceNetGetMemoryPoolStats(12345, nullptr) == netBadDescriptor);
    struct {
        NetMemoryPoolStats stats;
        std::array<unsigned char, 8> guard;
    } untouched;
    std::memset(&untouched, 0x5a, sizeof(untouched));
    auto buffer = untouched;
    Require(sceNetGetMemoryPoolStats(12345, &buffer.stats) == netBadDescriptor && std::memcmp(&buffer, &untouched, sizeof(buffer)) == 0);
    Require(sceNetPoolDestroy(pool) == 0);
    Require(sceNetGetMemoryPoolStats(pool, nullptr) == netBadDescriptor);
    return 0;
}
