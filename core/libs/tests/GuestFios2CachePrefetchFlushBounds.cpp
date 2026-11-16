#include "prx/libSceFios2/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::int32_t FiosOk = 0;
constexpr std::int32_t FiosErrorUnimplemented = static_cast<std::int32_t>(0x80820000u);
constexpr std::int32_t FiosErrorBadPath = static_cast<std::int32_t>(0x80820005u);
constexpr std::int32_t FiosErrorBadPtr = static_cast<std::int32_t>(0x80820006u);
constexpr std::int32_t FiosErrorBadSize = static_cast<std::int32_t>(0x80820008u);
constexpr std::int32_t FiosErrorBadOverlay = static_cast<std::int32_t>(0x8082001Au);
constexpr std::int32_t FiosErrorBadIndex = static_cast<std::int32_t>(0x8082001Cu);
constexpr std::int32_t FiosErrorBadFlags = static_cast<std::int32_t>(0x80820021u);

struct CacheFilterConfigTest {
    std::uint64_t size;
    std::uint64_t workBufferSize;
    std::uint64_t blockSize;
    void* workBuffer;
    const char* path;
    std::uint64_t flags;
    std::uint64_t reserved[3];
};

struct RamCacheContextTest {
    std::uint64_t size;
    std::uint64_t workBufferSize;
    std::uint64_t blockSize;
    void* workBuffer;
    const char* path;
    std::uint64_t flags;
    std::uint64_t reserved[3];
};

static_assert(sizeof(CacheFilterConfigTest) == 0x48);
static_assert(sizeof(RamCacheContextTest) == 0x48);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCacheFlushBounds() {
    sceFiosCacheFlushSync();
    sceFiosCacheFlushSync();

    sceFiosCacheFlushFileSync(nullptr);
    sceFiosCacheFlushFileSync("");
    sceFiosCacheFlushFileSync("/app0/data.bin");

    sceFiosCacheFlushFileRangeSync(nullptr, 0, 0);
    sceFiosCacheFlushFileRangeSync("", 0, 512);
    sceFiosCacheFlushFileRangeSync("/app0/data.bin", 0, 1024);
    sceFiosCacheFlushFileRangeSync("/app0/data.bin", -1, -1);
}

void TestCachePrefetchFileBounds() {
    Require(sceFiosCachePrefetchFile(nullptr, "/app0/data.bin") == 0);
    Require(sceFiosCachePrefetchFile(nullptr, nullptr) == 0);

    Require(sceFiosCachePrefetchFileRange(nullptr, "/app0/data.bin", 0, 1024) == 0);
    Require(sceFiosCachePrefetchFileRange(nullptr, nullptr, 0, 0) == 0);
    Require(sceFiosCachePrefetchFileRange(nullptr, "/app0/data.bin", -1, 512) == 0);

    Require(sceFiosCachePrefetchFileSync(nullptr, "/app0/data.bin") == FiosErrorUnimplemented);
    Require(sceFiosCachePrefetchFileSync(nullptr, nullptr) == FiosErrorUnimplemented);

    Require(sceFiosCachePrefetchFileRangeSync(nullptr, "/app0/data.bin", 0, 1024) == FiosOk);
    Require(sceFiosCachePrefetchFileRangeSync(nullptr, nullptr, 0, 0) == FiosOk);
    Require(sceFiosCachePrefetchFileRangeSync(nullptr, "/nonexistent/test.bin", 256, 128) == FiosOk);
}

void TestCachePrefetchFHBounds() {
    Require(sceFiosCachePrefetchFH(nullptr, 0) == 0);
    Require(sceFiosCachePrefetchFH(nullptr, 1) == 0);
    Require(sceFiosCachePrefetchFH(nullptr, 999) == 0);

    Require(sceFiosCachePrefetchFHRange(nullptr, 0, 0, 1024) == 0);
    Require(sceFiosCachePrefetchFHRange(nullptr, 1, 100, 200) == 0);
    Require(sceFiosCachePrefetchFHRange(nullptr, 999, 0, 500) == 0);

    Require(sceFiosCachePrefetchFHSync(nullptr, 0) == FiosOk);
    Require(sceFiosCachePrefetchFHSync(nullptr, 1) == FiosOk);
    Require(sceFiosCachePrefetchFHSync(nullptr, 999) == FiosOk);

    Require(sceFiosCachePrefetchFHRangeSync(nullptr, 0, 0, 1024) == FiosOk);
    Require(sceFiosCachePrefetchFHRangeSync(nullptr, 1, 100, 200) == FiosOk);
    Require(sceFiosCachePrefetchFHRangeSync(nullptr, 999, 0, 500) == FiosOk);
}

