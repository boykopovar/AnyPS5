#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios2_queue_bounds check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(condition) Require((condition), __LINE__)

template<typename TFunc>
static void RequireNotImplemented(TFunc&& func, int line) {
    try {
        func();
        std::fprintf(stderr, "Expected exception at line %d\n", line);
        std::abort();
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        if (message.find("not implemented") == std::string::npos) {
            std::fprintf(stderr, "Unexpected exception message at line %d: %s\n", line, error.what());
            std::abort();
        }
    } catch (...) {
        std::fprintf(stderr, "Unexpected non-runtime exception at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE_NOT_IMPLEMENTED(expr) RequireNotImplemented([&] { (void)(expr); }, __LINE__)

constexpr std::int32_t SCE_FIOS2_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80990001u);

static void CheckCacheBlockAlignment(const std::string& directory, const std::string& file) {
    static_assert(alignof(SceFiosBuffer) == 8);
    static_assert(alignof(SceFiosOpAttr) == 8);
    static_assert(alignof(SceFiosOpenParams) == 8);
    static_assert(alignof(SceFiosStat) == 8);
    static_assert(alignof(SceFiosDirEntry) == 8);
    static_assert(alignof(SceFiosTm) == 4);
    static_assert(sizeof(SceFiosBuffer) == 16);
    static_assert(sizeof(SceFiosOpAttr) == 48);
    static_assert(sizeof(SceFiosOpenParams) == 24);
    static_assert(sizeof(SceFiosStat) == 80);
    static_assert(sizeof(SceFiosDirEntry) == 24 + SCE_FIOS_PATH_MAX);

    const std::string target = directory + "/cache_align.bin";
    std::vector<char> block(4096, 'K');
    REQUIRE(sceFiosFileWriteSync(nullptr, target.c_str(), block.data(), 4096, 0) == 4096);

    std::vector<char> readBuffer(4096, 0);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), 512, 0) == 512);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), 512, 512) == 512);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), 4096, 0) == 4096);
    REQUIRE(std::memcmp(readBuffer.data(), block.data(), 4096) == 0);

    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), -4096, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), 512, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), readBuffer.data(), 512, -512) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), nullptr, 512, 0) == SCE_FIOS_ERROR_BAD_PTR);

    SceFiosOpenParams openParams{};
    openParams.openFlags = SCE_FIOS_O_READ | SCE_FIOS_O_WRITE;
    SceFiosFH fh = 0;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, target.c_str(), &openParams) == SCE_FIOS_OK);
    REQUIRE(fh > 0 && sceFiosIsValidHandle(fh));

    char seg1[512] = {};
    char seg2[512] = {};
    const SceFiosBuffer alignedIov[2] = {
        {seg1, sizeof(seg1)},
        {seg2, sizeof(seg2)}
    };
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, alignedIov, 2, 0) == 1024);
    REQUIRE(std::memcmp(seg1, block.data(), 512) == 0);
    REQUIRE(std::memcmp(seg2, block.data() + 512, 512) == 0);

    REQUIRE(sceFiosFHReadvSync(nullptr, fh, alignedIov, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, alignedIov, -1) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, alignedIov, 1025) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, nullptr, 2) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, alignedIov, 2, -1) == SCE_FIOS_ERROR_BAD_OFFSET);

    const SceFiosBuffer badIov[2] = {
        {nullptr, 512},
        {seg2, 512}
    };
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, badIov, 2) == SCE_FIOS_ERROR_BAD_PTR);

    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    SceFiosDH dh = 0;
    char dhStorage[64] = {};
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, directory.c_str(), SceFiosBuffer{nullptr, 0}) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, directory.c_str(), SceFiosBuffer{dhStorage, 1}) == SCE_FIOS_ERROR_BAD_SIZE);

    REQUIRE_NOT_IMPLEMENTED(sceFiosCacheFlushSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCacheFlushFileSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCacheFlushFileRangeSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCacheContainsFileSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCacheContainsFileRangeSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFile());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFileSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFileRange());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFileRangeSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFH());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFHSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFHRange());
    REQUIRE_NOT_IMPLEMENTED(sceFiosCachePrefetchFHRangeSync());

    REQUIRE(sceFiosDeleteSync(nullptr, target.c_str()) == SCE_FIOS_OK);
}

