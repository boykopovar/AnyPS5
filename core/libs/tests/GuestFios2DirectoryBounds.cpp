#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestDirectoryInvalidPointersAndHandles(const std::string& directory, const std::string& file) {
    char dirBuf[512] = {};
    const SceFiosBuffer validBuf{ dirBuf, sizeof(dirBuf) };
    SceFiosDH dh = 0;

    Require(sceFiosDHOpenSync(nullptr, nullptr, directory.c_str(), validBuf) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDHOpenSync(nullptr, &dh, nullptr, validBuf) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDHOpenSync(nullptr, &dh, "", validBuf) == SCE_FIOS_ERROR_BAD_PATH);
    Require(dh == 0);

    const std::string longPath(1025, 'x');
    Require(sceFiosDHOpenSync(nullptr, &dh, longPath.c_str(), validBuf) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    Require(dh == 0);

    const std::string nonExistent = directory + "/non_existent_dir";
    Require(sceFiosDHOpenSync(nullptr, &dh, nonExistent.c_str(), validBuf) == SCE_FIOS_ERROR_BAD_PATH);
    Require(dh == 0);

    Require(sceFiosDHOpenSync(nullptr, &dh, file.c_str(), validBuf) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    Require(dh == 0);

    const SceFiosOp badPtrOp = sceFiosDHOpen(nullptr, nullptr, directory.c_str(), validBuf);
    Require(sceFiosOpWait(badPtrOp) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOpGetError(badPtrOp) == SCE_FIOS_ERROR_BAD_PTR);
    sceFiosOpDelete(badPtrOp);

    const SceFiosOp badPathOp = sceFiosDHOpen(nullptr, &dh, nullptr, validBuf);
    Require(sceFiosOpWait(badPathOp) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosOpGetError(badPathOp) == SCE_FIOS_ERROR_BAD_PTR);
    sceFiosOpDelete(badPathOp);

    const SceFiosOp longPathOp = sceFiosDHOpen(nullptr, &dh, longPath.c_str(), validBuf);
    Require(sceFiosOpWait(longPathOp) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    Require(sceFiosOpGetError(longPathOp) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    sceFiosOpDelete(longPathOp);

    const SceFiosOp notDirOp = sceFiosDHOpen(nullptr, &dh, file.c_str(), validBuf);
    Require(sceFiosOpWait(notDirOp) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    Require(sceFiosOpGetError(notDirOp) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    sceFiosOpDelete(notDirOp);

    const SceFiosDH invalidHandles[] = {
        0,
        -1,
        -100,
        static_cast<SceFiosDH>(0x80000000u),
        0x7ffffffe,
        0x7fffffff,
        999999
    };

    SceFiosDirEntry entry{};
    for (const SceFiosDH badDH : invalidHandles) {
        Require(sceFiosDHReadSync(nullptr, badDH, &entry) == SCE_FIOS_ERROR_BAD_DH);
        Require(sceFiosDHCloseSync(nullptr, badDH) == SCE_FIOS_ERROR_BAD_DH);
        Require(sceFiosDHGetPath(badDH) == nullptr);
        Require(!sceFiosIsValidHandle(badDH));

        const SceFiosOp readOp = sceFiosDHRead(nullptr, badDH, &entry);
        Require(sceFiosOpWait(readOp) == SCE_FIOS_ERROR_BAD_DH);
        Require(sceFiosOpGetError(readOp) == SCE_FIOS_ERROR_BAD_DH);
        sceFiosOpDelete(readOp);

        const SceFiosOp closeOp = sceFiosDHClose(nullptr, badDH);
        Require(sceFiosOpWait(closeOp) == SCE_FIOS_ERROR_BAD_DH);
        Require(sceFiosOpGetError(closeOp) == SCE_FIOS_ERROR_BAD_DH);
        sceFiosOpDelete(closeOp);
    }
}

void TestDirectoryBufferBounds(const std::string& directory) {
    const std::string subDir = directory + "/buffer_probe";
    Require(sceFiosDirectoryCreateSync(nullptr, subDir.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/item1.bin").c_str(), "data1", 5, 0) == 5);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/item2.bin").c_str(), "data2", 5, 0) == 5);
    Require(sceFiosDirectoryCreateSync(nullptr, (subDir + "/sub").c_str()) == SCE_FIOS_OK);

    SceFiosDH dh = 0;
    const SceFiosBuffer nullBuf{ nullptr, 0 };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), nullBuf) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(dh == 0);

    char temp[256] = {};
    const SceFiosBuffer zeroLenBuf{ temp, 0 };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), zeroLenBuf) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(dh == 0);

    const SceFiosBuffer smallBuf{ temp, 1 };
    const SceFiosOp probeOp = sceFiosDHOpen(nullptr, &dh, subDir.c_str(), smallBuf);
    Require(sceFiosOpWait(probeOp) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosOpGetError(probeOp) == SCE_FIOS_ERROR_BAD_SIZE);
    const std::int64_t requiredBytes = sceFiosOpGetActualCount(probeOp);
    Require(requiredBytes > 16);
    sceFiosOpDelete(probeOp);
    Require(dh == 0);

    std::vector<char> underallocated(static_cast<std::size_t>(requiredBytes - 1));
    const SceFiosBuffer underBuf{ underallocated.data(), underallocated.size() };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), underBuf) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(dh == 0);

    std::vector<char> exact(static_cast<std::size_t>(requiredBytes));
    const SceFiosBuffer exactBuf{ exact.data(), exact.size() };
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), exactBuf) == SCE_FIOS_OK);
    Require(dh > 0);
    Require(sceFiosIsValidHandle(dh));
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
}