void TestCacheContainsSyncBounds() {
    Require(sceFiosCacheContainsFileRangeSync(nullptr, 0, 0) == FiosErrorBadIndex);
    Require(sceFiosCacheContainsFileRangeSync("/app0/data.bin", 0, 1024) == FiosErrorBadIndex);
    Require(sceFiosCacheContainsFileRangeSync("/nonexistent/file.bin", 100, 200) == FiosErrorBadIndex);

    std::uint64_t cached = 0xdeadbeef;
    Require(sceFiosCacheContainsFileSync(nullptr, &cached) == FiosErrorBadPtr);
    Require(cached == 0);

    Require(sceFiosCacheContainsFileSync("/nonexistent/missing/file.bin", &cached) == FiosErrorBadPath);
    Require(cached == 0);

    Require(sceFiosCacheContainsFileSync(nullptr, nullptr) == FiosErrorBadPtr);
    Require(sceFiosCacheContainsFileSync("/nonexistent/missing/file.bin", nullptr) == FiosErrorBadPath);
}

void TestIOFilterCacheDescriptorBounds() {
    alignas(64) std::uint8_t workBuffer[0x20000]{};
    const char dummyPath[] = "/cache/destination";

    Require(sceFiosIOFilterCache(nullptr, 0, nullptr) == FiosErrorBadPtr);

    CacheFilterConfigTest config{};
    config.size = sizeof(CacheFilterConfigTest);
    config.workBufferSize = sizeof(workBuffer);
    config.blockSize = 0x10000;
    config.workBuffer = workBuffer;
    config.path = dummyPath;
    config.flags = 0;

    RamCacheContextTest context{};
    context.size = sizeof(RamCacheContextTest);
    context.workBufferSize = sizeof(workBuffer);
    context.blockSize = 0x10000;
    context.workBuffer = workBuffer;
    context.path = dummyPath;
    context.flags = 0;

    config.size = sizeof(CacheFilterConfigTest) - 1;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    config.size = sizeof(CacheFilterConfigTest);

    config.workBufferSize = 0x10000;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    config.workBufferSize = sizeof(workBuffer);

    config.blockSize = 0;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    config.blockSize = sizeof(workBuffer) + 1;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    config.blockSize = 0x10000;

    config.workBuffer = nullptr;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadPtr);
    config.workBuffer = workBuffer + 1;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadPtr);
    config.workBuffer = workBuffer;

    config.path = nullptr;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadPtr);
    config.path = dummyPath;

    config.flags = 0x10000000ull;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadFlags);
    config.flags = 0;

    Require(sceFiosIOFilterCache(&config, 1, &context) == FiosErrorBadIndex);
    Require(sceFiosIOFilterCache(&config, -1, &context) == FiosErrorBadIndex);

    Require(sceFiosIOFilterCache(&config, 0, nullptr) == FiosErrorBadPtr);

    context.size = sizeof(RamCacheContextTest) + 8;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    context.size = sizeof(RamCacheContextTest);

    context.workBufferSize = 0x1000;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    context.workBufferSize = sizeof(workBuffer);

    context.blockSize = 0x8000;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    context.blockSize = 0x10000;

    context.workBuffer = nullptr;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    context.workBuffer = workBuffer + 8;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    context.workBuffer = workBuffer;

    context.path = nullptr;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadSize);
    context.path = dummyPath;

    const char mismatchPath[] = "/cache/other";
    context.path = mismatchPath;
    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadOverlay);
    context.path = dummyPath;

    Require(sceFiosIOFilterCache(&config, 0, &context) == FiosErrorBadOverlay);
}

} // namespace

int main() {
    TestCacheFlushBounds();
    TestCachePrefetchFileBounds();
    TestCachePrefetchFHBounds();
    TestCacheContainsSyncBounds();
    TestIOFilterCacheDescriptorBounds();
    return 0;
}
