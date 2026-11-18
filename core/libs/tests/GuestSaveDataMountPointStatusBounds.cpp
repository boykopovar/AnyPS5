#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;

constexpr std::uint64_t SaveDataBlocksMin2 = 96;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedState() {
    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataErrorNotInitialized);
    Require(sceSaveDataIsMounted() == SaveDataErrorNotInitialized);

    SaveDataMountPoint point{};
    std::memcpy(point.data, "uninit", sizeof("uninit"));
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&point) == SaveDataErrorNotMounted);
}

void TestNullAndParameterBounds() {
    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, nullptr) == SaveDataErrorParameter);

    bool reservedThrows = false;
    try {
        static_cast<void>(sceSaveDataGetMountedSaveDataCount(1, &count));
    } catch (const std::runtime_error&) {
        reservedThrows = true;
    }
    Require(reservedThrows);

    bool isMountedThrows = false;
    try {
        static_cast<void>(sceSaveDataIsMounted());
    } catch (const std::runtime_error&) {
        isMountedThrows = true;
    }
    Require(isMountedThrows);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);

    SaveDataMountPoint validUnmounted{};
    std::memcpy(validUnmounted.data, "slot_none", sizeof("slot_none"));
    Require(sceSaveDataGetMountInfo(&validUnmounted, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&validUnmounted, &info) == SaveDataErrorNotMounted);

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'x', sizeof(unterminatedPoint.data));
    Require(sceSaveDataGetMountInfo(&unterminatedPoint, &info) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&unterminatedPoint, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount2(0, &unterminatedPoint) == SaveDataErrorParameter);
    Require(sceSaveDataUmount(&unterminatedPoint) == SaveDataErrorParameter);

    for (const char* invalid : {"../escape", "", ".", "..", "..\\escape", "bad/point", "bad:name"}) {
        SaveDataMountPoint invalidPoint{};
        std::snprintf(invalidPoint.data, sizeof(invalidPoint.data), "%s", invalid);
        Require(sceSaveDataGetMountInfo(&invalidPoint, &info) == SaveDataErrorParameter);
        Require(sceSaveDataUmount2(0, &invalidPoint) == SaveDataErrorParameter);
        Require(sceSaveDataUmount(&invalidPoint) == SaveDataErrorParameter);
    }

    Require(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount(nullptr) == SaveDataErrorParameter);
}

void TestMountStatusAndLifecycle() {
    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "mount_status_1", sizeof("mount_status_1"));
    SaveDataMount2 mountReq{};
    mountReq.user_id = 0x10000000;
    mountReq.dir_name = &dirName;
    mountReq.mount_mode = MountReadWrite | MountCreate;
    mountReq.blocks = SaveDataBlocksMin2;

    SaveDataMountResult result{};
    Require(sceSaveDataMount2(&mountReq, &result) == SaveDataOk);
    Require(result.mount_status == 1u);
    Require(std::strncmp(result.mount_point.data, "/savedata", 9) == 0);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 1);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&result.mount_point, &info) == SaveDataOk);
    Require(info.blocks == SaveDataBlocksMin2);
    Require(info.free_blocks <= info.blocks);

    SaveDataMountResult duplicateResult{};
    Require(sceSaveDataMount2(&mountReq, &duplicateResult) == SaveDataErrorBusy);

    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);
    Require(sceSaveDataGetMountInfo(&result.mount_point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&result.mount_point) == SaveDataErrorNotMounted);

    mountReq.mount_mode = MountReadWrite | MountCreate2;
    Require(sceSaveDataMount2(&mountReq, &result) == SaveDataOk);
    Require(result.mount_status == 0u);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 1);

    Require(sceSaveDataUmount(&result.mount_point) == SaveDataOk);
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);

    mountReq.mount_mode = MountReadWrite;
    Require(sceSaveDataMount2(&mountReq, &result) == SaveDataOk);
    Require(result.mount_status == 0u);

    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataOk);
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);
}

void TestMultiMountPointStatusTracking() {
    SceSaveDataDirName dirA{};
    std::memcpy(dirA.data, "multi_status_a", sizeof("multi_status_a"));
    SaveDataMount2 mountA{};
    mountA.user_id = 0x10000000;
    mountA.dir_name = &dirA;
    mountA.mount_mode = MountReadWrite | MountCreate;
    mountA.blocks = SaveDataBlocksMin2;

    SceSaveDataDirName dirB{};
    std::memcpy(dirB.data, "multi_status_b", sizeof("multi_status_b"));
    SaveDataMount2 mountB{};
    mountB.user_id = 0x10000000;
    mountB.dir_name = &dirB;
    mountB.mount_mode = MountReadWrite | MountCreate;
    mountB.blocks = SaveDataBlocksMin2;

    SceSaveDataDirName dirC{};
    std::memcpy(dirC.data, "multi_status_c", sizeof("multi_status_c"));
    SaveDataMount2 mountC{};
    mountC.user_id = 0x10000000;
    mountC.dir_name = &dirC;
    mountC.mount_mode = MountReadWrite | MountCreate;
    mountC.blocks = SaveDataBlocksMin2;

    SaveDataMountResult resultA{};
    SaveDataMountResult resultB{};
    SaveDataMountResult resultC{};

    Require(sceSaveDataMount2(&mountA, &resultA) == SaveDataOk);
    Require(resultA.mount_status == 1u);

    Require(sceSaveDataMount2(&mountB, &resultB) == SaveDataOk);
    Require(resultB.mount_status == 1u);

    Require(sceSaveDataMount2(&mountC, &resultC) == SaveDataOk);
    Require(resultC.mount_status == 1u);

    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 3);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&resultA.mount_point, &info) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&resultB.mount_point, &info) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&resultC.mount_point, &info) == SaveDataOk);

    Require(sceSaveDataUmount2(0, &resultB.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 2);

    Require(sceSaveDataGetMountInfo(&resultB.mount_point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetMountInfo(&resultA.mount_point, &info) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&resultC.mount_point, &info) == SaveDataOk);

    Require(sceSaveDataUmount2(0, &resultA.mount_point) == SaveDataOk);
    Require(sceSaveDataUmount2(0, &resultC.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);
}

void TestPostTerminationState() {
    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataErrorNotInitialized);
    Require(sceSaveDataIsMounted() == SaveDataErrorNotInitialized);

    SaveDataMountPoint point{};
    std::memcpy(point.data, "postterm", sizeof("postterm"));
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-mountpoint-status-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestUninitializedState();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestNullAndParameterBounds();
    TestMountStatusAndLifecycle();
    TestMultiMountPointStatusTracking();
    Require(sceSaveDataTerminate() == SaveDataOk);
    TestPostTerminationState();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