static void CheckAsyncQueueBounds(const std::string& file) {
    SceFiosOpAttr attrMin{};
    attrMin.priority = -128;
    attrMin.userTag = 1001;
    const SceFiosOp opMin = sceFiosFileGetSize(&attrMin, file.c_str());
    REQUIRE(opMin > 0 && sceFiosIsValidHandle(opMin));
    const SceFiosOpAttr* fetchedMin = sceFiosOpGetAttr(opMin);
    REQUIRE(fetchedMin != nullptr && fetchedMin->priority == -128 && fetchedMin->userTag == 1001);
    REQUIRE(sceFiosOpSyncWaitForIO(opMin) >= 0);
    sceFiosOpDelete(opMin);

    SceFiosOpAttr attrMax{};
    attrMax.priority = 127;
    attrMax.userTag = 1002;
    const SceFiosOp opMax = sceFiosFileGetSize(&attrMax, file.c_str());
    REQUIRE(opMax > 0 && sceFiosIsValidHandle(opMax));
    const SceFiosOpAttr* fetchedMax = sceFiosOpGetAttr(opMax);
    REQUIRE(fetchedMax != nullptr && fetchedMax->priority == 127 && fetchedMax->userTag == 1002);
    REQUIRE(sceFiosOpSyncWaitForIO(opMax) >= 0);
    sceFiosOpDelete(opMax);

    sceFiosSuspend();
    REQUIRE(sceFiosIsSuspended());
    REQUIRE(sceFiosGetSuspendCount() == 1);

    std::vector<SceFiosOp> queuedOps;
    for (int i = 0; i < 12; ++i) {
        SceFiosOpAttr attr{};
        attr.priority = static_cast<std::int8_t>(-64 + i * 10);
        attr.userTag = static_cast<std::uint32_t>(2000 + i);
        const SceFiosOp op = sceFiosFileGetSize(&attr, file.c_str());
        REQUIRE(op > 0);
        REQUIRE(sceFiosIsValidHandle(op));
        REQUIRE(!sceFiosOpIsDone(op));
        REQUIRE(sceFiosOpGetError(op) == 1);
        queuedOps.push_back(op);
    }
    REQUIRE(!sceFiosIsIdle());

    const std::uint32_t totalListed = sceFiosGetAllOps(nullptr, 0);
    REQUIRE(totalListed >= 12);

    std::vector<SceFiosOp> smallList(4, 0);
    const std::uint32_t smallReported = sceFiosGetAllOps(smallList.data(), 4);
    REQUIRE(smallReported >= 12);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(smallList[i] > 0);
        REQUIRE(sceFiosIsValidHandle(smallList[i]));
    }

    std::vector<SceFiosOp> fullList(totalListed, 0);
    const std::uint32_t fullReported = sceFiosGetAllOps(fullList.data(), static_cast<std::uint32_t>(fullList.size()));
    REQUIRE(fullReported == totalListed);

    sceFiosOpCancel(queuedOps[0]);
    REQUIRE(sceFiosOpIsCancelled(queuedOps[0]));
    REQUIRE(sceFiosOpGetError(queuedOps[0]) == SCE_FIOS_ERROR_CANCELLED);

    sceFiosCancelAllOps();
    REQUIRE(sceFiosIsIdle());

    for (const SceFiosOp op : queuedOps) {
        REQUIRE(sceFiosOpGetError(op) == SCE_FIOS_ERROR_CANCELLED);
    }

    sceFiosResume();
    REQUIRE(!sceFiosIsSuspended());
    REQUIRE(sceFiosGetSuspendCount() == 0);

    for (const SceFiosOp op : queuedOps) {
        REQUIRE(sceFiosOpSyncWaitForIO(op) == SCE_FIOS_ERROR_CANCELLED);
        sceFiosOpDelete(op);
        REQUIRE(!sceFiosIsValidHandle(op));
        sceFiosOpDelete(op);
    }
}

