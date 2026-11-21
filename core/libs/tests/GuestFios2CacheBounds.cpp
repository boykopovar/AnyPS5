#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios2_cache_bounds check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(condition) Require((condition), __LINE__)

static void CheckCacheBlockAlignment(const std::string& directory, const std::string& file) {
    static_cast<void>(directory);
    std::vector<std::uint8_t> storage(65536 * 2);
    const auto rawAddr = reinterpret_cast<std::uintptr_t>(storage.data());
    const auto alignedAddr = (rawAddr + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    auto* alignedPtr = reinterpret_cast<std::uint8_t*>(alignedAddr);

    SceFiosOpenParams openParams{};
    openParams.openFlags = SCE_FIOS_O_READ | SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT;
    openParams.buffer.pPtr = alignedPtr;
    openParams.buffer.length = 65536;

    SceFiosFH fh = 0;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), &openParams) == SCE_FIOS_OK);
    REQUIRE(fh > 0 && sceFiosIsValidHandle(fh));
    const auto* storedParams = sceFiosFHGetOpenParams(fh);
    REQUIRE(storedParams != nullptr);
    REQUIRE(storedParams->openFlags == openParams.openFlags);
    REQUIRE(storedParams->buffer.pPtr == alignedPtr);
    REQUIRE(storedParams->buffer.length == 65536);

    std::vector<std::uint8_t> blockData(8192);
    for (std::size_t i = 0; i < blockData.size(); ++i) {
        blockData[i] = static_cast<std::uint8_t>((i * 7 + 13) & 0xff);
    }
    REQUIRE(sceFiosFHWriteSync(nullptr, fh, blockData.data(), blockData.size()) == static_cast<SceFiosSize>(blockData.size()));
    REQUIRE(sceFiosFHGetSize(fh) == 8192);

    std::vector<std::uint8_t> alignedRead(512);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, alignedRead.data(), 512, 4096) == 512);
    REQUIRE(std::memcmp(alignedRead.data(), blockData.data() + 4096, 512) == 0);

    std::vector<std::uint8_t> unalignedRead(1024);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, unalignedRead.data(), 1024, 4095) == 1024);
    REQUIRE(std::memcmp(unalignedRead.data(), blockData.data() + 4095, 1024) == 0);

    std::vector<std::uint8_t> smallUnalignedRead(64);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, smallUnalignedRead.data(), 64, 511) == 64);
    REQUIRE(std::memcmp(smallUnalignedRead.data(), blockData.data() + 511, 64) == 0);

    alignas(64) std::uint8_t vec1[512];
    alignas(64) std::uint8_t vec2[1024];
    alignas(64) std::uint8_t vec3[2048];
    for (std::size_t i = 0; i < sizeof(vec1); ++i) vec1[i] = static_cast<std::uint8_t>(i & 0xff);
    for (std::size_t i = 0; i < sizeof(vec2); ++i) vec2[i] = static_cast<std::uint8_t>((i + 3) & 0xff);
    for (std::size_t i = 0; i < sizeof(vec3); ++i) vec3[i] = static_cast<std::uint8_t>((i + 7) & 0xff);

    const SceFiosBuffer writeVecs[3] = {
        { vec1, sizeof(vec1) },
        { vec2, sizeof(vec2) },
        { vec3, sizeof(vec3) }
    };
    const SceFiosSize vecTotal = sizeof(vec1) + sizeof(vec2) + sizeof(vec3);
    REQUIRE(sceFiosFHPwritevSync(nullptr, fh, writeVecs, 3, 8192) == vecTotal);
    REQUIRE(sceFiosFHGetSize(fh) == 8192 + vecTotal);

    std::vector<std::uint8_t> unalignedDst(vecTotal + 32);
    std::uint8_t* misaligned1 = unalignedDst.data() + 1;
    std::uint8_t* misaligned2 = unalignedDst.data() + sizeof(vec1) + 3;
    std::uint8_t* misaligned3 = unalignedDst.data() + sizeof(vec1) + sizeof(vec2) + 7;

    const SceFiosBuffer readVecs[3] = {
        { misaligned1, sizeof(vec1) },
        { misaligned2, sizeof(vec2) },
        { misaligned3, sizeof(vec3) }
    };
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, readVecs, 3, 8192) == vecTotal);
    REQUIRE(std::memcmp(misaligned1, vec1, sizeof(vec1)) == 0);
    REQUIRE(std::memcmp(misaligned2, vec2, sizeof(vec2)) == 0);
    REQUIRE(std::memcmp(misaligned3, vec3, sizeof(vec3)) == 0);

    const SceFiosBuffer zeroVecs[2] = {
        { alignedPtr, 0 },
        { misaligned1, 0 }
    };
    REQUIRE(sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_SET) == 0);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, zeroVecs, 2) == 0);
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, zeroVecs, 2, 0) == 0);

    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);
}

