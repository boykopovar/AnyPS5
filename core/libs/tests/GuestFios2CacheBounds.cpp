#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "Testing.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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
    Require(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), &openParams) == SCE_FIOS_OK);
    Require(fh > 0 && sceFiosIsValidHandle(fh));
    const auto* storedParams = sceFiosFHGetOpenParams(fh);
    Require(storedParams != nullptr);
    Require(storedParams->openFlags == openParams.openFlags);
    Require(storedParams->buffer.pPtr == alignedPtr);
    Require(storedParams->buffer.length == 65536);

    std::vector<std::uint8_t> blockData(8192);
    for (std::size_t i = 0; i < blockData.size(); ++i) {
        blockData[i] = static_cast<std::uint8_t>((i * 7 + 13) & 0xff);
    }
    Require(sceFiosFHWriteSync(nullptr, fh, blockData.data(), blockData.size()) == static_cast<SceFiosSize>(blockData.size()));
    Require(sceFiosFHGetSize(fh) == 8192);

    std::vector<std::uint8_t> alignedRead(512);
    Require(sceFiosFHPreadSync(nullptr, fh, alignedRead.data(), 512, 4096) == 512);
    Require(std::memcmp(alignedRead.data(), blockData.data() + 4096, 512) == 0);

    std::vector<std::uint8_t> unalignedRead(1024);
    Require(sceFiosFHPreadSync(nullptr, fh, unalignedRead.data(), 1024, 4095) == 1024);
    Require(std::memcmp(unalignedRead.data(), blockData.data() + 4095, 1024) == 0);

    std::vector<std::uint8_t> smallUnalignedRead(64);
    Require(sceFiosFHPreadSync(nullptr, fh, smallUnalignedRead.data(), 64, 511) == 64);
    Require(std::memcmp(smallUnalignedRead.data(), blockData.data() + 511, 64) == 0);

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
    Require(sceFiosFHPwritevSync(nullptr, fh, writeVecs, 3, 8192) == vecTotal);
    Require(sceFiosFHGetSize(fh) == 8192 + vecTotal);

    std::vector<std::uint8_t> unalignedDst(vecTotal + 32);
    std::uint8_t* misaligned1 = unalignedDst.data() + 1;
    std::uint8_t* misaligned2 = unalignedDst.data() + sizeof(vec1) + 3;
    std::uint8_t* misaligned3 = unalignedDst.data() + sizeof(vec1) + sizeof(vec2) + 7;

    const SceFiosBuffer readVecs[3] = {
        { misaligned1, sizeof(vec1) },
        { misaligned2, sizeof(vec2) },
        { misaligned3, sizeof(vec3) }
    };
    Require(sceFiosFHPreadvSync(nullptr, fh, readVecs, 3, 8192) == vecTotal);
    Require(std::memcmp(misaligned1, vec1, sizeof(vec1)) == 0);
    Require(std::memcmp(misaligned2, vec2, sizeof(vec2)) == 0);
    Require(std::memcmp(misaligned3, vec3, sizeof(vec3)) == 0);

    const SceFiosBuffer zeroVecs[2] = {
        { alignedPtr, 0 },
        { misaligned1, 0 }
    };
    Require(sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_SET) == 0);
    Require(sceFiosFHReadvSync(nullptr, fh, zeroVecs, 2) == 0);
    Require(sceFiosFHPreadvSync(nullptr, fh, zeroVecs, 2, 0) == 0);

    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);
}