static void CheckArchiveMountParameters() {
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveMount());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveMountSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveMountWithOrder());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveMountWithOrderSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveUnmount());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveUnmountSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveGetMountBufferSize());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveGetMountBufferSizeSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveGetDecompressorThreadCount());
    REQUIRE_NOT_IMPLEMENTED(sceFiosArchiveSetDecompressorThreadCount());
    REQUIRE_NOT_IMPLEMENTED(sceFiosIOFilterPsarcDearchiver());
    REQUIRE_NOT_IMPLEMENTED(sceFiosIOFilterAdd());
    REQUIRE_NOT_IMPLEMENTED(sceFiosIOFilterRemove());
    REQUIRE_NOT_IMPLEMENTED(sceFiosIOFilterCache());
    REQUIRE_NOT_IMPLEMENTED(sceFiosIOFilterGetInfo());
    REQUIRE_NOT_IMPLEMENTED(sceFiosOverlayAdd());
    REQUIRE_NOT_IMPLEMENTED(sceFiosOverlayRemove());
    REQUIRE_NOT_IMPLEMENTED(sceFiosOverlayGetInfo());
    REQUIRE_NOT_IMPLEMENTED(sceFiosOverlayGetList());
    REQUIRE_NOT_IMPLEMENTED(sceFiosOverlayModify());
    REQUIRE_NOT_IMPLEMENTED(sceFiosOverlayResolveSync());
    REQUIRE_NOT_IMPLEMENTED(sceFiosResolve());
    REQUIRE_NOT_IMPLEMENTED(sceFiosResolveSync());
}

static void CheckInvalidHandleRejection() {
    const SceFiosHandle invalidHandles[] = {
        SCE_FIOS2_ERROR_INVALID_HANDLE,
        0,
        -1,
        -999,
        INT32_MIN,
        INT32_MAX,
        0x7ffffffe,
        0x12345678
    };

    char buffer[32] = {};
    const SceFiosBuffer vbuf{buffer, sizeof(buffer)};
    SceFiosStat stat{};
    SceFiosDirEntry entry{};

    for (const SceFiosHandle h : invalidHandles) {
        REQUIRE(!sceFiosIsValidHandle(h));
        REQUIRE(!sceFiosOpIsDone(h));
        REQUIRE(!sceFiosOpIsCancelled(h));
        REQUIRE(sceFiosOpWait(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpSyncWait(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpSyncWaitForIO(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetError(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetAttr(h) == nullptr);
        REQUIRE(sceFiosOpGetPath(h) == nullptr);
        REQUIRE(sceFiosOpGetBuffer(h) == nullptr);
        REQUIRE(sceFiosOpGetOffset(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetRequestCount(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetActualCount(h) == SCE_FIOS_ERROR_BAD_OP);

        REQUIRE(sceFiosFHCloseSync(nullptr, h) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHStatSync(nullptr, h, &stat) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHTruncateSync(nullptr, h, 0) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHSyncSync(nullptr, h) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHReadSync(nullptr, h, buffer, sizeof(buffer)) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHWriteSync(nullptr, h, buffer, sizeof(buffer)) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHPreadSync(nullptr, h, buffer, sizeof(buffer), 0) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHPwriteSync(nullptr, h, buffer, sizeof(buffer), 0) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHReadvSync(nullptr, h, &vbuf, 1) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHWritevSync(nullptr, h, &vbuf, 1) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHPreadvSync(nullptr, h, &vbuf, 1, 0) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHPwritevSync(nullptr, h, &vbuf, 1, 0) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHSeek(h, 0, SCE_FIOS_SEEK_SET) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHTell(h) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHGetSize(h) == SCE_FIOS_ERROR_BAD_FH);
        REQUIRE(sceFiosFHGetPath(h) == nullptr);
        REQUIRE(sceFiosFHGetOpenParams(h) == nullptr);

        REQUIRE(sceFiosDHCloseSync(nullptr, h) == SCE_FIOS_ERROR_BAD_DH);
        REQUIRE(sceFiosDHReadSync(nullptr, h, &entry) == SCE_FIOS_ERROR_BAD_DH);
        REQUIRE(sceFiosDHGetPath(h) == nullptr);
    }
}

int main() {
    const std::filesystem::path directory = "fios2_queue_bounds_probe";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::string file = (directory / "probe_data.bin").generic_string();
    {
        std::ofstream out(file, std::ios::binary);
        out << "fios2_queue_bounds_payload";
    }

    std::uint32_t params[16] = {};
    params[0] = (sizeof(params) << 1) | 0u;
    REQUIRE(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    REQUIRE(sceFiosIsInitialized(nullptr));

    CheckCacheBlockAlignment(directory.generic_string(), file);
    CheckAsyncQueueBounds(file);
    CheckArchiveMountParameters();
    CheckInvalidHandleRejection();

    sceFiosTerminate();
    REQUIRE(!sceFiosIsInitialized(nullptr));

    std::filesystem::remove_all(directory);
    std::puts("guest_fios2_queue_bounds ok");
    return 0;
}
