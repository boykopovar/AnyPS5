#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <source_location>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotMounted = -2137063420;

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void WritePayload(const std::filesystem::path& path, std::size_t bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    const std::string payload(bytes, 's');
    stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

void TestUninitializedState() {
    std::uint64_t sizeVal = 0xDEADBEEF;
    Require(sceSaveDataGetAllSize(0, &sizeVal) == SaveDataErrorNotInitialized);
    Require(sizeVal == 0xDEADBEEF);
    Require(sceSaveDataGetAllSize(0, nullptr) == SaveDataErrorNotInitialized);

    std::uint32_t countVal = 0xCAFEBABE;
    Require(sceSaveDataGetSaveDataCount(0, &countVal) == SaveDataErrorNotInitialized);
    Require(countVal == 0xCAFEBABE);
    Require(sceSaveDataGetSaveDataCount(0, nullptr) == SaveDataErrorNotInitialized);

    Require(sceSaveDataIsMounted() == SaveDataErrorNotInitialized);

    std::uint32_t mountedCount = 0x12345678;
    Require(sceSaveDataGetMountedSaveDataCount(0, &mountedCount) == SaveDataErrorNotInitialized);
    Require(mountedCount == 0x12345678);
    Require(sceSaveDataGetMountedSaveDataCount(0, nullptr) == SaveDataErrorNotInitialized);

    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "slot_uninit", sizeof("slot_uninit"));
    SaveDataMountInfo mountInfo{};
    Require(sceSaveDataGetMountInfo(&unmountedPoint, &mountInfo) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &unmountedPoint) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmountWithBackup(&unmountedPoint) == SaveDataErrorNotInitialized);
}

