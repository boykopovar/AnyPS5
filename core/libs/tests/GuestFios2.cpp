#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "Testing.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static std::atomic<int> g_completed{0};
static std::atomic<int> g_deleted{0};
static std::atomic<int> g_lastError{0};
static std::atomic<int> g_lastOp{0};

static int APS5_VABI CountEvents(void* context, SceFiosOp op, SceFiosOpEvent event, int error) {
    Require(context == reinterpret_cast<void*>(0x1234));
    g_lastOp = op;
    if (event == SCE_FIOS_OPEVENT_COMPLETE) {
        g_lastError = error;
        ++g_completed;
    } else if (event == SCE_FIOS_OPEVENT_DELETE) {
        ++g_deleted;
    }
    return 0;
}

static std::int32_t g_filterModes[8]{};
static int g_filterCalls = 0;

static int APS5_VABI FilterCallback(void*, std::int32_t mode, void*) {
    if (g_filterCalls < 8) g_filterModes[g_filterCalls] = mode;
    ++g_filterCalls;
    return 0;
}

struct OverlayRecordTest {
    std::uint8_t type;
    std::uint8_t order;
    std::uint16_t destinationLength;
    std::uint16_t sourceLength;
    std::uint16_t reserved;
    std::int32_t processId;
    std::int32_t id;
    char destination[292];
    char source[292];
};

static_assert(sizeof(OverlayRecordTest) == 0x258);

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

static SceFiosOpAttr CountingAttr() {
    SceFiosOpAttr attr{};
    attr.pCallback = CountEvents;
    attr.pCallbackContext = reinterpret_cast<void*>(0x1234);
    return attr;
}

static void CheckInitialization() {
    Require(!sceFiosIsInitialized(nullptr));
    std::uint32_t params[16] = {};
    params[0] = (sizeof(params) << 1) | 0u;
    params[3] = 0xfeedbeef;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);
    Require(sceFiosArchiveSetDecompressorThreadCount(5) == 2);
    Require(sceFiosArchiveSetDecompressorThreadCount(1) == 1);
    Require(sceFiosArchiveSetDecompressorThreadCount(0) == 1);
    Require(sceFiosArchiveSetDecompressorThreadCount(2) == 2);
    Require(sceFiosArchiveGetDecompressorThreadCount() == 2);
    std::uint32_t copy[16] = {};
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(copy)));
    Require(copy[3] == 0xfeedbeef);
    params[3] = 0x1;
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(params));
    Require(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(copy)) && copy[3] == 0x1);
    Require(sceFiosDLLInitialize() != nullptr);
    Require(sceFiosDLLTerminate() == 0);
    Require(sceFiosIsInitialized(nullptr));
}

