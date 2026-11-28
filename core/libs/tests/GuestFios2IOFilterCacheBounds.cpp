#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", location.line());
        std::fflush(stderr);
        std::abort();
    }
}

constexpr std::int32_t FilterNotHandled = static_cast<std::int32_t>(0x8082001Du);

struct PsarcDescriptorTest {
    std::uint64_t size;
    std::uint64_t workBufferSize;
    std::uint64_t workBuffer;
    std::uint64_t flags;
    std::uint64_t reserved[3];
};
static_assert(sizeof(PsarcDescriptorTest) == 0x38);

struct CacheFilterConfigTest {
    std::uint64_t size{};
    std::uint64_t workBufferSize{};
    std::uint64_t blockSize{};
    void* workBuffer{};
    const char* path{};
    std::uint64_t flags{};
    std::uint64_t reserved[3]{};
};
static_assert(sizeof(CacheFilterConfigTest) == 0x48);

struct RamCacheContextTest {
    std::uint64_t size{};
    std::uint64_t workBufferSize{};
    std::uint64_t blockSize{};
    void* workBuffer{};
    const char* path{};
    std::uint64_t flags{};
    std::uint64_t reserved[3]{};
};
static_assert(sizeof(RamCacheContextTest) == 0x48);

struct OverlayRecordTest {
    std::uint8_t type{};
    std::uint8_t order{};
    std::uint16_t destinationLength{};
    std::uint16_t sourceLength{};
    std::uint16_t reserved{};
    std::int32_t processId{};
    std::int32_t id{};
    char destination[292]{};
    char source[292]{};
};
static_assert(sizeof(OverlayRecordTest) == 0x258);

struct FilterTracker {
    std::int32_t lastOp{-1};
    void* lastData{nullptr};
    std::int32_t callCount{0};
    std::int32_t returnCode{0};
};

FilterTracker g_tracker{};

int APS5_VABI MockFilterCallback(void* context, std::int32_t operation, void* data) {
    auto* tracker = static_cast<FilterTracker*>(context);
    if (tracker != nullptr) {
        tracker->lastOp = operation;
        tracker->lastData = data;
        ++tracker->callCount;
        return tracker->returnCode;
    }
    return 0;
}