static void CheckBufferAllocationBounds(const std::string& directory, const std::string& file) {
    const std::string subDir = directory + "/dh_bounds";
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, subDir.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileWriteSync(nullptr, (subDir + "/f1.bin").c_str(), "alpha", 5, 0) == 5);
    REQUIRE(sceFiosFileWriteSync(nullptr, (subDir + "/f2.bin").c_str(), "beta", 4, 0) == 4);
    REQUIRE(sceFiosFileWriteSync(nullptr, (subDir + "/f3.bin").c_str(), "gamma", 5, 0) == 5);

    SceFiosDH dh = 0;
    const SceFiosBuffer none{ nullptr, 0 };
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), none) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(dh == 0);

    std::uint8_t probeStorage[8] = {};
    const SceFiosBuffer smallBuf{ probeStorage, sizeof(probeStorage) };
    const SceFiosOp probeOp = sceFiosDHOpen(nullptr, &dh, subDir.c_str(), smallBuf);
    REQUIRE(probeOp > 0);
    REQUIRE(sceFiosOpWait(probeOp) == SCE_FIOS_ERROR_BAD_SIZE);
    const std::int64_t reqBytes = sceFiosOpGetActualCount(probeOp);
    REQUIRE(reqBytes == 37);
    sceFiosOpDelete(probeOp);
    REQUIRE(dh == 0);

    std::vector<std::uint8_t> undersized(static_cast<std::size_t>(reqBytes - 1));
    const SceFiosBuffer underBuf{ undersized.data(), undersized.size() };
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), underBuf) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(dh == 0);

    const SceFiosBuffer nullPtrWithLength{ nullptr, static_cast<std::uint64_t>(reqBytes) };
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), nullPtrWithLength) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(dh == 0);

    std::vector<std::uint8_t> exact(static_cast<std::size_t>(reqBytes));
    const SceFiosBuffer exactBuf{ exact.data(), exact.size() };
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), exactBuf) == SCE_FIOS_OK);
    REQUIRE(dh > 0 && sceFiosIsValidHandle(dh));
    REQUIRE(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    REQUIRE(!sceFiosIsValidHandle(dh));

    std::vector<std::uint8_t> oversized(static_cast<std::size_t>(reqBytes + 256));
    const SceFiosBuffer overBuf{ oversized.data(), oversized.size() };
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), overBuf) == SCE_FIOS_OK);
    REQUIRE(dh > 0 && sceFiosIsValidHandle(dh));
    REQUIRE(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);

    REQUIRE(sceFiosFileDeleteSync(nullptr, (subDir + "/f1.bin").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileDeleteSync(nullptr, (subDir + "/f2.bin").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileDeleteSync(nullptr, (subDir + "/f3.bin").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDirectoryDeleteSync(nullptr, subDir.c_str()) == SCE_FIOS_OK);

    SceFiosFH fh = 0;
    SceFiosOpenParams openP{};
    openP.openFlags = SCE_FIOS_O_READ | SCE_FIOS_O_WRITE;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), &openP) == SCE_FIOS_OK);

    std::uint8_t dummy[64] = {};
    REQUIRE(sceFiosFHReadSync(nullptr, fh, dummy, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHReadSync(nullptr, fh, dummy, -100) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHReadSync(nullptr, fh, dummy, INT64_MIN) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHWriteSync(nullptr, fh, dummy, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHPwriteSync(nullptr, fh, dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHTruncateSync(nullptr, fh, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileReadSync(nullptr, file.c_str(), dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileWriteSync(nullptr, file.c_str(), dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileTruncateSync(nullptr, file.c_str(), -1) == SCE_FIOS_ERROR_BAD_SIZE);

    REQUIRE(sceFiosFHPreadSync(nullptr, fh, dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, dummy, 10, -100) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, dummy, 10, INT64_MIN) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFHPwriteSync(nullptr, fh, dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFileReadSync(nullptr, file.c_str(), dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFileWriteSync(nullptr, file.c_str(), dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);

    REQUIRE(sceFiosFHReadSync(nullptr, fh, nullptr, 16) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHWriteSync(nullptr, fh, nullptr, 16) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHPwriteSync(nullptr, fh, nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileReadSync(nullptr, file.c_str(), nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileWriteSync(nullptr, file.c_str(), nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);

    REQUIRE(sceFiosFHReadSync(nullptr, fh, nullptr, 0) == 0);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, nullptr, 0, 0) == 0);
    REQUIRE(sceFiosFileReadSync(nullptr, file.c_str(), nullptr, 0, 0) == 0);

    const SceFiosBuffer validBuf = { dummy, sizeof(dummy) };
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, &validBuf, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, &validBuf, -1) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, &validBuf, 1025) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, &validBuf, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, &validBuf, -1) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, &validBuf, 1025) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);

    const SceFiosBuffer nullPtrInVec = { nullptr, 32 };
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, &nullPtrInVec, 1) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, &nullPtrInVec, 1, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, &nullPtrInVec, 1) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHPwritevSync(nullptr, fh, &nullPtrInVec, 1, 0) == SCE_FIOS_ERROR_BAD_PTR);

    const SceFiosBuffer overflowVec = { dummy, static_cast<std::uint64_t>(INT64_MAX) + 1ull };
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, &overflowVec, 1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, &overflowVec, 1) == SCE_FIOS_ERROR_BAD_SIZE);

    const SceFiosBuffer twoVecs[2] = {
        { dummy, static_cast<std::uint64_t>(INT64_MAX) - 10ull },
        { dummy, 20ull }
    };
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, twoVecs, 2) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, twoVecs, 2) == SCE_FIOS_ERROR_BAD_SIZE);

    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    const std::string longPath(1025, 'x');
    REQUIRE(sceFiosFileGetSizeSync(nullptr, longPath.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    REQUIRE(!sceFiosFileExistsSync(nullptr, longPath.c_str()));
}

static void CheckInvalidHandleRejectionOnCancel(const std::string& directory, const std::string& file) {
    const SceFiosHandle invalidHandles[] = {
        0,
        -1,
        -2,
        -999,
        static_cast<SceFiosHandle>(0x80000000u),
        0x7ffffffe,
        0x7fffffff,
        999999
    };
    for (const SceFiosHandle h : invalidHandles) {
        sceFiosOpCancel(h);
        REQUIRE(!sceFiosOpIsCancelled(h));
        REQUIRE(!sceFiosIsValidHandle(h));
        REQUIRE(!sceFiosOpIsDone(h));
        REQUIRE(sceFiosOpGetError(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpWait(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpSyncWait(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpSyncWaitForIO(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetOffset(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetRequestCount(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetActualCount(h) == SCE_FIOS_ERROR_BAD_OP);
        REQUIRE(sceFiosOpGetPath(h) == nullptr);
        REQUIRE(sceFiosOpGetBuffer(h) == nullptr);
        REQUIRE(sceFiosOpGetAttr(h) == nullptr);
        sceFiosOpDelete(h);
    }

    SceFiosFH fileHandle = 0;
    SceFiosOpenParams openP{};
    openP.openFlags = SCE_FIOS_O_READ;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fileHandle, file.c_str(), &openP) == SCE_FIOS_OK);
    REQUIRE(fileHandle > 0 && sceFiosIsValidHandle(fileHandle));

    sceFiosOpCancel(fileHandle);
    REQUIRE(!sceFiosOpIsCancelled(fileHandle));
    REQUIRE(sceFiosIsValidHandle(fileHandle));
    REQUIRE(sceFiosFHGetSize(fileHandle) > 0);

    char dirBuf[256] = {};
    SceFiosDH dirHandle = 0;
    REQUIRE(sceFiosDHOpenSync(nullptr, &dirHandle, directory.c_str(), SceFiosBuffer{ dirBuf, sizeof(dirBuf) }) == SCE_FIOS_OK);
    REQUIRE(dirHandle > 0 && sceFiosIsValidHandle(dirHandle));

    sceFiosOpCancel(dirHandle);
    REQUIRE(!sceFiosOpIsCancelled(dirHandle));
    REQUIRE(sceFiosIsValidHandle(dirHandle));

    REQUIRE(sceFiosFHCloseSync(nullptr, fileHandle) == SCE_FIOS_OK);
    REQUIRE(sceFiosDHCloseSync(nullptr, dirHandle) == SCE_FIOS_OK);

    sceFiosOpCancel(fileHandle);
    REQUIRE(!sceFiosOpIsCancelled(fileHandle));
    sceFiosOpCancel(dirHandle);
    REQUIRE(!sceFiosOpIsCancelled(dirHandle));

    const SceFiosOp completedOp = sceFiosFileGetSize(nullptr, file.c_str());
    REQUIRE(completedOp > 0);
    REQUIRE(sceFiosOpIsDone(completedOp));
    REQUIRE(!sceFiosOpIsCancelled(completedOp));
    REQUIRE(sceFiosOpGetError(completedOp) == SCE_FIOS_OK);

    sceFiosOpCancel(completedOp);
    REQUIRE(!sceFiosOpIsCancelled(completedOp));
    REQUIRE(sceFiosOpGetError(completedOp) == SCE_FIOS_OK);

    sceFiosOpDelete(completedOp);
    REQUIRE(!sceFiosIsValidHandle(completedOp));

    sceFiosOpCancel(completedOp);
    REQUIRE(!sceFiosOpIsCancelled(completedOp));
    REQUIRE(sceFiosOpGetError(completedOp) == SCE_FIOS_ERROR_BAD_OP);

    sceFiosSuspend();
    REQUIRE(sceFiosIsSuspended());

    const SceFiosOp qOp1 = sceFiosFileGetSize(nullptr, file.c_str());
    const SceFiosOp qOp2 = sceFiosFileGetSize(nullptr, file.c_str());
    const SceFiosOp qOp3 = sceFiosFileGetSize(nullptr, file.c_str());

    REQUIRE(qOp1 > 0 && qOp2 > 0 && qOp3 > 0);
    REQUIRE(!sceFiosOpIsDone(qOp1) && !sceFiosOpIsDone(qOp2) && !sceFiosOpIsDone(qOp3));

    sceFiosOpCancel(0);
    sceFiosOpCancel(-1);
    sceFiosOpCancel(999999);

    REQUIRE(!sceFiosOpIsCancelled(qOp1));
    REQUIRE(!sceFiosOpIsCancelled(qOp2));
    REQUIRE(!sceFiosOpIsCancelled(qOp3));

    sceFiosOpCancel(qOp2);
    REQUIRE(sceFiosOpIsCancelled(qOp2));
    REQUIRE(!sceFiosOpIsCancelled(qOp1));
    REQUIRE(!sceFiosOpIsCancelled(qOp3));

    sceFiosOpCancel(qOp2);
    REQUIRE(sceFiosOpIsCancelled(qOp2));

    sceFiosResume();
    REQUIRE(!sceFiosIsSuspended());

    REQUIRE(sceFiosOpWait(qOp1) == SCE_FIOS_OK);
    REQUIRE(sceFiosOpWait(qOp2) == SCE_FIOS_ERROR_CANCELLED);
    REQUIRE(sceFiosOpWait(qOp3) == SCE_FIOS_OK);

    sceFiosOpDelete(qOp1);
    sceFiosOpDelete(qOp2);
    sceFiosOpDelete(qOp3);
}

int main() {
    const std::filesystem::path directory = "fios2_cache_bounds_probe";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::string file = (directory / "cache_data.bin").generic_string();

    std::uint32_t params[16] = {};
    params[0] = (sizeof(params) << 1) | 0u;
    params[3] = 0x80000;
    REQUIRE(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    REQUIRE(sceFiosIsInitialized(nullptr));

    CheckCacheBlockAlignment(directory.generic_string(), file);
    CheckBufferAllocationBounds(directory.generic_string(), file);
    CheckInvalidHandleRejectionOnCancel(directory.generic_string(), file);

    sceFiosTerminate();
    REQUIRE(!sceFiosIsInitialized(nullptr));
    std::filesystem::remove_all(directory);
    std::puts("guest_fios2_cache_bounds ok");
    return 0;
}