static void CheckQueries(const std::string& directory, const std::string& file) {
    Require(sceFiosIOFilterAdd(0, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosIOFilterAdd(-1, FilterCallback, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterAdd(256, FilterCallback, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosIOFilterAdd(0, FilterCallback, reinterpret_cast<void*>(0x1234)) == SCE_FIOS_OK);
    Require(g_filterCalls == 1 && g_filterModes[0] == 0);
    Require(sceFiosIOFilterAdd(0, FilterCallback, reinterpret_cast<void*>(0x5678)) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(g_filterCalls == 1);
    Require(sceFiosIOFilterRemove(0) == SCE_FIOS_OK && g_filterCalls == 2 && g_filterModes[1] == 1);
    Require(sceFiosIOFilterAdd(0, FilterCallback, reinterpret_cast<void*>(0x1234)) == SCE_FIOS_OK && g_filterModes[2] == 0);
    void* filterCallback = nullptr;
    void* filterContext = nullptr;
    Require(sceFiosIOFilterGetInfo(0, &filterCallback, &filterContext) == SCE_FIOS_OK);
    Require(filterCallback != nullptr && filterContext == reinterpret_cast<void*>(0x1234));
    Require(sceFiosIOFilterGetInfo(0, nullptr, nullptr) == SCE_FIOS_OK);
    Require(sceFiosIOFilterGetInfo(256, &filterCallback, &filterContext) == SCE_FIOS_ERROR_BAD_INDEX);
    std::uint8_t resolveRequest[0x410]{};
    std::uint8_t resolveResult[0x410]{};
    resolveRequest[0] = 0x10;
    Require(sceFiosResolveSync(nullptr, resolveRequest, resolveResult) == SCE_FIOS_OK);
    Require(std::memcmp(resolveRequest, resolveResult, sizeof(resolveRequest)) == 0);
    std::memset(resolveResult, 0, sizeof(resolveResult));
    const SceFiosOp resolveOp = sceFiosResolve(nullptr, resolveRequest, resolveResult);
    Require(resolveOp != 0);
    Require(sceFiosOpSyncWait(resolveOp) == SCE_FIOS_OK);
    Require(std::memcmp(resolveRequest, resolveResult, sizeof(resolveRequest)) == 0);
    sceFiosOpDelete(resolveOp);
    int cacheConfig = 0;
    int cacheContext = 0;
    Require(sceFiosIOFilterCache(nullptr, 0, &cacheContext) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosIOFilterCache(&cacheConfig, 0, &cacheContext) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosIOFilterCache(&cacheConfig, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    alignas(64) std::uint8_t psarcWork[0x100]{};
    std::uint64_t psarc[7]{0x38, sizeof(psarcWork), reinterpret_cast<std::uint64_t>(psarcWork), 0, 0, 0, 0};
    std::uint64_t psarcBad[7]{};
    std::memcpy(psarcBad, psarc, sizeof(psarc));
    psarcBad[0] = 0x30;
    Require(sceFiosIOFilterPsarcDearchiver(psarcBad, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    std::memcpy(psarcBad, psarc, sizeof(psarc));
    psarcBad[2] += 8;
    Require(sceFiosIOFilterPsarcDearchiver(psarcBad, 0, nullptr) == SCE_FIOS_ERROR_BAD_ALIGNMENT);
    std::memcpy(psarcBad, psarc, sizeof(psarc));
    psarcBad[3] = 2;
    Require(sceFiosIOFilterPsarcDearchiver(psarcBad, 0, nullptr) == SCE_FIOS_ERROR_BAD_FLAGS);
    std::memcpy(psarcBad, psarc, sizeof(psarc));
    psarcBad[6] = 1;
    Require(sceFiosIOFilterPsarcDearchiver(psarcBad, 0, nullptr) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosIOFilterPsarcDearchiver(psarc, 0, nullptr) == SCE_FIOS_OK);
    char psarcText[0x100]{};
    Require(sceFiosIOFilterPsarcDearchiver(psarc, 2, psarcText) == SCE_FIOS_OK);
    Require(std::strcmp(psarcText, "description not implemented") == 0);
    Require(sceFiosIOFilterPsarcDearchiver(psarc, 2, nullptr) == static_cast<std::int32_t>(0x8082001Du));
    Require(sceFiosIOFilterPsarcDearchiver(psarc, -1, nullptr) == static_cast<std::int32_t>(0x8082001Du));
    std::uint16_t psarcSlot = 7;
    Require(sceFiosIOFilterPsarcDearchiver(psarc, -1, &psarcSlot) == SCE_FIOS_OK && psarcSlot == 7);
    Require(sceFiosIOFilterAdd(1, reinterpret_cast<SceFiosIOFilterCallback>(sceFiosIOFilterPsarcDearchiver), psarc) == SCE_FIOS_OK);
    Require(sceFiosIOFilterRemove(1) == SCE_FIOS_OK);
    Require(sceFiosIOFilterPsarcDearchiver(psarc, 3, psarcText) == static_cast<std::int32_t>(0x8082001Du));
    Require(sceFiosIOFilterPsarcDearchiver(psarc, 1, nullptr) == SCE_FIOS_OK);
    Require(sceFiosIOFilterPsarcDearchiver(nullptr, 0, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosIOFilterPsarcDearchiver(nullptr, 1, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosIOFilterPsarcDearchiver(psarc, 2, psarcText) == SCE_FIOS_ERROR_BAD_PTR);
    char overlayResult[0x400]{};
    Require(sceFiosOverlayResolveSync(0, file.c_str(), overlayResult, sizeof(overlayResult)) == SCE_FIOS_OK && file == overlayResult);
    Require(sceFiosOverlayResolveSync(1, file.c_str(), overlayResult, sizeof(overlayResult)) == SCE_FIOS_OK && file == overlayResult);
    Require(sceFiosOverlayResolveSync(0, file.c_str(), overlayResult, 0x401) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOverlayResolveSync(0, nullptr, overlayResult, sizeof(overlayResult)) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayResolveSync(0, file.c_str(), nullptr, sizeof(overlayResult)) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayResolveSync(0, file.c_str(), overlayResult, file.size()) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    std::memset(overlayResult, 0, sizeof(overlayResult));
    Require(sceFiosOverlayResolveSync(2, file.c_str(), overlayResult, sizeof(overlayResult)) == SCE_FIOS_ERROR_BAD_RESOLVE_FLAG && file == overlayResult);
    Require(sceFiosOverlayResolveSync(-1, file.c_str(), overlayResult, sizeof(overlayResult)) == SCE_FIOS_ERROR_BAD_RESOLVE_FLAG);
    OverlayRecordTest overlay{};
    overlay.type = 0;
    overlay.order = 1;
    std::memcpy(overlay.destination, directory.c_str(), directory.size() + 1);
    std::memcpy(overlay.source, file.c_str(), file.size() + 1);
    std::int32_t overlayId = 0;
    Require(sceFiosOverlayAdd(nullptr, &overlayId) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOverlayAdd(&overlay, &overlayId) == SCE_FIOS_OK);
    Require(overlayId > 0);
    OverlayRecordTest overlayInfo{};
    Require(sceFiosOverlayGetInfo(overlayId, &overlayInfo) == SCE_FIOS_OK);
    Require(overlayInfo.id == overlayId && overlayInfo.type == overlay.type && overlayInfo.order == overlay.order);
    Require(overlayInfo.destinationLength == directory.size() && overlayInfo.sourceLength == file.size());
    std::int32_t overlayIds[4]{};
    std::uint64_t overlayCount = 0;
    Require(sceFiosOverlayGetList(overlayIds, 4, &overlayCount) == SCE_FIOS_OK);
    Require(overlayCount == 1 && overlayIds[0] == overlayId);
    alignas(64) std::uint8_t cacheWork[0x20000]{};
    CacheFilterConfigTest validCacheConfig{sizeof(CacheFilterConfigTest), sizeof(cacheWork), 0x10000, cacheWork, directory.c_str(), 0, {}};
    RamCacheContextTest validCacheContext{sizeof(RamCacheContextTest), sizeof(cacheWork), 0x10000, cacheWork, directory.c_str(), 0, {}};
    Require(sceFiosIOFilterCache(&validCacheConfig, 0, &validCacheContext) == SCE_FIOS_OK);
    Require(sceFiosIOFilterCache(&validCacheConfig, 1, &validCacheContext) == SCE_FIOS_ERROR_BAD_INDEX);
    overlay.type = 2;
    Require(sceFiosOverlayModify(overlayId, &overlay) == SCE_FIOS_OK);
    Require(sceFiosOverlayGetInfo(overlayId, &overlayInfo) == SCE_FIOS_OK && overlayInfo.type == 2);
    Require(sceFiosOverlayRemove(overlayId) == SCE_FIOS_OK);
    Require(sceFiosOverlayGetInfo(overlayId, &overlayInfo) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayRemove(overlayId) == SCE_FIOS_ERROR_BAD_OVERLAY);
    Require(sceFiosOverlayGetList(overlayIds, 4, &overlayCount) == SCE_FIOS_OK && overlayCount == 0);
    SceFiosSize mountSize = 0;
    SceFiosFH archive = 0;
    Require(sceFiosArchiveGetMountBufferSize(nullptr, file.c_str(), &mountSize) == 0);
    Require(sceFiosArchiveGetMountBufferSizeSync(nullptr, file.c_str(), &mountSize) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveMount(nullptr, &archive, file.c_str(), directory.c_str(), nullptr, 0, nullptr) == 0);
    Require(sceFiosArchiveMountSync(nullptr, &archive, file.c_str(), directory.c_str(), nullptr, 0, nullptr) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveMountWithOrder(nullptr, &archive, file.c_str(), directory.c_str(), nullptr, 0, nullptr, 0) == 0);
    Require(sceFiosArchiveMountWithOrderSync(nullptr, &archive, file.c_str(), directory.c_str(), nullptr, 0, nullptr, 0) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosArchiveUnmount(nullptr, INT32_MAX) == 0);
    Require(sceFiosArchiveUnmountSync(nullptr, INT32_MAX) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    sceFiosCacheFlushSync();
    sceFiosCacheFlushFileSync(file.c_str());
    sceFiosCacheFlushFileRangeSync(file.c_str(), 0, 10);
    Require(sceFiosCachePrefetchFileSync(nullptr, file.c_str()) == SCE_FIOS_ERROR_UNIMPLEMENTED);
    Require(sceFiosCachePrefetchFileRangeSync(nullptr, file.c_str(), 0, 10) == SCE_FIOS_OK);
    Require(sceFiosCachePrefetchFile(nullptr, file.c_str()) == 0);
    Require(sceFiosCachePrefetchFileRange(nullptr, file.c_str(), 0, 10) == 0);
    Require(sceFiosCacheContainsFileRangeSync(file.c_str(), 0, 10) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosCacheContainsFileRangeSync((directory + "/missing").c_str(), 0, 1) == SCE_FIOS_ERROR_BAD_INDEX);
    Require(sceFiosExistsSync(nullptr, file.c_str()));
    Require(sceFiosExistsSync(nullptr, directory.c_str()));
    Require(!sceFiosExistsSync(nullptr, (directory + "/missing").c_str()));
    Require(sceFiosFileExistsSync(nullptr, file.c_str()));
    Require(!sceFiosFileExistsSync(nullptr, directory.c_str()));
    Require(sceFiosDirectoryExistsSync(nullptr, directory.c_str()));
    Require(!sceFiosDirectoryExistsSync(nullptr, file.c_str()));
    Require(!sceFiosExistsSync(nullptr, nullptr));
    Require(!sceFiosExistsSync(nullptr, ""));

    Require(sceFiosFileGetSizeSync(nullptr, file.c_str()) == 10);
    Require(sceFiosFileGetSizeSync(nullptr, directory.c_str()) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(sceFiosFileGetSizeSync(nullptr, (directory + "/missing").c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosFileGetSizeSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileGetSizeSync(nullptr, "") == SCE_FIOS_ERROR_BAD_PATH);
    const std::string tooLong(2000, 'a');
    Require(sceFiosFileGetSizeSync(nullptr, tooLong.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    SceFiosStat stat{};
    std::memset(&stat, 0xaa, sizeof(stat));
    Require(sceFiosStatSync(nullptr, file.c_str(), &stat) == SCE_FIOS_OK);
    Require(stat.fileSize == 10 && (stat.statFlags & SCE_FIOS_STAT_DIRECTORY) == 0);
    Require(stat.modificationDate > 1000000000ull * 1000000000ull);
    Require((stat.mode & 0170000) == 0100000);
    Require(sceFiosStatSync(nullptr, directory.c_str(), &stat) == SCE_FIOS_OK);
    Require((stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0);
    Require(sceFiosStatSync(nullptr, (directory + "/missing").c_str(), &stat) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosStatSync(nullptr, file.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);

    SceFiosStat asyncStat{};
    std::memset(&asyncStat, 0xaa, sizeof(asyncStat));
    const SceFiosOp statOp = sceFiosStat(nullptr, file.c_str(), &asyncStat);
    Require(sceFiosOpGetError(statOp) == SCE_FIOS_OK && asyncStat.fileSize == 10 && (asyncStat.statFlags & SCE_FIOS_STAT_DIRECTORY) == 0);
    Require(sceFiosOpSyncWait(statOp) == 0);
    const SceFiosOp directoryStat = sceFiosStat(nullptr, directory.c_str(), &asyncStat);
    Require(sceFiosOpGetError(directoryStat) == SCE_FIOS_OK && (asyncStat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0);
    Require(sceFiosOpSyncWait(directoryStat) == 0);
    const SceFiosOp missingStat = sceFiosStat(nullptr, (directory + "/missing").c_str(), &asyncStat);
    Require(sceFiosOpGetError(missingStat) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosOpSyncWait(missingStat) == SCE_FIOS_ERROR_BAD_PATH);
}

static void CheckPathCompare() {
    Require(sceFiosPathcmp("a/b", "a/b") == 0);
    Require(sceFiosPathcmp("", "") == 0);
    Require(sceFiosPathcmp("a", "b") < 0);
    Require(sceFiosPathcmp("b", "a") > 0);
    Require(sceFiosPathcmp("A", "a") < 0);
    Require(sceFiosPathcmp("ab", "a") > 0);
    Require(sceFiosPathcmp("a", "ab") < 0);
    Require(sceFiosPathncmp("abcdef", "abcxyz", 3) == 0);
    Require(sceFiosPathncmp("abcdef", "abcxyz", 4) < 0);
    Require(sceFiosPathncmp("abcxyz", "abcdef", 4) > 0);
    Require(sceFiosPathncmp("ab", "ac", 10) < 0);
    Require(sceFiosPathncmp("abc", "xyz", 0) == 0);
}

static void CheckOpWaitUntilDelegation(const std::string& file) {
    Require(sceFiosIsInitialized(nullptr));
    const SceFiosOp op = sceFiosFileGetSize(nullptr, file.c_str());
    Require(op > 0);
    Require(sceFiosOpIsDone(op));
    Require(sceFiosOpWaitUntil(op, 0) == SCE_FIOS_OK);
    Require(sceFiosOpWaitUntil(op, 1000000000000000000LL) == SCE_FIOS_OK);
    Require(sceFiosOpGetError(op) == SCE_FIOS_OK);
    sceFiosOpDelete(op);
    Require(sceFiosOpWaitUntil(op, 1000000000000000000LL) == SCE_FIOS_ERROR_BAD_OP);
}

static void CheckOperationLifecycle(const std::string& file) {
    const SceFiosOp op = sceFiosFileGetSize(nullptr, file.c_str());
    Require(op > 0);
    Require(sceFiosIsValidHandle(op));
    Require(sceFiosOpIsDone(op));
    Require(sceFiosOpGetError(op) == SCE_FIOS_OK);
    Require(sceFiosOpWait(op) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(op) == 10);
    Require(sceFiosOpGetPath(op) != nullptr && file == sceFiosOpGetPath(op));
    Require(sceFiosOpGetAttr(op) != nullptr && !sceFiosOpIsCancelled(op));
    Require(sceFiosOpGetOffset(op) == 0);
    std::vector<SceFiosOp> listed(8);
    Require(sceFiosGetAllOps(listed.data(), UINT32_MAX) >= 1);
    Require(listed[0] == op);
    Require(sceFiosOpSyncWaitForIO(op) == 10);
    Require(!sceFiosIsValidHandle(op));
    Require(sceFiosOpGetError(op) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpWait(op) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpSyncWait(op) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpGetActualCount(op) == SCE_FIOS_ERROR_BAD_OP);
    Require(sceFiosOpGetPath(op) == nullptr);
    sceFiosOpDelete(op);

    bool exists = false;
    const SceFiosOp existsOp = sceFiosExists(nullptr, file.c_str(), &exists);
    Require(exists && sceFiosOpGetActualCount(existsOp) == 1);
    Require(sceFiosOpSyncWait(existsOp) == 1);
    const SceFiosOp missingOp = sceFiosExists(nullptr, "fios2_probe/none", &exists);
    Require(!exists && sceFiosOpSyncWait(missingOp) == 0);

    const SceFiosOp directoryOp = sceFiosDirectoryExists(nullptr, "fios2_probe", &exists);
    Require(exists && sceFiosOpGetActualCount(directoryOp) == 1);
    Require(sceFiosOpSyncWait(directoryOp) == 1);
    const SceFiosOp fileAsDirectoryOp = sceFiosDirectoryExists(nullptr, file.c_str(), &exists);
    Require(!exists && sceFiosOpSyncWait(fileAsDirectoryOp) == 0);

    const SceFiosOp failed = sceFiosFileGetSize(nullptr, "fios2_probe/none");
    Require(sceFiosOpGetError(failed) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosOpSyncWait(failed) == SCE_FIOS_ERROR_BAD_PATH);
}

static void CheckCallbacks(const std::string& file) {
    g_completed = 0;
    g_deleted = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp op = sceFiosFileExists(&attr, file.c_str(), nullptr);
    Require(g_completed == 1 && g_lastOp == op && g_lastError == SCE_FIOS_OK);
    Require(g_deleted == 0);
    sceFiosOpDelete(op);
    Require(g_deleted == 1);
    Require(sceFiosFileExistsSync(&attr, file.c_str()));
    Require(g_completed == 2 && g_deleted == 2);

    const SceFiosOp failed = sceFiosFileGetSize(&attr, "fios2_probe/none");
    Require(g_completed == 3 && g_lastError == SCE_FIOS_ERROR_BAD_PATH);
    sceFiosOpDelete(failed);

    SceFiosOpAttr stored{};
    Require(!sceFiosGetGlobalDefaultOpAttr(&stored));
    SceFiosOpAttr global = CountingAttr();
    global.userTag = 77;
    sceFiosSetGlobalDefaultOpAttr(&global);
    Require(sceFiosGetGlobalDefaultOpAttr(&stored) && stored.userTag == 77);
    SceFiosOpAttr effective{};
    sceFiosGetDefaultOpAttr(&effective);
    Require(effective.userTag == 77);
    const SceFiosOp usesGlobal = sceFiosFileExists(nullptr, file.c_str(), nullptr);
    Require(sceFiosOpGetAttr(usesGlobal)->userTag == 77 && g_completed == 4);
    sceFiosOpDelete(usesGlobal);

    SceFiosOpAttr thread = CountingAttr();
    thread.userTag = 88;
    sceFiosSetThreadDefaultOpAttr(&thread);
    Require(sceFiosGetThreadDefaultOpAttr(&stored) && stored.userTag == 88);
    std::thread other([&] {
        SceFiosOpAttr seen{};
        Require(!sceFiosGetThreadDefaultOpAttr(&seen));
        sceFiosGetDefaultOpAttr(&seen);
        Require(seen.userTag == 77);
    });
    other.join();
    const SceFiosOp usesThread = sceFiosFileExists(nullptr, file.c_str(), nullptr);
    Require(sceFiosOpGetAttr(usesThread)->userTag == 88);
    sceFiosOpDelete(usesThread);
    sceFiosSetThreadDefaultOpAttr(nullptr);
    Require(!sceFiosGetThreadDefaultOpAttr(&stored));
    sceFiosSetGlobalDefaultOpAttr(nullptr);
    Require(!sceFiosGetGlobalDefaultOpAttr(&stored));
}

static void CheckSuspension(const std::string& file) {
    Require(!sceFiosIsSuspended() && sceFiosGetSuspendCount() == 0 && sceFiosIsIdle());
    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    sceFiosSuspend();
    sceFiosSuspend();
    Require(sceFiosIsSuspended() && sceFiosGetSuspendCount() == 2);
    const SceFiosOp first = sceFiosFileGetSize(&attr, file.c_str());
    const SceFiosOp second = sceFiosFileGetSize(&attr, file.c_str());
    Require(!sceFiosOpIsDone(first) && !sceFiosOpIsDone(second) && !sceFiosIsIdle());
    sceFiosOpRescheduleWithPriority(first, nullptr, 127);
    sceFiosOpRescheduleWithPriority(second, nullptr, -127);
    sceFiosOpReschedule(INT32_MAX, nullptr);
    Require(!sceFiosOpIsDone(first) && !sceFiosOpIsDone(second));
    Require(sceFiosOpGetError(first) == 1 && g_completed == 0);
    sceFiosResume();
    Require(!sceFiosOpIsDone(first) && sceFiosGetSuspendCount() == 1);
    std::thread resumer([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sceFiosResume();
    });
    Require(sceFiosOpWait(first) == SCE_FIOS_OK);
    resumer.join();
    Require(sceFiosOpIsDone(second) && sceFiosIsIdle() && g_completed == 2);
    Require(sceFiosOpGetActualCount(first) == 10 && sceFiosOpGetActualCount(second) == 10);
    sceFiosOpDelete(first);
    sceFiosOpDelete(second);
    sceFiosResume();
    Require(sceFiosGetSuspendCount() == 0);

    sceFiosSuspend();
    const SceFiosOp cancelled = sceFiosFileGetSize(&attr, file.c_str());
    const SceFiosOp alsoCancelled = sceFiosFileGetSize(&attr, file.c_str());
    sceFiosOpCancel(cancelled);
    Require(sceFiosOpIsDone(cancelled) && sceFiosOpIsCancelled(cancelled));
    Require(sceFiosOpGetError(cancelled) == SCE_FIOS_ERROR_CANCELLED);
    Require(g_lastError == SCE_FIOS_ERROR_CANCELLED);
    sceFiosCancelAllOps();
    Require(sceFiosOpGetError(alsoCancelled) == SCE_FIOS_ERROR_CANCELLED && sceFiosIsIdle());
    sceFiosResume();
    Require(sceFiosOpSyncWaitForIO(cancelled) == SCE_FIOS_ERROR_CANCELLED);
    sceFiosOpDelete(alsoCancelled);

    sceFiosSuspend();
    const SceFiosOp abandoned = sceFiosFileGetSize(nullptr, file.c_str());
    sceFiosOpDelete(abandoned);
    Require(!sceFiosIsValidHandle(abandoned));
    sceFiosResume();
    sceFiosResume();
    Require(sceFiosGetSuspendCount() == 0);

    std::atomic<bool> returned{false};
    std::thread watchdog([&] {
        for (int waited = 0; waited < 200 && !returned.load(); ++waited) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!returned.load()) {
            std::fputs("guest_fios2: sceFiosFileGetSizeSync did not return while suspended\n", stderr);
            std::_Exit(1);
        }
    });
    sceFiosSuspend();
    const SceFiosSize syncSize = sceFiosFileGetSizeSync(&attr, file.c_str());
    sceFiosResume();
    returned.store(true);
    watchdog.join();
    Require(syncSize == 10 && sceFiosGetSuspendCount() == 0 && sceFiosIsIdle());
}

static void CheckMutations(const std::string& directory) {
    const std::string sub = directory + "/sub";
    const std::string target = sub + "/a.bin";
    Require(sceFiosDirectoryCreateSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDirectoryCreateSync(nullptr, sub.c_str()) == SCE_FIOS_ERROR_ALREADY_EXISTS);
    Require(sceFiosDirectoryCreateSync(nullptr, (directory + "/none/deeper").c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosDirectoryCreateSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDirectoryExistsSync(nullptr, sub.c_str()));

    Require(sceFiosFileWriteSync(nullptr, target.c_str(), "hello world", 11, 0) == 11);
    char buffer[32] = {};
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 11, 0) == 11 && std::memcmp(buffer, "hello world", 11) == 0);
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 5, 6) == 5 && std::memcmp(buffer, "world", 5) == 0);
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 100, 6) == 5);
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 4, 500) == 0);
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 0, 0) == 0);
    Require(sceFiosFileReadSync(nullptr, (sub + "/none").c_str(), buffer, 4, 0) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFileReadSync(nullptr, target.c_str(), nullptr, 4, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileReadSync(nullptr, sub.c_str(), buffer, 4, 0) < 0);
    Require(sceFiosFileWriteSync(nullptr, target.c_str(), "xyzzy", 5, 20) == 5);
    Require(sceFiosFileGetSizeSync(nullptr, target.c_str()) == 25);
    Require(sceFiosFileWriteSync(nullptr, target.c_str(), nullptr, 5, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileWriteSync(nullptr, (sub + "/none/x").c_str(), "a", 1, 0) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosFileWriteSync(nullptr, (sub + "/created.bin").c_str(), "a", 1, 0) == 1);
    Require(sceFiosFileDeleteSync(nullptr, (sub + "/created.bin").c_str()) == SCE_FIOS_OK);

    std::memset(buffer, 0, sizeof(buffer));
    const SceFiosOp read = sceFiosFileRead(nullptr, target.c_str(), buffer, 5, 6);
    Require(sceFiosOpGetRequestCount(read) == 5 && sceFiosOpGetOffset(read) == 6 && sceFiosOpGetBuffer(read) == buffer);
    Require(sceFiosOpGetActualCount(read) == 5 && sceFiosOpSyncWaitForIO(read) == 5 && std::memcmp(buffer, "world", 5) == 0);

    Require(sceFiosFileTruncateSync(nullptr, target.c_str(), 4) == SCE_FIOS_OK);
    Require(sceFiosFileGetSizeSync(nullptr, target.c_str()) == 4);
    Require(sceFiosFileTruncateSync(nullptr, target.c_str(), -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileTruncateSync(nullptr, (sub + "/none").c_str(), 4) == SCE_FIOS_ERROR_BAD_PATH);

    const std::string renamed = sub + "/b.bin";
    Require(sceFiosRenameSync(nullptr, target.c_str(), renamed.c_str()) == SCE_FIOS_OK);
    Require(!sceFiosExistsSync(nullptr, target.c_str()) && sceFiosFileExistsSync(nullptr, renamed.c_str()));
    Require(sceFiosRenameSync(nullptr, target.c_str(), renamed.c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosRenameSync(nullptr, renamed.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosRenameSync(nullptr, nullptr, renamed.c_str()) == SCE_FIOS_ERROR_BAD_PTR);

    Require(sceFiosFileDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(sceFiosDirectoryDeleteSync(nullptr, renamed.c_str()) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    Require(sceFiosDirectoryDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_ERROR_ACCESS);
    Require(sceFiosFileDeleteSync(nullptr, renamed.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileDeleteSync(nullptr, renamed.c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosDirectoryDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDirectoryCreateSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileWriteSync(nullptr, renamed.c_str(), "a", 1, 0) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosFileWriteSync(nullptr, (directory + "/c.bin").c_str(), "a", 1, 0) == 1);
    Require(sceFiosDeleteSync(nullptr, (directory + "/c.bin").c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, (directory + "/c.bin").c_str()) == SCE_FIOS_ERROR_BAD_PATH);

    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp created = sceFiosDirectoryCreate(&attr, sub.c_str());
    Require(g_completed == 1 && sceFiosOpGetError(created) == SCE_FIOS_OK);
    sceFiosOpDelete(created);
    const SceFiosOp removed = sceFiosDelete(&attr, sub.c_str());
    Require(g_completed == 2 && sceFiosOpSyncWait(removed) == SCE_FIOS_OK);
}

static void CheckDirectoryCreateWithModeAnswers(const std::string& directory) {
    Require(sceFiosIsInitialized(nullptr));
    const std::string withMode = directory + "/mode777";
    const SceFiosOp created = sceFiosDirectoryCreateWithMode(nullptr, withMode.c_str(), 0x1ff);
    Require(created > 0);
    Require(sceFiosOpWait(created) == SCE_FIOS_OK);
    sceFiosOpDelete(created);
    Require(sceFiosDirectoryExistsSync(nullptr, withMode.c_str()));

    const SceFiosOp duplicate = sceFiosDirectoryCreateWithMode(nullptr, withMode.c_str(), 0x1b6);
    Require(sceFiosOpWait(duplicate) == SCE_FIOS_ERROR_ALREADY_EXISTS);
    sceFiosOpDelete(duplicate);
    Require(sceFiosDirectoryCreateWithModeSync(nullptr, withMode.c_str(), 0x1b6) == SCE_FIOS_ERROR_ALREADY_EXISTS);

    const std::string syncForm = directory + "/mode666";
    Require(sceFiosDirectoryCreateWithModeSync(nullptr, syncForm.c_str(), 0x1b6) == SCE_FIOS_OK);
    Require(sceFiosDirectoryExistsSync(nullptr, syncForm.c_str()));

    Require(sceFiosDirectoryDeleteSync(nullptr, withMode.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDirectoryDeleteSync(nullptr, syncForm.c_str()) == SCE_FIOS_OK);
}

static void CheckFileHandles(const std::string& directory) {
    const std::string path = directory + "/handle.bin";
    SceFiosOpenParams writer{};
    writer.openFlags = (SCE_FIOS_O_READ | SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT);
    SceFiosFH fh = 0;
    Require(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosFHOpenSync(nullptr, nullptr, path.c_str(), &writer) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHOpenSync(nullptr, &fh, nullptr, &writer) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHOpenSync(nullptr, &fh, directory.c_str(), nullptr) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), &writer) == SCE_FIOS_OK);
    Require(fh > 0 && sceFiosIsValidHandle(fh));
    Require(sceFiosCachePrefetchFHSync(nullptr, fh) == SCE_FIOS_OK);
    Require(sceFiosCachePrefetchFHRangeSync(nullptr, fh, 0, 6) == SCE_FIOS_OK);
    Require(sceFiosCachePrefetchFH(nullptr, fh) == 0);
    Require(sceFiosCachePrefetchFHRange(nullptr, fh, 0, 6) == 0);
    Require(path == sceFiosFHGetPath(fh) && sceFiosFHGetOpenParams(fh)->openFlags == writer.openFlags);
    Require(sceFiosFHGetSize(fh) == 0);
    std::uint32_t listed[4] = {};
    Require(sceFiosGetAllFHs(reinterpret_cast<SceFiosFH*>(listed), 4) == 1 && listed[0] == static_cast<std::uint32_t>(fh));

    Require(sceFiosFHWriteSync(nullptr, fh, "abcdef", 6) == 6);
    Require(sceFiosFHTell(fh) == 6 && sceFiosFHGetSize(fh) == 6);
    Require(sceFiosFHSeek(fh, 1, SCE_FIOS_SEEK_SET) == 1);
    char buffer[16] = {};
    Require(sceFiosFHReadSync(nullptr, fh, buffer, 3) == 3 && std::memcmp(buffer, "bcd", 3) == 0);
    Require(sceFiosFHSeek(fh, -1, SCE_FIOS_SEEK_END) == 5 && sceFiosFHSeek(fh, 1, SCE_FIOS_SEEK_CUR) == 6);
    Require(sceFiosFHReadSync(nullptr, fh, buffer, 3) == 0);
    Require(sceFiosFHSeek(fh, 0, 7) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFHSeek(fh, -100, SCE_FIOS_SEEK_SET) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFHPwriteSync(nullptr, fh, "XY", 2, 1) == 2 && sceFiosFHTell(fh) == 6);
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceFiosFHPreadSync(nullptr, fh, buffer, 6, 0) == 6 && std::memcmp(buffer, "aXYdef", 6) == 0);
    Require(sceFiosFHPreadSync(nullptr, fh, buffer, 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFHReadSync(nullptr, fh, buffer, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHReadSync(nullptr, fh, nullptr, 4) == SCE_FIOS_ERROR_BAD_PTR);

    char first[2] = {};
    char second[3] = {};
    const SceFiosBuffer reads[2] = {{first, sizeof(first)}, {second, sizeof(second)}};
    Require(sceFiosFHPreadvSync(nullptr, fh, reads, 2, 1) == 5 && std::memcmp(first, "XY", 2) == 0 && std::memcmp(second, "def", 3) == 0);
    Require(sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_SET) == 0);
    Require(sceFiosFHReadvSync(nullptr, fh, reads, 2) == 5 && std::memcmp(first, "aX", 2) == 0 && std::memcmp(second, "Yde", 3) == 0);
    const SceFiosBuffer writes[2] = {{const_cast<char*>("12"), 2}, {const_cast<char*>("345"), 3}};
    Require(sceFiosFHPwritevSync(nullptr, fh, writes, 2, 6) == 5 && sceFiosFHGetSize(fh) == 11);
    Require(sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_END) == 11);
    Require(sceFiosFHWritevSync(nullptr, fh, writes, 2) == 5 && sceFiosFHGetSize(fh) == 16);
    Require(sceFiosFHReadvSync(nullptr, fh, reads, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHReadvSync(nullptr, fh, reads, 5000) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHReadvSync(nullptr, fh, nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);

    SceFiosStat stat{};
    Require(sceFiosFHStatSync(nullptr, fh, &stat) == SCE_FIOS_OK && stat.fileSize == 16);
    Require(sceFiosFHStatSync(nullptr, fh, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHTruncateSync(nullptr, fh, 4) == SCE_FIOS_OK && sceFiosFHGetSize(fh) == 4);
    Require(sceFiosFHTruncateSync(nullptr, fh, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHSyncSync(nullptr, fh) == SCE_FIOS_OK);

    const SceFiosOp op = sceFiosFHPread(nullptr, fh, buffer, 4, 0);
    Require(sceFiosOpGetRequestCount(op) == 4 && sceFiosOpGetOffset(op) == 0 && path == sceFiosOpGetPath(op));
    Require(sceFiosOpSyncWaitForIO(op) == 4 && std::memcmp(buffer, "aXYd", 4) == 0);

    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);
    Require(!sceFiosIsValidHandle(fh) && sceFiosFHGetPath(fh) == nullptr && sceFiosFHGetOpenParams(fh) == nullptr);
    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_ERROR_BAD_FH);
    Require(sceFiosFHReadSync(nullptr, fh, buffer, 1) == SCE_FIOS_ERROR_BAD_FH);
    Require(sceFiosFHSeek(fh, 0, 0) == SCE_FIOS_ERROR_BAD_FH && sceFiosFHTell(fh) == SCE_FIOS_ERROR_BAD_FH);
    Require(sceFiosFHGetSize(fh) == SCE_FIOS_ERROR_BAD_FH);
    Require(sceFiosFHStatSync(nullptr, fh, &stat) == SCE_FIOS_ERROR_BAD_FH);
    Require(sceFiosFHSyncSync(nullptr, fh) == SCE_FIOS_ERROR_BAD_FH);

    SceFiosOpenParams reader{};
    reader.openFlags = SCE_FIOS_O_READ;
    Require(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), &reader) == SCE_FIOS_OK);
    Require(sceFiosFHWriteSync(nullptr, fh, "z", 1) == SCE_FIOS_ERROR_READ_ONLY);
    Require(sceFiosFHTruncateSync(nullptr, fh, 1) == SCE_FIOS_ERROR_READ_ONLY);
    Require(sceFiosFHPreadSync(nullptr, fh, buffer, 4, 0) == 4);
    sceFiosCloseAllFiles();
    Require(!sceFiosIsValidHandle(fh) && sceFiosGetAllFHs(nullptr, 0) == 0);

    SceFiosOpenParams writeOnly{};
    writeOnly.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_TRUNC;
    Require(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), &writeOnly) == SCE_FIOS_OK);
    Require(sceFiosFHGetSize(fh) == 0 && sceFiosFHReadSync(nullptr, fh, buffer, 1) == SCE_FIOS_ERROR_WRITE_ONLY);
    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    SceFiosOpenParams appender{};
    appender.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_APPEND | SCE_FIOS_O_CREAT;
    const std::string created = directory + "/appended.bin";
    Require(sceFiosFHOpenWithModeSync(nullptr, &fh, created.c_str(), &appender, 0644) == SCE_FIOS_OK);
    Require(sceFiosFHWriteSync(nullptr, fh, "ab", 2) == 2 && sceFiosFHWriteSync(nullptr, fh, "cd", 2) == 2);
    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);
    Require(sceFiosFileGetSizeSync(nullptr, created.c_str()) == 4);

    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp opened = sceFiosFHOpen(&attr, &fh, created.c_str(), &reader);
    Require(g_completed == 1 && sceFiosOpGetError(opened) == SCE_FIOS_OK && sceFiosIsValidHandle(fh));
    sceFiosOpDelete(opened);
    const SceFiosOp closed = sceFiosFHClose(&attr, fh);
    Require(g_completed == 2 && sceFiosOpSyncWait(closed) == SCE_FIOS_OK);

    sceFiosSuspend();
    const SceFiosOp deferred = sceFiosFHOpen(nullptr, &fh, created.c_str(), &reader);
    Require(!sceFiosOpIsDone(deferred));
    sceFiosResume();
    Require(sceFiosOpSyncWait(deferred) == SCE_FIOS_OK && sceFiosIsValidHandle(fh));
    sceFiosShutdownAndCancelOps();
    Require(!sceFiosIsValidHandle(fh));
    Require(sceFiosDeleteSync(nullptr, created.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, path.c_str()) == SCE_FIOS_OK);
}

static void CheckOpenFlagsAndVectors(const std::string& directory, const std::string& file) {
    Require(sceFiosIsInitialized(nullptr));
    SceFiosFH opened = 0x1234;
    const SceFiosOp dirOpen = sceFiosFHOpen(nullptr, &opened, directory.c_str(), nullptr);
    Require(sceFiosOpWait(dirOpen) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(opened == 0x1234);
    sceFiosOpDelete(dirOpen);

    const SceFiosOp dirOpenMode = sceFiosFHOpenWithMode(nullptr, &opened, directory.c_str(), nullptr, 0);
    Require(sceFiosOpWait(dirOpenMode) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(opened == 0x1234);
    sceFiosOpDelete(dirOpenMode);

    opened = 0;
    const SceFiosOp fileOpen = sceFiosFHOpen(nullptr, &opened, file.c_str(), nullptr);
    Require(sceFiosOpWait(fileOpen) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(fileOpen);

    const SceFiosOp badCount = sceFiosFHReadv(nullptr, opened, nullptr, 0);
    Require(sceFiosOpWait(badCount) == SCE_FIOS_ERROR_BAD_IOVCNT);
    sceFiosOpDelete(badCount);

    std::vector<std::uint8_t> first(8, 'Z');
    std::vector<std::uint8_t> second(4, 'Z');
    std::vector<std::uint8_t> third(4, 'Z');
    const SceFiosBuffer readVecs[3] = {
        { first.data(), 8 },
        { second.data(), 4 },
        { third.data(), 4 },
    };
    const SceFiosOp readv = sceFiosFHReadv(nullptr, opened, readVecs, 3);
    Require(sceFiosOpWait(readv) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(readv) == 10);
    Require(std::memcmp(first.data(), "01234567", 8) == 0);
    Require(second[0] == '8' && second[1] == '9' && second[2] == 'Z' && second[3] == 'Z');
    Require(third[0] == 'Z' && third[1] == 'Z' && third[2] == 'Z' && third[3] == 'Z');
    sceFiosOpDelete(readv);

    const SceFiosOp badCountWrite = sceFiosFHWritev(nullptr, opened, readVecs, 0);
    Require(sceFiosOpWait(badCountWrite) == SCE_FIOS_ERROR_BAD_IOVCNT);
    sceFiosOpDelete(badCountWrite);
    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);

    const std::string written = directory + "/vectored.bin";
    SceFiosOpenParams writer{};
    writer.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT | SCE_FIOS_O_TRUNC;
    opened = 0;
    const SceFiosOp writeOpen = sceFiosFHOpenWithMode(nullptr, &opened, written.c_str(), &writer, 0666);
    Require(sceFiosOpWait(writeOpen) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(writeOpen);

    std::vector<std::uint8_t> head(4, 'a');
    std::vector<std::uint8_t> tail(4, 'e');
    const SceFiosBuffer writeVecs[2] = {
        { head.data(), 4 },
        { tail.data(), 4 },
    };
    const SceFiosOp writev = sceFiosFHWritev(nullptr, opened, writeVecs, 2);
    Require(sceFiosOpWait(writev) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(writev) == 8);
    sceFiosOpDelete(writev);
    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);

    std::uint8_t contents[8]{};
    Require(sceFiosFileReadSync(nullptr, written.c_str(), contents, 8, 0) == 8);
    Require(std::memcmp(contents, "aaaaeeee", 8) == 0);
    Require(sceFiosDeleteSync(nullptr, written.c_str()) == SCE_FIOS_OK);
}

static void CheckAsyncHandleAnswers(const std::string& directory, const std::string& file) {
    Require(sceFiosIsInitialized(nullptr));
    SceFiosFH opened = 0;
    const SceFiosOp open = sceFiosFHOpen(nullptr, &opened, file.c_str(), nullptr);
    Require(sceFiosOpWait(open) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(open);

    std::uint8_t buffer[6]{};
    const SceFiosOp firstRead = sceFiosFHRead(nullptr, opened, buffer, 4);
    Require(sceFiosOpWait(firstRead) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(firstRead) == 4);
    Require(std::memcmp(buffer, "0123", 4) == 0);
    sceFiosOpDelete(firstRead);

    const SceFiosOp secondRead = sceFiosFHRead(nullptr, opened, buffer, 4);
    Require(sceFiosOpWait(secondRead) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(secondRead) == 4);
    Require(std::memcmp(buffer, "4567", 4) == 0);
    sceFiosOpDelete(secondRead);

    const SceFiosOp sync = sceFiosFHSync(nullptr, opened);
    Require(sceFiosOpWait(sync) == SCE_FIOS_OK);
    sceFiosOpDelete(sync);

    SceFiosStat status{};
    const SceFiosOp stat = sceFiosFHStat(nullptr, opened, &status);
    Require(sceFiosOpWait(stat) == SCE_FIOS_OK);
    Require(status.fileSize == 10);
    Require((status.statFlags & 1u) == 0);
    sceFiosOpDelete(stat);

    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);

    const std::string scratch = directory + "/asyncfh.bin";
    SceFiosOpenParams writer{};
    writer.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT | SCE_FIOS_O_TRUNC;
    opened = 0;
    const SceFiosOp writeOpen = sceFiosFHOpenWithMode(nullptr, &opened, scratch.c_str(), &writer, 0666);
    Require(sceFiosOpWait(writeOpen) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(writeOpen);

    const SceFiosOp write = sceFiosFHWrite(nullptr, opened, "ABCDE", 5);
    Require(sceFiosOpWait(write) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(write) == 5);
    sceFiosOpDelete(write);

    const SceFiosOp pwrite = sceFiosFHPwrite(nullptr, opened, "xy", 2, 0);
    Require(sceFiosOpWait(pwrite) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(pwrite) == 2);
    sceFiosOpDelete(pwrite);
    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);

    std::uint8_t contents[5]{};
    Require(sceFiosFileReadSync(nullptr, scratch.c_str(), contents, 5, 0) == 5);
    Require(std::memcmp(contents, "xyCDE", 5) == 0);

    SceFiosOpenParams plainWriter{};
    plainWriter.openFlags = SCE_FIOS_O_WRITE;
    opened = 0;
    const SceFiosOp reopen = sceFiosFHOpenWithMode(nullptr, &opened, scratch.c_str(), &plainWriter, 0666);
    Require(sceFiosOpWait(reopen) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(reopen);

    const SceFiosOp truncate = sceFiosFHTruncate(nullptr, opened, 2);
    Require(sceFiosOpWait(truncate) == SCE_FIOS_OK);
    sceFiosOpDelete(truncate);
    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);

    Require(sceFiosFileGetSizeSync(nullptr, scratch.c_str()) == 2);
    Require(sceFiosDeleteSync(nullptr, scratch.c_str()) == SCE_FIOS_OK);
}

static void CheckCacheContainsFileProbe(const std::string& directory, const std::string& file) {
    Require(sceFiosIsInitialized(nullptr));
    std::uint64_t cached = 0x7B7B7B7B7B7B7B7BULL;
    Require(sceFiosCacheContainsFileSync(file.c_str(), &cached) == SCE_FIOS_OK);
    Require(cached == 0);
    const std::string missing = directory + "/absent.bin";
    Require(sceFiosCacheContainsFileSync(missing.c_str(), &cached) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosCacheContainsFileSync(nullptr, &cached) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosCacheContainsFileSync(file.c_str(), nullptr) == SCE_FIOS_OK);
    Require(sceFiosCacheContainsFileSync(file.c_str(), reinterpret_cast<std::uint64_t*>(0x10)) == SCE_FIOS_ERROR_BAD_PTR);
}

static void CheckAsyncPositionedVectors(const std::string& directory, const std::string& file) {
    Require(sceFiosIsInitialized(nullptr));
    SceFiosFH opened = 0;
    const SceFiosOp open = sceFiosFHOpen(nullptr, &opened, file.c_str(), nullptr);
    Require(sceFiosOpWait(open) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(open);

    std::vector<std::uint8_t> head(4, 'Z');
    std::vector<std::uint8_t> middle(4, 'Z');
    std::vector<std::uint8_t> tail(4, 'Z');
    const SceFiosBuffer readVecs[3] = {
        { head.data(), 4 },
        { middle.data(), 4 },
        { tail.data(), 4 },
    };
    const SceFiosOp preadv = sceFiosFHPreadv(nullptr, opened, readVecs, 3, 2);
    Require(sceFiosOpWait(preadv) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(preadv) == 8);
    Require(std::memcmp(head.data(), "2345", 4) == 0);
    Require(std::memcmp(middle.data(), "6789", 4) == 0);
    Require(tail[0] == 'Z' && tail[1] == 'Z' && tail[2] == 'Z' && tail[3] == 'Z');
    sceFiosOpDelete(preadv);

    std::uint8_t positionProbe[2]{};
    const SceFiosOp plainRead = sceFiosFHRead(nullptr, opened, positionProbe, 2);
    Require(sceFiosOpWait(plainRead) == SCE_FIOS_OK);
    Require(std::memcmp(positionProbe, "01", 2) == 0);
    sceFiosOpDelete(plainRead);

    const SceFiosOp badCount = sceFiosFHPreadv(nullptr, opened, readVecs, 0, 2);
    Require(sceFiosOpWait(badCount) == SCE_FIOS_ERROR_BAD_IOVCNT);
    sceFiosOpDelete(badCount);

    const SceFiosOp badOffset = sceFiosFHPreadv(nullptr, opened, readVecs, 3, -1);
    Require(sceFiosOpWait(badOffset) == SCE_FIOS_ERROR_BAD_OFFSET);
    sceFiosOpDelete(badOffset);

    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);

    const std::string scratch = directory + "/pv.bin";
    SceFiosOpenParams writer{};
    writer.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT | SCE_FIOS_O_TRUNC;
    opened = 0;
    const SceFiosOp writeOpen = sceFiosFHOpenWithMode(nullptr, &opened, scratch.c_str(), &writer, 0666);
    Require(sceFiosOpWait(writeOpen) == SCE_FIOS_OK && sceFiosIsValidHandle(opened));
    sceFiosOpDelete(writeOpen);

    const SceFiosOp seed = sceFiosFHWrite(nullptr, opened, "XXXXX", 5);
    Require(sceFiosOpWait(seed) == SCE_FIOS_OK);
    sceFiosOpDelete(seed);

    std::vector<std::uint8_t> first(2, 'a');
    std::vector<std::uint8_t> second(2, 'b');
    const SceFiosBuffer writeVecs[2] = {
        { first.data(), 2 },
        { second.data(), 2 },
    };
    const SceFiosOp pwritev = sceFiosFHPwritev(nullptr, opened, writeVecs, 2, 1);
    Require(sceFiosOpWait(pwritev) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(pwritev) == 4);
    sceFiosOpDelete(pwritev);

    const SceFiosOp badWriteOffset = sceFiosFHPwritev(nullptr, opened, writeVecs, 2, -1);
    Require(sceFiosOpWait(badWriteOffset) == SCE_FIOS_ERROR_BAD_OFFSET);
    sceFiosOpDelete(badWriteOffset);

    Require(sceFiosFHCloseSync(nullptr, opened) == SCE_FIOS_OK);
    std::uint8_t contents[5]{};
    Require(sceFiosFileReadSync(nullptr, scratch.c_str(), contents, 5, 0) == 5);
    Require(std::memcmp(contents, "Xaabb", 5) == 0);
    Require(sceFiosDeleteSync(nullptr, scratch.c_str()) == SCE_FIOS_OK);
}

static std::int32_t AddOverlay(std::uint8_t type, std::uint8_t order, const std::string& destination, const std::string& source) {
    OverlayRecordTest overlay{};
    overlay.type = type;
    overlay.order = order;
    std::memcpy(overlay.destination, destination.c_str(), destination.size() + 1);
    std::memcpy(overlay.source, source.c_str(), source.size() + 1);
    std::int32_t id = 0;
    Require(sceFiosOverlayAdd(&overlay, &id) == SCE_FIOS_OK && id > 0);
    return id;
}

static std::string Resolve(std::int32_t flag, const std::string& path, std::size_t capacity = 0x400) {
    char result[0x400]{};
    Require(sceFiosOverlayResolveSync(flag, path.c_str(), result, capacity) == SCE_FIOS_OK);
    return result;
}

static void CheckOverlayResolve(const std::string& directory) {
    const std::string base = directory + "/ovlbase";
    const std::string source = directory + "/ovlsrc";
    std::filesystem::create_directories(base);
    std::filesystem::create_directories(source);
    for (const char* name : {"/a.txt", "/older.txt", "/newer.txt"}) std::ofstream(source + name) << "source";
    for (const char* name : {"/b.txt", "/older.txt", "/newer.txt"}) std::ofstream(base + name) << "base";
    const auto now = std::filesystem::last_write_time(source + "/a.txt");
    std::filesystem::last_write_time(base + "/older.txt", now - std::chrono::hours(1));
    std::filesystem::last_write_time(source + "/older.txt", now);
    std::filesystem::last_write_time(base + "/newer.txt", now);
    std::filesystem::last_write_time(source + "/newer.txt", now - std::chrono::hours(1));

    std::int32_t opaque = AddOverlay(0, 5, "/app0/ovltest", source);
    Require(Resolve(0, "/app0/ovltest/a.txt") == source + "/a.txt");
    Require(Resolve(1, "/app0/ovltest/missing.txt") == source + "/missing.txt");
    Require(Resolve(0, "/app0/ovltest") == source && Resolve(0, "/app0/ovltest/") == source);
    Require(Resolve(0, "/app0//ovltest/./a.txt") == source + "/a.txt");
    Require(Resolve(0, "/app0/ovltestx/a.txt") == "/app0/ovltestx/a.txt");
    Require(Resolve(0, "/app0/OVLTEST/a.txt") == "/app0/OVLTEST/a.txt");
    Require(Resolve(0, "ovltest/a.txt") == "ovltest/a.txt");
    char small[0x400]{};
    const std::string tooLong = "/app0/ovltest/a.txt";
    Require(sceFiosOverlayResolveSync(0, tooLong.c_str(), small, tooLong.size() + 1) == SCE_FIOS_ERROR_PATH_TOO_LONG && tooLong == small);
    const std::int32_t first = AddOverlay(0, 1, "/app0/ovltest/", base + "/");
    const std::int32_t nested = AddOverlay(0, 3, "/app0/ovltest/sub", source);
    Require(Resolve(0, "/app0/ovltest/sub/b.txt") == base + "/sub/b.txt");
    Require(Resolve(0, "/app0/ovltest") == base);
    Require(sceFiosOverlayRemove(first) == SCE_FIOS_OK && sceFiosOverlayRemove(nested) == SCE_FIOS_OK);
    Require(sceFiosOverlayRemove(opaque) == SCE_FIOS_OK);

    for (std::uint8_t type : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}}) {
        const std::int32_t id = AddOverlay(type, 0, base, source);
        Require(Resolve(0, base + "/a.txt") == source + "/a.txt");
        Require(Resolve(0, base + "/b.txt") == base + "/b.txt");
        Require(Resolve(1, base + "/b.txt") == (type == 3 ? source : base) + "/b.txt");
        Require(Resolve(0, base + "/older.txt") == source + "/older.txt");
        Require(Resolve(0, base + "/newer.txt") == (type == 2 ? base : source) + "/newer.txt");
        Require(sceFiosOverlayRemove(id) == SCE_FIOS_OK);
    }
}

static void CheckDirectories(const std::string& directory) {
    const std::string root = directory + "/listing";
    Require(sceFiosDirectoryCreateSync(nullptr, root.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileWriteSync(nullptr, (root + "/one.bin").c_str(), "12345", 5, 0) == 5);
    Require(sceFiosFileWriteSync(nullptr, (root + "/two.bin").c_str(), "1", 1, 0) == 1);
    Require(sceFiosDirectoryCreateSync(nullptr, (root + "/inner").c_str()) == SCE_FIOS_OK);

    SceFiosDH dh = 0;
    char storage[256] = {};
    SceFiosBuffer small{storage, 1};
    SceFiosBuffer none{nullptr, 0};
    SceFiosBuffer big{storage, sizeof(storage)};
    Require(sceFiosDHOpenSync(nullptr, &dh, root.c_str(), none) == SCE_FIOS_ERROR_BAD_SIZE);
    const SceFiosOp probe = sceFiosDHOpen(nullptr, &dh, root.c_str(), small);
    Require(sceFiosOpWait(probe) == SCE_FIOS_ERROR_BAD_SIZE);
    const std::int64_t required = sceFiosOpGetActualCount(probe);
    Require(required > 0 && required <= static_cast<std::int64_t>(sizeof(storage)));
    sceFiosOpDelete(probe);
    Require(dh == 0 && sceFiosGetAllDHs(nullptr, 0) == 0);
    Require(sceFiosDHOpenSync(nullptr, &dh, (root + "/missing").c_str(), big) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosDHOpenSync(nullptr, &dh, (root + "/one.bin").c_str(), big) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    Require(sceFiosDHOpenSync(nullptr, nullptr, root.c_str(), big) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDHOpenSync(nullptr, &dh, nullptr, big) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDHOpenSync(nullptr, &dh, root.c_str(), big) == SCE_FIOS_OK);
    Require(sceFiosIsValidHandle(dh) && root == sceFiosDHGetPath(dh) && sceFiosGetAllDHs(nullptr, 0) == 1);

    int files = 0, directories = 0;
    SceFiosDirEntry entry{};
    for (;;) {
        std::memset(&entry, 0x7f, sizeof(entry));
        const int result = sceFiosDHReadSync(nullptr, dh, &entry);
        if (result == SCE_FIOS_ERROR_EOF) break;
        Require(result == SCE_FIOS_OK);
        const std::string name(entry.fullPath + entry.offsetToName, entry.nameLength);
        Require(std::string(entry.fullPath, entry.fullPathLength) == root + "/" + name && entry.fullPath[entry.fullPathLength] == 0);
        if (name == "inner") {
            ++directories;
            Require((entry.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0);
        } else {
            ++files;
            Require(entry.statFlags == 0 && entry.fileSize == (name == "one.bin" ? 5 : 1));
            Require(name == "one.bin" || name == "two.bin");
        }
    }
    Require(files == 2 && directories == 1);
    Require(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_EOF);
    Require(sceFiosDHReadSync(nullptr, dh, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    Require(!sceFiosIsValidHandle(dh) && sceFiosDHGetPath(dh) == nullptr);
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_ERROR_BAD_DH);
    Require(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_BAD_DH);

    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp opened = sceFiosDHOpen(&attr, &dh, (root + "/").c_str(), big);
    Require(g_completed == 1 && sceFiosOpGetError(opened) == SCE_FIOS_OK);
    sceFiosOpDelete(opened);
    const SceFiosOp read = sceFiosDHRead(nullptr, dh, &entry);
    Require(sceFiosOpGetError(read) == SCE_FIOS_OK && entry.fullPathLength > root.size() + 1);
    Require(entry.fullPath[root.size()] == '/' && entry.fullPath[root.size() + 1] != '/');
    sceFiosOpDelete(read);
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    Require(sceFiosDHOpenSync(nullptr, &dh, root.c_str(), big) == SCE_FIOS_OK);
    sceFiosTerminate();
    Require(!sceFiosIsValidHandle(dh));
    Require(sceFiosDeleteSync(nullptr, (root + "/inner").c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, (root + "/one.bin").c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, (root + "/two.bin").c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, root.c_str()) == SCE_FIOS_OK);
}

static void CheckDates() {
    SceFiosTm tm{};
    tm.tm_year = 124;
    tm.tm_mon = 1;
    tm.tm_mday = 29;
    tm.tm_hour = 13;
    tm.tm_min = 14;
    tm.tm_sec = 15;
    const SceFiosDate date = sceFiosDateFromComponents(&tm);
    Require(date == 1709212455ull * 1000000000ull);
    SceFiosTm back{};
    Require(sceFiosDateToComponents(date + 999999999, &back) == &back);
    Require(back.tm_year == 124 && back.tm_mon == 1 && back.tm_mday == 29 && back.tm_hour == 13 && back.tm_min == 14 && back.tm_sec == 15);
    Require(back.tm_wday == 4 && back.tm_yday == 59 && back.tm_isdst == 0);
    Require(sceFiosDateToComponents(0, &back) == &back && back.tm_year == 70 && back.tm_mon == 0 && back.tm_mday == 1 && back.tm_wday == 4 && back.tm_yday == 0);
    Require(sceFiosDateToComponents(date, nullptr) == nullptr && sceFiosDateFromComponents(nullptr) == 0);
    SceFiosTm overflow{};
    overflow.tm_year = 99;
    overflow.tm_mon = 12;
    overflow.tm_mday = 32;
    overflow.tm_hour = 24;
    overflow.tm_min = 60;
    overflow.tm_sec = 60;
    SceFiosTm normalized{};
    sceFiosDateToComponents(sceFiosDateFromComponents(&overflow), &normalized);
    Require(normalized.tm_year == 100 && normalized.tm_mon == 1 && normalized.tm_mday == 2 && normalized.tm_hour == 1 && normalized.tm_min == 1 && normalized.tm_sec == 0);
    SceFiosTm far{};
    far.tm_year = 500;
    far.tm_mday = 1;
    const SceFiosDate farDate = sceFiosDateFromComponents(&far);
    SceFiosTm farBack{};
    Require(farDate != 0 && sceFiosDateToComponents(farDate, &farBack) == &farBack && farBack.tm_year == 500 && farBack.tm_mon == 0 && farBack.tm_mday == 1);
    far.tm_year = 1000;
    Require(sceFiosDateFromComponents(&far) == 0);
    SceFiosTm negative{};
    negative.tm_year = 70;
    negative.tm_mon = -1;
    negative.tm_mday = 1;
    Require(sceFiosDateFromComponents(&negative) == 0);
    const SceFiosDate now = sceFiosDateGetCurrent();
    Require(now > 1700000000ull * 1000000000ull && sceFiosDateGetCurrent() >= now);
    Require(sceFiosTimeIntervalFromNanoseconds(-5) == -5 && sceFiosTimeIntervalToNanoseconds(123456789012) == 123456789012);
}

static void CheckBadPointers(const std::string& file) {
    const auto badAttr = reinterpret_cast<const SceFiosOpAttr*>(0x10);
    Require(sceFiosFileGetSizeSync(badAttr, file.c_str()) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileGetSize(badAttr, file.c_str()) == SCE_FIOS_ERROR_BAD_PTR);
    Require(!sceFiosFileExistsSync(badAttr, file.c_str()));
    Require(sceFiosStatSync(badAttr, file.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    sceFiosSetGlobalDefaultOpAttr(badAttr);
    SceFiosOpAttr stored{};
    Require(!sceFiosGetGlobalDefaultOpAttr(&stored));
    sceFiosSetThreadDefaultOpAttr(badAttr);
    Require(!sceFiosGetThreadDefaultOpAttr(&stored));

    const auto badParams = reinterpret_cast<const SceFiosOpenParams*>(0x10);
    SceFiosFH fh = 0;
    Require(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), badParams) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHOpenWithModeSync(nullptr, &fh, file.c_str(), badParams, -1) == SCE_FIOS_ERROR_BAD_PTR);
    Require(fh == 0 && sceFiosGetAllFHs(nullptr, 0) == 0);

    SceFiosOpenParams reader{};
    reader.openFlags = SCE_FIOS_O_READ;
    Require(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), &reader) == SCE_FIOS_OK);
    const auto badVectors = reinterpret_cast<const SceFiosBuffer*>(0x10);
    Require(sceFiosFHReadvSync(nullptr, fh, badVectors, 2) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHPreadvSync(nullptr, fh, badVectors, 2, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHWritevSync(nullptr, fh, badVectors, 2) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    Require(sceFiosDateFromComponents(reinterpret_cast<const SceFiosTm*>(0x10)) == 0);
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(0x10)) == SCE_FIOS_ERROR_BAD_PTR);
    Require(!sceFiosIsInitialized(nullptr));
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(0x10));
}

int main() {
    const std::filesystem::path directory = "fios2_probe";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::string file = (directory / "data.bin").generic_string();
    {
        std::ofstream out(file, std::ios::binary);
        out << "0123456789";
    }
    CheckInitialization();
    CheckQueries(directory.generic_string(), file);
    CheckPathCompare();
    CheckOperationLifecycle(file);
    CheckOpWaitUntilDelegation(file);
    CheckCallbacks(file);
    CheckSuspension(file);
    CheckMutations(directory.generic_string());
    CheckDirectoryCreateWithModeAnswers(directory.generic_string());
    CheckOpenFlagsAndVectors(directory.generic_string(), file);
    CheckAsyncHandleAnswers(directory.generic_string(), file);
    CheckCacheContainsFileProbe(directory.generic_string(), file);
    CheckAsyncPositionedVectors(directory.generic_string(), file);
    CheckFileHandles(directory.generic_string());
    CheckDirectories(directory.generic_string());
    CheckOverlayResolve(directory.generic_string());
    Require(sceFiosIOFilterGetInfo(0, nullptr, nullptr) == SCE_FIOS_ERROR_BAD_INDEX);
    CheckDates();
    CheckBadPointers(file);
    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
    Require(sceFiosArchiveGetDecompressorThreadCount() == 0);
    std::uint32_t params[16] = {};
    params[0] = sizeof(params) << 1;
    params[6] = 7;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosArchiveSetDecompressorThreadCount(9) == 3);
    sceFiosTerminate();
    std::filesystem::remove_all(directory);
    std::puts("guest_fios2 ok");
    return 0;
}