static void CheckBufferAllocationBounds(const std::string& directory, const std::string& file) {
    const std::string subDir = directory + "/dh_bounds";
    Require(sceFiosDirectoryCreateSync(nullptr, subDir.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/f1.bin").c_str(), "alpha", 5, 0) == 5);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/f2.bin").c_str(), "beta", 4, 0) == 4);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/f3.bin").c_str(), "gamma", 5, 0) == 5);

    SceFiosDH dh = 0;
    const SceFiosBuffer none{ nullptr, 0 };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), none) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(dh == 0);

    std::uint8_t probeStorage[8] = {};
    const SceFiosBuffer smallBuf{ probeStorage, sizeof(probeStorage) };
    const SceFiosOp probeOp = sceFiosDHOpen(nullptr, &dh, subDir.c_str(), smallBuf);
    Require(probeOp > 0);
    Require(sceFiosOpWait(probeOp) == SCE_FIOS_ERROR_BAD_SIZE);
    const std::int64_t reqBytes = sceFiosOpGetActualCount(probeOp);
    Require(reqBytes == 37);
    sceFiosOpDelete(probeOp);
    Require(dh == 0);

    std::vector<std::uint8_t> undersized(static_cast<std::size_t>(reqBytes - 1));
    const SceFiosBuffer underBuf{ undersized.data(), undersized.size() };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), underBuf) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(dh == 0);

    const SceFiosBuffer nullPtrWithLength{ nullptr, static_cast<std::uint64_t>(reqBytes) };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), nullPtrWithLength) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(dh == 0);

    std::vector<std::uint8_t> exact(static_cast<std::size_t>(reqBytes));
    const SceFiosBuffer exactBuf{ exact.data(), exact.size() };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), exactBuf) == SCE_FIOS_OK);
    Require(dh > 0 && sceFiosIsValidHandle(dh));
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    Require(!sceFiosIsValidHandle(dh));

    std::vector<std::uint8_t> oversized(static_cast<std::size_t>(reqBytes + 256));
    const SceFiosBuffer overBuf{ oversized.data(), oversized.size() };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), overBuf) == SCE_FIOS_OK);
    Require(dh > 0 && sceFiosIsValidHandle(dh));
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);

    Require(sceFiosFileDeleteSync(nullptr, (subDir + "/f1.bin").c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileDeleteSync(nullptr, (subDir + "/f2.bin").c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileDeleteSync(nullptr, (subDir + "/f3.bin").c_str()) == SCE_FIOS_OK);
    Require(sceFiosDirectoryDeleteSync(nullptr, subDir.c_str()) == SCE_FIOS_OK);

    SceFiosFH fh = 0;
    SceFiosOpenParams openP{};
    openP.openFlags = SCE_FIOS_O_READ | SCE_FIOS_O_WRITE;
    Require(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), &openP) == SCE_FIOS_OK);

    std::uint8_t dummy[64] = {};
    Require(sceFiosFHReadSync(nullptr, fh, dummy, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHReadSync(nullptr, fh, dummy, -100) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHReadSync(nullptr, fh, dummy, INT64_MIN) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHPreadSync(nullptr, fh, dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHWriteSync(nullptr, fh, dummy, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHPwriteSync(nullptr, fh, dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHTruncateSync(nullptr, fh, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileReadSync(nullptr, file.c_str(), dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileWriteSync(nullptr, file.c_str(), dummy, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileTruncateSync(nullptr, file.c_str(), -1) == SCE_FIOS_ERROR_BAD_SIZE);

    Require(sceFiosFHPreadSync(nullptr, fh, dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFHPreadSync(nullptr, fh, dummy, 10, -100) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFHPreadSync(nullptr, fh, dummy, 10, INT64_MIN) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFHPwriteSync(nullptr, fh, dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFileReadSync(nullptr, file.c_str(), dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFileWriteSync(nullptr, file.c_str(), dummy, 10, -1) == SCE_FIOS_ERROR_BAD_OFFSET);

    Require(sceFiosFHReadSync(nullptr, fh, nullptr, 16) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHPreadSync(nullptr, fh, nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHWriteSync(nullptr, fh, nullptr, 16) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHPwriteSync(nullptr, fh, nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileReadSync(nullptr, file.c_str(), nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileWriteSync(nullptr, file.c_str(), nullptr, 16, 0) == SCE_FIOS_ERROR_BAD_PTR);

    Require(sceFiosFHReadSync(nullptr, fh, nullptr, 0) == 0);
    Require(sceFiosFHPreadSync(nullptr, fh, nullptr, 0, 0) == 0);
    Require(sceFiosFileReadSync(nullptr, file.c_str(), nullptr, 0, 0) == 0);

    const SceFiosBuffer validBuf = { dummy, sizeof(dummy) };
    Require(sceFiosFHReadvSync(nullptr, fh, &validBuf, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHReadvSync(nullptr, fh, &validBuf, -1) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHReadvSync(nullptr, fh, &validBuf, 1025) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHReadvSync(nullptr, fh, nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHWritevSync(nullptr, fh, &validBuf, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHWritevSync(nullptr, fh, &validBuf, -1) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHWritevSync(nullptr, fh, &validBuf, 1025) == SCE_FIOS_ERROR_BAD_IOVCNT);
    Require(sceFiosFHWritevSync(nullptr, fh, nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);

    const SceFiosBuffer nullPtrInVec = { nullptr, 32 };
    Require(sceFiosFHReadvSync(nullptr, fh, &nullPtrInVec, 1) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHPreadvSync(nullptr, fh, &nullPtrInVec, 1, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHWritevSync(nullptr, fh, &nullPtrInVec, 1) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFHPwritevSync(nullptr, fh, &nullPtrInVec, 1, 0) == SCE_FIOS_ERROR_BAD_PTR);

    const SceFiosBuffer overflowVec = { dummy, static_cast<std::uint64_t>(INT64_MAX) + 1ull };
    Require(sceFiosFHReadvSync(nullptr, fh, &overflowVec, 1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHWritevSync(nullptr, fh, &overflowVec, 1) == SCE_FIOS_ERROR_BAD_SIZE);

    const SceFiosBuffer twoVecs[2] = {
        { dummy, static_cast<std::uint64_t>(INT64_MAX) - 10ull },
        { dummy, 20ull }
    };
    Require(sceFiosFHReadvSync(nullptr, fh, twoVecs, 2) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFHWritevSync(nullptr, fh, twoVecs, 2) == SCE_FIOS_ERROR_BAD_SIZE);

    Require(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    const std::string longPath(1025, 'x');
    Require(sceFiosFileGetSizeSync(nullptr, longPath.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    Require(!sceFiosFileExistsSync(nullptr, longPath.c_str()));
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
        Require(!sceFiosOpIsCancelled(h));
        Require(!sceFiosIsValidHandle(h));
        Require(!sceFiosOpIsDone(h));
        Require(sceFiosOpGetError(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpWait(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpSyncWait(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpSyncWaitForIO(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpGetOffset(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpGetRequestCount(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpGetActualCount(h) == SCE_FIOS_ERROR_BAD_OP);
        Require(sceFiosOpGetPath(h) == nullptr);
        Require(sceFiosOpGetBuffer(h) == nullptr);
        Require(sceFiosOpGetAttr(h) == nullptr);
        sceFiosOpDelete(h);
    }

    SceFiosFH fileHandle = 0;
    SceFiosOpenParams openP{};
    openP.openFlags = SCE_FIOS_O_READ;
    Require(sceFiosFHOpenSync(nullptr, &fileHandle, file.c_str(), &openP) == SCE_FIOS_OK);
    Require(fileHandle > 0 && sceFiosIsValidHandle(fileHandle));

    sceFiosOpCancel(fileHandle);
    Require(!sceFiosOpIsCancelled(fileHandle));
    Require(sceFiosIsValidHandle(fileHandle));
    Require(sceFiosFHGetSize(fileHandle) > 0);

    char dirBuf[256] = {};
    SceFiosDH dirHandle = 0;
    Require(sceFiosDHOpenSync(nullptr, &dirHandle, directory.c_str(), SceFiosBuffer{ dirBuf, sizeof(dirBuf) }) == SCE_FIOS_OK);
    Require(dirHandle > 0 && sceFiosIsValidHandle(dirHandle));

    sceFiosOpCancel(dirHandle);
    Require(!sceFiosOpIsCancelled(dirHandle));
    Require(sceFiosIsValidHandle(dirHandle));

    Require(sceFiosFHCloseSync(nullptr, fileHandle) == SCE_FIOS_OK);
    Require(sceFiosDHCloseSync(nullptr, dirHandle) == SCE_FIOS_OK);

    sceFiosOpCancel(fileHandle);
    Require(!sceFiosOpIsCancelled(fileHandle));
    sceFiosOpCancel(dirHandle);
    Require(!sceFiosOpIsCancelled(dirHandle));

    const SceFiosOp completedOp = sceFiosFileGetSize(nullptr, file.c_str());
    Require(completedOp > 0);
    Require(sceFiosOpIsDone(completedOp));
    Require(!sceFiosOpIsCancelled(completedOp));
    Require(sceFiosOpGetError(completedOp) == SCE_FIOS_OK);

    sceFiosOpCancel(completedOp);
    Require(!sceFiosOpIsCancelled(completedOp));
    Require(sceFiosOpGetError(completedOp) == SCE_FIOS_OK);

    sceFiosOpDelete(completedOp);
    Require(!sceFiosIsValidHandle(completedOp));

    sceFiosOpCancel(completedOp);
    Require(!sceFiosOpIsCancelled(completedOp));
    Require(sceFiosOpGetError(completedOp) == SCE_FIOS_ERROR_BAD_OP);

    sceFiosSuspend();
    Require(sceFiosIsSuspended());

    const SceFiosOp qOp1 = sceFiosFileGetSize(nullptr, file.c_str());
    const SceFiosOp qOp2 = sceFiosFileGetSize(nullptr, file.c_str());
    const SceFiosOp qOp3 = sceFiosFileGetSize(nullptr, file.c_str());

    Require(qOp1 > 0 && qOp2 > 0 && qOp3 > 0);
    Require(!sceFiosOpIsDone(qOp1) && !sceFiosOpIsDone(qOp2) && !sceFiosOpIsDone(qOp3));

    sceFiosOpCancel(0);
    sceFiosOpCancel(-1);
    sceFiosOpCancel(999999);

    Require(!sceFiosOpIsCancelled(qOp1));
    Require(!sceFiosOpIsCancelled(qOp2));
    Require(!sceFiosOpIsCancelled(qOp3));

    sceFiosOpCancel(qOp2);
    Require(sceFiosOpIsCancelled(qOp2));
    Require(!sceFiosOpIsCancelled(qOp1));
    Require(!sceFiosOpIsCancelled(qOp3));

    sceFiosOpCancel(qOp2);
    Require(sceFiosOpIsCancelled(qOp2));

    sceFiosResume();
    Require(!sceFiosIsSuspended());

    Require(sceFiosOpWait(qOp1) == SCE_FIOS_OK);
    Require(sceFiosOpWait(qOp2) == SCE_FIOS_ERROR_CANCELLED);
    Require(sceFiosOpWait(qOp3) == SCE_FIOS_OK);

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
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));

    CheckCacheBlockAlignment(directory.generic_string(), file);
    CheckBufferAllocationBounds(directory.generic_string(), file);
    CheckInvalidHandleRejectionOnCancel(directory.generic_string(), file);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));
    std::filesystem::remove_all(directory);
    std::puts("guest_fios2_cache_bounds ok");
    return 0;
}