void TestIOFilterLifecycleAndTableBounds() {
    Require(sceFiosIOFilterAdd(-1, MockFilterCallback, &g_tracker) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterAdd(256, MockFilterCallback, &g_tracker) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterAdd(10, nullptr, &g_tracker) == SCE_FIOS_ERROR_BAD_PTR);

    g_tracker.returnCode = SCE_FIOS_ERROR_BAD_FLAGS;
    Require(sceFiosIOFilterAdd(10, MockFilterCallback, &g_tracker) == SCE_FIOS_ERROR_BAD_FLAGS);
    Require(sceFiosIOFilterGetInfo(10, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);

    g_tracker.returnCode = 0;
    Require(sceFiosIOFilterAdd(10, MockFilterCallback, &g_tracker) == SCE_FIOS_OK);
    Require(g_tracker.callCount == 2);
    Require(g_tracker.lastOp == 0);

    Require(sceFiosIOFilterAdd(10, MockFilterCallback, &g_tracker) == SCE_FIOS_ERROR_BAD_INDEX);

    Require(sceFiosIOFilterGetInfo(-1, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterGetInfo(256, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterGetInfo(11, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);

    void* retrievedCallback = nullptr;
    void* retrievedContext = nullptr;
    Require(sceFiosIOFilterGetInfo(10, &retrievedCallback, &retrievedContext) == SCE_FIOS_OK);
    Require(retrievedCallback == reinterpret_cast<void*>(MockFilterCallback));
    Require(retrievedContext == &g_tracker);

    Require(sceFiosIOFilterRemove(-1) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterRemove(256) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterRemove(11) == SCE_FIOS_ERROR_BAD_INDEX);

    Require(sceFiosIOFilterRemove(10) == SCE_FIOS_OK);
    Require(g_tracker.callCount == 3);
    Require(g_tracker.lastOp == 1);

    Require(sceFiosIOFilterRemove(10) == SCE_FIOS_ERROR_BAD_INDEX);

    Require(sceFiosIOFilterAdd(50, MockFilterCallback, &g_tracker) == SCE_FIOS_OK);
    sceFiosTerminate();
    Require(sceFiosIOFilterGetInfo(50, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
}

void TestPsarcDearchiverModesAndDescriptorValidation() {
    alignas(64) std::uint8_t psarcWorkBuffer[4096]{};
    PsarcDescriptorTest desc{};
    desc.size = sizeof(PsarcDescriptorTest);
    desc.workBufferSize = sizeof(psarcWorkBuffer);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(psarcWorkBuffer);
    desc.flags = 0;
    desc.reserved[0] = 0;
    desc.reserved[1] = 0;
    desc.reserved[2] = 0;

    Require(sceFiosIOFilterPsarcDearchiver(nullptr, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    desc.workBuffer = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(psarcWorkBuffer);

    desc.size = 0x30;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.size = sizeof(PsarcDescriptorTest);

    desc.workBufferSize = 0;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.workBufferSize = sizeof(psarcWorkBuffer);

    desc.workBuffer = reinterpret_cast<std::uint64_t>(psarcWorkBuffer) + 8;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_ALIGNMENT);
    desc.workBuffer = reinterpret_cast<std::uint64_t>(psarcWorkBuffer);

    desc.flags = 2;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);
    desc.flags = 0;

    desc.reserved[0] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    desc.reserved[0] = 0;

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 0, nullptr) == SCE_FIOS_OK);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, nullptr) == FilterNotHandled);

    char descBuffer[64]{};
    Require(sceFiosIOFilterPsarcDearchiver(&desc, 2, descBuffer) == SCE_FIOS_OK);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, -1, nullptr) == FilterNotHandled);

    std::uint64_t dummyOp = 1;
    Require(sceFiosIOFilterPsarcDearchiver(&desc, -1, &dummyOp) == SCE_FIOS_OK);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 5, &dummyOp) == FilterNotHandled);

    Require(sceFiosIOFilterPsarcDearchiver(&desc, 1, nullptr) == SCE_FIOS_OK);

    Require(sceFiosIOFilterAdd(5, sceFiosIOFilterPsarcDearchiver, &desc) == SCE_FIOS_OK);

    void* cb = nullptr;
    void* ctx = nullptr;
    Require(sceFiosIOFilterGetInfo(5, &cb, &ctx) == SCE_FIOS_OK);
    Require(cb == reinterpret_cast<void*>(sceFiosIOFilterPsarcDearchiver));
    Require(ctx == &desc);

    Require(sceFiosIOFilterRemove(5) == SCE_FIOS_OK);
}

