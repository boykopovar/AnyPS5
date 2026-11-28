#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cerrno>
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

static void CheckKernelMemoryPoolStats() {
    constexpr std::size_t kTwoMiB = 2ULL * 1024 * 1024;
    constexpr std::size_t kFourMiB = 4ULL * 1024 * 1024;
    constexpr std::size_t kPageSize = 0x4000;
    constexpr int kProtRw = 3;

    const int64_t directSize = static_cast<int64_t>(sceKernelGetDirectMemorySize());
    Require(directSize >= static_cast<int64_t>(kFourMiB));

    errno = 123;
    Require(sceKernelMemoryPoolExpand(0, directSize, kFourMiB, 0, reinterpret_cast<int64_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 123);

    int64_t phys = -1;
    Require(sceKernelMemoryPoolExpand(0, directSize, kFourMiB, 0, &phys) == 0 && phys >= 0);

    void* base = nullptr;
    Require(sceKernelMemoryPoolReserve(nullptr, kFourMiB, 0, 0, &base) == 0 && base != nullptr);
    auto* bytes = static_cast<unsigned char*>(base);

    KernelMemoryPoolBlockStats stats{};
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 0 && stats.available_flushed_blocks == 2);

    Require(sceKernelMemoryPoolCommit(bytes, kTwoMiB, 0, kProtRw, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 1 && stats.available_flushed_blocks == 1);

    Require(sceKernelMemoryPoolCommit(bytes, kTwoMiB, 0, kProtRw, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 1 && stats.available_flushed_blocks == 1);

    Require(sceKernelMemoryPoolDecommit(bytes + kTwoMiB, kTwoMiB, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 1 && stats.available_flushed_blocks == 1);

    KernelMemoryPoolBatchEntry entries[4]{};
    entries[0].op = 1;
    entries[0].commit.addr = bytes + kTwoMiB - kPageSize;
    entries[0].commit.len = 2 * kPageSize;
    entries[0].commit.prot = kProtRw;
    entries[0].commit.type = 0;

    entries[1].op = 1;
    entries[1].commit.addr = bytes + kTwoMiB + kPageSize;
    entries[1].commit.len = kTwoMiB - kPageSize;
    entries[1].commit.prot = kProtRw;
    entries[1].commit.type = 0;

    entries[2].op = 2;
    entries[2].decommit.addr = bytes + kTwoMiB;
    entries[2].decommit.len = kTwoMiB;

    entries[3].op = 2;
    entries[3].decommit.addr = bytes + kTwoMiB;
    entries[3].decommit.len = kTwoMiB;

    int done = -1;
    Require(sceKernelMemoryPoolBatch(entries, 2, &done, 0) == 0 && done == 2);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 2 && stats.available_flushed_blocks == 0);

    done = -1;
    Require(sceKernelMemoryPoolBatch(entries + 2, 2, &done, 0) == 0 && done == 2);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 1 && stats.available_flushed_blocks == 1);

    errno = 456;
    Require(sceKernelMemoryPoolGetBlockStats(reinterpret_cast<KernelMemoryPoolBlockStats*>(0x10), sizeof(stats)) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 456);

    Require(sceKernelMemoryPoolCommit(bytes, ~static_cast<std::size_t>(kPageSize - 1), 0, kProtRw, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMemoryPoolDecommit(bytes, ~static_cast<std::size_t>(kPageSize - 1), 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMemoryPoolDecommit(bytes, kFourMiB, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 0 && stats.available_flushed_blocks == 2);

    Require(sceKernelMemoryPoolCommit(bytes, kFourMiB, 0, kProtRw, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 2 && stats.available_flushed_blocks == 0);
    Require(sceKernelMemoryPoolDecommit(bytes + kPageSize, kPageSize, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 2 && stats.available_flushed_blocks == 0);
    Require(sceKernelMemoryPoolDecommit(bytes, kTwoMiB, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 1 && stats.available_flushed_blocks == 1);
    Require(sceKernelMemoryPoolDecommit(bytes + kTwoMiB, kTwoMiB, 0) == 0);
    Require(sceKernelMemoryPoolGetBlockStats(&stats, sizeof(stats)) == 0);
    Require(stats.allocated_flushed_blocks == 0 && stats.available_flushed_blocks == 2);

    Require(munmap_nid_postfix(base, kFourMiB) == 0);
    Require(sceKernelReleaseDirectMemory(phys, kFourMiB) == 0);
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
    Require(sceHttp2GetMemoryPoolStats(http2, reinterpret_cast<Http2MemoryPoolStats*>(0x10)) == http2NullPointer);
    Require(sceHttp2GetMemoryPoolStats(12345, nullptr) == http2NullPointer);
    Require(sceHttp2GetMemoryPoolStats(12345, reinterpret_cast<Http2MemoryPoolStats*>(0x10)) == http2NullPointer);
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

    CheckKernelMemoryPoolStats();
    return 0;
}