void TestDirectoryEnumerationLifecycle(const std::string& directory) {
    const std::string subDir = directory + "/enum_dir";
    Require(sceFiosDirectoryCreateSync(nullptr, subDir.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/first.dat").c_str(), "hello", 5, 0) == 5);
    Require(sceFiosFileWriteSync(nullptr, (subDir + "/second.dat").c_str(), "world!!", 7, 0) == 7);
    Require(sceFiosDirectoryCreateSync(nullptr, (subDir + "/nested").c_str()) == SCE_FIOS_OK);

    char storage[512] = {};
    const SceFiosBuffer buffer{ storage, sizeof(storage) };
    SceFiosDH dh = 0;
    Require(sceFiosDHOpenSync(nullptr, &dh, subDir.c_str(), buffer) == SCE_FIOS_OK);
    Require(dh > 0);
    Require(sceFiosIsValidHandle(dh));

    const char* pathStr = sceFiosDHGetPath(dh);
    Require(pathStr != nullptr);
    Require(subDir == pathStr);

    Require(sceFiosDHReadSync(nullptr, dh, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    const SceFiosOp badReadOp = sceFiosDHRead(nullptr, dh, nullptr);
    Require(sceFiosOpWait(badReadOp) == SCE_FIOS_ERROR_BAD_PTR);
    sceFiosOpDelete(badReadOp);

    int entryCount = 0;
    bool foundFirst = false;
    bool foundSecond = false;
    bool foundNested = false;

    SceFiosDirEntry entry{};
    for (;;) {
        std::memset(&entry, 0, sizeof(entry));
        const int result = sceFiosDHReadSync(nullptr, dh, &entry);
        if (result == SCE_FIOS_ERROR_EOF) {
            break;
        }
        Require(result == SCE_FIOS_OK);
        ++entryCount;
        Require(entry.nameLength > 0);
        Require(entry.fullPathLength > entry.nameLength);
        Require(entry.offsetToName > 0);

        const std::string name(entry.fullPath + entry.offsetToName, entry.nameLength);

        if (name == "first.dat") {
            foundFirst = true;
            Require(entry.fileSize == 5);
            Require((entry.statFlags & SCE_FIOS_STAT_DIRECTORY) == 0);
        } else if (name == "second.dat") {
            foundSecond = true;
            Require(entry.fileSize == 7);
            Require((entry.statFlags & SCE_FIOS_STAT_DIRECTORY) == 0);
        } else if (name == "nested") {
            foundNested = true;
            Require((entry.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0);
        }
    }

    Require(entryCount == 3);
    Require(foundFirst && foundSecond && foundNested);

    Require(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_EOF);
    Require(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_EOF);

    const SceFiosOp eofReadOp = sceFiosDHRead(nullptr, dh, &entry);
    Require(sceFiosOpWait(eofReadOp) == SCE_FIOS_ERROR_EOF);
    sceFiosOpDelete(eofReadOp);

    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    Require(!sceFiosIsValidHandle(dh));
    Require(sceFiosDHGetPath(dh) == nullptr);
    Require(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_ERROR_BAD_DH);
    Require(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_BAD_DH);
}

void TestMultipleHandlesAndGetAllDHs(const std::string& directory) {
    const std::string d1 = directory + "/multi_1";
    const std::string d2 = directory + "/multi_2";
    const std::string d3 = directory + "/multi_3";
    Require(sceFiosDirectoryCreateSync(nullptr, d1.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDirectoryCreateSync(nullptr, d2.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDirectoryCreateSync(nullptr, d3.c_str()) == SCE_FIOS_OK);

    const std::uint32_t baselineDHs = sceFiosGetAllDHs(nullptr, 0);

    char buf1[256] = {};
    char buf2[256] = {};
    char buf3[256] = {};

    SceFiosDH dh1 = 0;
    SceFiosDH dh2 = 0;
    SceFiosDH dh3 = 0;

    Require(sceFiosDHOpenSync(nullptr, &dh1, d1.c_str(), SceFiosBuffer{ buf1, sizeof(buf1) }) == SCE_FIOS_OK);
    Require(sceFiosDHOpenSync(nullptr, &dh2, d2.c_str(), SceFiosBuffer{ buf2, sizeof(buf2) }) == SCE_FIOS_OK);
    Require(sceFiosDHOpenSync(nullptr, &dh3, d3.c_str(), SceFiosBuffer{ buf3, sizeof(buf3) }) == SCE_FIOS_OK);

    Require(dh1 > 0 && dh2 > 0 && dh3 > 0);
    Require(dh1 != dh2 && dh2 != dh3 && dh1 != dh3);

    const std::uint32_t currentCount = sceFiosGetAllDHs(nullptr, 0);
    Require(currentCount == baselineDHs + 3);

    std::array<SceFiosDH, 2> shortList{};
    const std::uint32_t listedShort = sceFiosGetAllDHs(shortList.data(), static_cast<std::uint32_t>(shortList.size()));
    Require(listedShort == currentCount);

    std::vector<SceFiosDH> fullList(currentCount + 4);
    const std::uint32_t listedFull = sceFiosGetAllDHs(fullList.data(), static_cast<std::uint32_t>(fullList.size()));
    Require(listedFull == currentCount);

    bool hasDH1 = false;
    bool hasDH2 = false;
    bool hasDH3 = false;
    for (std::uint32_t i = 0; i < currentCount; ++i) {
        if (fullList[i] == dh1) hasDH1 = true;
        if (fullList[i] == dh2) hasDH2 = true;
        if (fullList[i] == dh3) hasDH3 = true;
    }
    Require(hasDH1 && hasDH2 && hasDH3);

    Require(sceFiosDHCloseSync(nullptr, dh2) == SCE_FIOS_OK);
    Require(!sceFiosIsValidHandle(dh2));
    Require(sceFiosGetAllDHs(nullptr, 0) == baselineDHs + 2);

    Require(sceFiosDHCloseSync(nullptr, dh1) == SCE_FIOS_OK);
    Require(sceFiosDHCloseSync(nullptr, dh3) == SCE_FIOS_OK);
    Require(sceFiosGetAllDHs(nullptr, 0) == baselineDHs);
}

void TestDirectoryCreationDeletionBounds(const std::string& directory, const std::string& file) {
    Require(sceFiosDirectoryCreateSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDirectoryCreateSync(nullptr, "") == SCE_FIOS_ERROR_BAD_PATH);
    const std::string longPath(1025, 'y');
    Require(sceFiosDirectoryCreateSync(nullptr, longPath.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    const std::string newDir = directory + "/create_delete_test";
    Require(!sceFiosDirectoryExistsSync(nullptr, newDir.c_str()));
    Require(sceFiosDirectoryCreateWithModeSync(nullptr, newDir.c_str(), 0755) == SCE_FIOS_OK);
    Require(sceFiosDirectoryExistsSync(nullptr, newDir.c_str()));

    Require(!sceFiosDirectoryExistsSync(nullptr, file.c_str()));
    Require(!sceFiosDirectoryExistsSync(nullptr, nullptr));
    Require(!sceFiosDirectoryExistsSync(nullptr, ""));

    Require(sceFiosDirectoryDeleteSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosDirectoryDeleteSync(nullptr, "") == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosDirectoryDeleteSync(nullptr, longPath.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    Require(sceFiosDirectoryDeleteSync(nullptr, file.c_str()) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);

    Require(sceFiosDirectoryDeleteSync(nullptr, newDir.c_str()) == SCE_FIOS_OK);
    Require(!sceFiosDirectoryExistsSync(nullptr, newDir.c_str()));
    Require(sceFiosDirectoryDeleteSync(nullptr, newDir.c_str()) != SCE_FIOS_OK);
}

} // namespace

int main() {
    const std::filesystem::path tempDir = "fios2_directory_bounds_probe";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);
    const std::string testFile = (tempDir / "regular_file.txt").generic_string();
    {
        std::ofstream out(testFile, std::ios::binary);
        out << "regular file content";
    }

    std::uint32_t params[16] = {};
    params[0] = static_cast<std::uint32_t>(sizeof(params)) << 1;
    params[3] = 0x80000;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));

    TestDirectoryInvalidPointersAndHandles(tempDir.generic_string(), testFile);
    TestDirectoryBufferBounds(tempDir.generic_string());
    TestDirectoryEnumerationLifecycle(tempDir.generic_string());
    TestMultipleHandlesAndGetAllDHs(tempDir.generic_string());
    TestDirectoryCreationDeletionBounds(tempDir.generic_string(), testFile);

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));

    std::filesystem::remove_all(tempDir);
    return 0;
}