void TestCacheFilterAndOverlayResolutionBounds() {
    char resolveBuffer[0x400]{};
    Require(sceFiosOverlayResolveSync(0x400, "/app0", resolveBuffer, sizeof(resolveBuffer)) == SCE_FIOS_ERROR_BAD_RESOLVE_FLAG);
    Require(sceFiosOverlayResolveSync(0, "/app0", resolveBuffer, 0x401) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0, nullptr, resolveBuffer, sizeof(resolveBuffer)) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayResolveSync(0, "/app0", nullptr, sizeof(resolveBuffer)) == SCE_FIOS_ERROR_BAD_PTR);

    Require(sceFiosResolveSync(nullptr, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    alignas(8) std::uint8_t reqBuffer[0x410]{};
    alignas(8) std::uint8_t resBuffer[0x410]{};
    std::memset(reqBuffer, 0x5a, sizeof(reqBuffer));

    Require(sceFiosResolveSync(nullptr, reqBuffer, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosResolveSync(nullptr, nullptr, resBuffer) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosResolveSync(nullptr, reqBuffer, resBuffer) == SCE_FIOS_OK);
    Require(std::memcmp(reqBuffer, resBuffer, sizeof(reqBuffer)) == 0);

    sceFiosTerminate();
    Require(sceFiosResolve(nullptr, reqBuffer, resBuffer) == 0);

    Require(sceFiosIOFilterCache(nullptr, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    alignas(64) std::uint8_t cacheWorkBuffer[0x20000]{};
    CacheFilterConfigTest config{};
    config.size = 0x40;
    config.workBufferSize = sizeof(cacheWorkBuffer);
    config.blockSize = 4096;
    config.workBuffer = cacheWorkBuffer;
    config.path = "/app0";
    config.flags = 0;

    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    config.size = sizeof(CacheFilterConfigTest);

    config.workBufferSize = 0x10000;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    config.workBufferSize = sizeof(cacheWorkBuffer);

    config.blockSize = 0;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    config.blockSize = 4096;

    config.blockSize = sizeof(cacheWorkBuffer) + 1;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    config.blockSize = 4096;

    config.workBuffer = nullptr;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    config.workBuffer = cacheWorkBuffer;

    config.workBuffer = cacheWorkBuffer + 1;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    config.workBuffer = cacheWorkBuffer;

    config.path = nullptr;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    config.path = "/app0";

    config.flags = 0x20;
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);
    config.flags = 0;

    Require(sceFiosIOFilterCache(&config, 1, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterCache(&config, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    alignas(64) std::uint8_t ramContextWorkBuffer[0x20000]{};
    RamCacheContextTest ramCtx{};
    ramCtx.size = sizeof(RamCacheContextTest);
    ramCtx.workBufferSize = sizeof(ramContextWorkBuffer);
    ramCtx.blockSize = 4096;
    ramCtx.workBuffer = ramContextWorkBuffer;
    ramCtx.path = "/app0";
    ramCtx.flags = 0;

    ramCtx.size = 0x40;
    Require(sceFiosIOFilterCache(&config, 0, &ramCtx) == SCE_FIOS_ERROR_BAD_SIZE);
    ramCtx.size = sizeof(RamCacheContextTest);

    ramCtx.workBuffer = ramContextWorkBuffer + 8;
    Require(sceFiosIOFilterCache(&config, 0, &ramCtx) == SCE_FIOS_ERROR_BAD_SIZE);
    ramCtx.workBuffer = ramContextWorkBuffer;

    ramCtx.path = "/system";
    Require(sceFiosIOFilterCache(&config, 0, &ramCtx) == SCE_FIOS_ERROR_BAD_OVERLAY);
    ramCtx.path = "/app0";

    Require(sceFiosIOFilterCache(&config, 0, &ramCtx) == SCE_FIOS_ERROR_BAD_OVERLAY);

    OverlayRecordTest overlay{};
    overlay.type = 0;
    overlay.order = 1;
    std::strncpy(overlay.destination, "/app0", sizeof(overlay.destination) - 1);
    std::strncpy(overlay.source, "/hostapp", sizeof(overlay.source) - 1);
    std::int32_t overlayId = 0;
    Require(sceFiosOverlayAdd(&overlay, &overlayId) == SCE_FIOS_OK);
    Require(overlayId > 0);

    Require(sceFiosIOFilterCache(&config, 0, &ramCtx) == SCE_FIOS_OK);
}

void TestCachePrefetchAndFlushSyncNoOps() {
    sceFiosCacheFlushSync();
    sceFiosCacheFlushFileSync("/app0/file.bin");
    sceFiosCacheFlushFileRangeSync("/app0/file.bin", 0, 4096);

    Require(sceFiosCachePrefetchFHRangeSync(nullptr, 1, 0, 4096) == SCE_FIOS_OK);
    Require(sceFiosCachePrefetchFHSync(nullptr, 1) == SCE_FIOS_OK);
    Require(sceFiosCachePrefetchFileRangeSync(nullptr, "/app0/file.bin", 0, 4096) == SCE_FIOS_OK);
    Require(sceFiosCacheContainsFileSync("/invalid_path_xyz_12345.bin", nullptr) != SCE_FIOS_OK);
}

} // namespace

int main() {
    TestIOFilterLifecycleAndTableBounds();
    TestPsarcDearchiverModesAndDescriptorValidation();
    TestCacheFilterAndOverlayResolutionBounds();
    TestCachePrefetchAndFlushSyncNoOps();
    return 0;
}