void TestParameterBoundsAndNullChecks() {
    Require(sceSaveDataGetAllSize(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetAllSize(0, reinterpret_cast<std::uint64_t*>(0x10)) == SaveDataErrorParameter);

    Require(sceSaveDataGetSaveDataCount(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataCount(0, reinterpret_cast<std::uint32_t*>(0x10)) == SaveDataErrorParameter);

    Require(sceSaveDataGetMountedSaveDataCount(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountedSaveDataCount(0, reinterpret_cast<std::uint32_t*>(0x10)) == SaveDataErrorParameter);

    std::uint64_t sizeOut = 999;
    std::uint32_t countOut = 999;
    for (const std::int32_t reservedArg : {0, 1, -1, 0x10000000, 0x7FFFFFFF}) {
        Require(sceSaveDataGetAllSize(reservedArg, &sizeOut) == SaveDataOk);
        Require(sizeOut == 0);
        Require(sceSaveDataGetSaveDataCount(reservedArg, &countOut) == SaveDataOk);
        Require(countOut == 0);
    }

    std::uint32_t mountedOut = 999;
    Require(sceSaveDataGetMountedSaveDataCount(0, &mountedOut) == SaveDataOk);
    Require(mountedOut == 0);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);

    SaveDataMountPoint validUnmounted{};
    std::memcpy(validUnmounted.data, "slot_absent", sizeof("slot_absent"));
    Require(sceSaveDataGetMountInfo(&validUnmounted, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&validUnmounted, &info) == SaveDataErrorNotMounted);

    SaveDataMountPoint unterminated{};
    std::memset(unterminated.data, 'k', sizeof(unterminated.data));
    Require(sceSaveDataGetMountInfo(&unterminated, &info) == SaveDataErrorParameter);
    Require(sceSaveDataUmount2(0, &unterminated) == SaveDataErrorParameter);
    Require(sceSaveDataUmountWithBackup(&unterminated) == SaveDataErrorParameter);

    Require(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount2(0, &validUnmounted) == SaveDataErrorNotMounted);

    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmountWithBackup(&validUnmounted) == SaveDataErrorNotMounted);
}

void TestDirectorySizeAndCountCalculations() {
    const std::filesystem::path rootPath = "_sd";
    WritePayload(rootPath / "plain_ignored_file.bin", 4096);

    std::uint32_t countOut = 999;
    std::uint64_t sizeOut = 999;
    Require(sceSaveDataGetSaveDataCount(0, &countOut) == SaveDataOk);
    Require(countOut == 0);
    Require(sceSaveDataGetAllSize(0, &sizeOut) == SaveDataOk);
    Require(sizeOut == 0);

    WritePayload(rootPath / "SAVE_DIR_A" / "block1.dat", 1024);
    Require(sceSaveDataGetSaveDataCount(0, &countOut) == SaveDataOk);
    Require(countOut == 1);
    Require(sceSaveDataGetAllSize(0, &sizeOut) == SaveDataOk);
    Require(sizeOut == 1);

    WritePayload(rootPath / "SAVE_DIR_A" / "block2.dat", 1);
    Require(sceSaveDataGetSaveDataCount(0, &countOut) == SaveDataOk);
    Require(countOut == 1);
    Require(sceSaveDataGetAllSize(0, &sizeOut) == SaveDataOk);
    Require(sizeOut == 2);

    WritePayload(rootPath / "SAVE_DIR_B" / "nested" / "payload.dat", 2048);
    Require(sceSaveDataGetSaveDataCount(0, &countOut) == SaveDataOk);
    Require(countOut == 2);
    Require(sceSaveDataGetAllSize(0, &sizeOut) == SaveDataOk);
    Require(sizeOut == 4);

    WritePayload(rootPath / "SAVE_DIR_B" / "leaf.dat", 1024);
    Require(sceSaveDataGetSaveDataCount(0, &countOut) == SaveDataOk);
    Require(countOut == 2);
    Require(sceSaveDataGetAllSize(0, &sizeOut) == SaveDataOk);
    Require(sizeOut == 5);
}

void TestMountLifecycleAndCountPersistence() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "SAVE_DIR_M", sizeof("SAVE_DIR_M"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    std::uint32_t mountedCount = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &mountedCount) == SaveDataOk);
    Require(mountedCount == 1);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &info) == SaveDataOk);

    std::uint32_t totalCount = 0;
    Require(sceSaveDataGetSaveDataCount(0, &totalCount) == SaveDataOk);
    Require(totalCount == 3);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountedSaveDataCount(0, &mountedCount) == SaveDataOk);
    Require(mountedCount == 0);

    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataErrorNotMounted);

    Require(sceSaveDataGetSaveDataCount(0, &totalCount) == SaveDataOk);
    Require(totalCount == 3);

    SaveDataDelete delParam{};
    delParam.user_id = 1;
    delParam.dir_name = &dirName;
    Require(sceSaveDataDelete(&delParam) == SaveDataOk);

    Require(sceSaveDataGetSaveDataCount(0, &totalCount) == SaveDataOk);
    Require(totalCount == 2);
}

void TestTerminationState() {
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::uint64_t sizeVal = 0xAA55AA55;
    Require(sceSaveDataGetAllSize(0, &sizeVal) == SaveDataErrorNotInitialized);
    Require(sizeVal == 0xAA55AA55);

    std::uint32_t countVal = 0x55AA55AA;
    Require(sceSaveDataGetSaveDataCount(0, &countVal) == SaveDataErrorNotInitialized);
    Require(countVal == 0x55AA55AA);

    Require(sceSaveDataIsMounted() == SaveDataErrorNotInitialized);

    std::uint32_t mountedCount = 0x33221100;
    Require(sceSaveDataGetMountedSaveDataCount(0, &mountedCount) == SaveDataErrorNotInitialized);
    Require(mountedCount == 0x33221100);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-allsize-count-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestUninitializedState();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestParameterBoundsAndNullChecks();
    TestDirectorySizeAndCountCalculations();
    TestMountLifecycleAndCountPersistence();
    TestTerminationState();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
