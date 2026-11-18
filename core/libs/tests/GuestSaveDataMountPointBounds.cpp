#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr int SaveDataErrorMountFull = -2137063412;

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function function, std::string_view expectedName) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(expectedName) != std::string_view::npos);
        Require(std::string_view(error.what()).find("not implemented") != std::string_view::npos);
        return;
    }
    std::abort();
}

void TestPreInitializationBounds() {
    std::uint32_t count = 999;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataErrorNotInitialized);

    SaveDataMountPoint point{};
    std::memcpy(point.data, "/savedata0", sizeof("/savedata0"));
    Require(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotInitialized);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&point) == SaveDataErrorNotMounted);
}

void TestNullAndUnterminatedMountPointBounds() {
    SaveDataMountPoint unterminated{};
    std::memset(unterminated.data, 'y', sizeof(unterminated.data));

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&unterminated, &info) == SaveDataErrorParameter);

    SaveDataMountPoint validUnmounted{};
    std::memcpy(validUnmounted.data, "/savedata0", sizeof("/savedata0"));
    Require(sceSaveDataGetMountInfo(&validUnmounted, nullptr) == SaveDataErrorParameter);

    Require(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount2(0, &unterminated) == SaveDataErrorParameter);

    Require(sceSaveDataUmount(nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount(&unterminated) == SaveDataErrorParameter);

    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmountWithBackup(&unterminated) == SaveDataErrorParameter);

    std::array<char, 128> paramBuf{};
    std::size_t gotSize = 0;
    Require(sceSaveDataGetParam(nullptr, 1, paramBuf.data(), paramBuf.size(), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&unterminated, 1, paramBuf.data(), paramBuf.size(), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&validUnmounted, 1, nullptr, paramBuf.size(), &gotSize) == SaveDataErrorParameter);

    Require(sceSaveDataSetParam(nullptr, 1, paramBuf.data(), paramBuf.size()) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&unterminated, 1, paramBuf.data(), paramBuf.size()) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&validUnmounted, 1, nullptr, paramBuf.size()) == SaveDataErrorParameter);

    SaveDataIcon icon{};
    Require(sceSaveDataLoadIcon(nullptr, &icon) == SaveDataErrorParameter);
    Require(sceSaveDataLoadIcon(&unterminated, &icon) == SaveDataErrorParameter);
    Require(sceSaveDataLoadIcon(&validUnmounted, nullptr) == SaveDataErrorParameter);

    char rawIconBuf[64]{};
    SaveDataIcon nullBufIcon{};
    Require(sceSaveDataLoadIcon(&validUnmounted, &nullBufIcon) == SaveDataErrorParameter);

    Require(sceSaveDataSaveIcon(nullptr, &icon) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(&unterminated, &icon) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(&validUnmounted, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(&validUnmounted, &nullBufIcon) == SaveDataErrorParameter);

    Require(sceSaveDataSaveIconByPath(nullptr, "icon.png") == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&unterminated, "icon.png") == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&validUnmounted, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&validUnmounted, "") == SaveDataErrorParameter);

    Require(sceSaveDataGetMountedSaveDataCount(0, nullptr) == SaveDataErrorParameter);
    RequireThrowsUnimplemented([] {
        std::uint32_t mounted = 0;
        static_cast<void>(sceSaveDataGetMountedSaveDataCount(1, &mounted));
    }, "sceSaveDataGetMountedSaveDataCount");

    (void)rawIconBuf;
}

void TestUnmountedMountPointBounds() {
    SaveDataMountPoint nonExistent{};
    std::memcpy(nonExistent.data, "/savedata99", sizeof("/savedata99"));

    SaveDataMountPoint emptyPoint{};
    emptyPoint.data[0] = '\0';

    for (const auto* point : {&nonExistent, &emptyPoint}) {
        SaveDataMountInfo info{};
        Require(sceSaveDataGetMountInfo(point, &info) == SaveDataErrorNotMounted);
        Require(sceSaveDataUmount2(0, point) == SaveDataErrorNotMounted);
        Require(sceSaveDataUmount(point) == SaveDataErrorNotMounted);
        Require(sceSaveDataUmountWithBackup(point) == SaveDataErrorNotMounted);

        std::array<char, 128> paramBuf{};
        std::size_t gotSize = 0;
        Require(sceSaveDataGetParam(point, 1, paramBuf.data(), paramBuf.size(), &gotSize) == SaveDataErrorNotMounted);
        Require(sceSaveDataSetParam(point, 1, paramBuf.data(), paramBuf.size()) == SaveDataErrorNotMounted);

        char rawIconBuf[64]{};
        SaveDataIcon validIcon{rawIconBuf, sizeof(rawIconBuf), 0, {}};
        Require(sceSaveDataLoadIcon(point, &validIcon) == SaveDataErrorNotMounted);
        Require(sceSaveDataSaveIcon(point, &validIcon) == SaveDataErrorNotMounted);
        Require(sceSaveDataSaveIconByPath(point, "dummy.png") == SaveDataErrorNotMounted);
    }
}

void TestActiveMountLifecycleAndOperations() {
    std::uint32_t count = 999;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "active_slot_test", sizeof("active_slot_test"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);
    Require(mountResult.mount_status == 1u);

    const std::string_view mountPointStr(mountResult.mount_point.data);
    Require(mountPointStr.find("/savedata") == 0);
    Require(std::strlen(mountResult.mount_point.data) < sizeof(mountResult.mount_point.data));

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 1);

    SaveDataMountInfo mountInfo{};
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &mountInfo) == SaveDataOk);
    Require(mountInfo.blocks >= 48);

    Require(sceSaveDataPrepare(&mountResult.mount_point, nullptr) == SaveDataOk);
    RequireThrowsUnimplemented([&] {
        static_cast<void>(sceSaveDataPrepareForTransferring(&mountResult.mount_point, nullptr));
    }, "sceSaveDataPrepareForTransferring");

    std::array<char, 128> titleBuf{};
    std::memcpy(titleBuf.data(), "MountPointTitle", sizeof("MountPointTitle"));
    Require(sceSaveDataSetParam(&mountResult.mount_point, 1, titleBuf.data(), titleBuf.size()) == SaveDataOk);

    std::array<char, 128> readTitle{};
    std::size_t gotSize = 0;
    Require(sceSaveDataGetParam(&mountResult.mount_point, 1, readTitle.data(), readTitle.size(), &gotSize) == SaveDataOk);
    Require(gotSize == 128);
    Require(std::strcmp(readTitle.data(), "MountPointTitle") == 0);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&mountResult.mount_point) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &mountInfo) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetParam(&mountResult.mount_point, 1, readTitle.data(), readTitle.size(), &gotSize) == SaveDataErrorNotMounted);
    Require(sceSaveDataSetParam(&mountResult.mount_point, 1, titleBuf.data(), titleBuf.size()) == SaveDataErrorNotMounted);
}

void TestSlotCapacityExhaustionAndRecycling() {
    constexpr std::size_t MaxSlots = 16;
    std::array<SceSaveDataDirName, MaxSlots> dirNames{};
    std::array<SaveDataMount3, MaxSlots> mounts{};
    std::array<SaveDataMountResult, MaxSlots> results{};

    for (std::size_t i = 0; i < MaxSlots; ++i) {
        std::snprintf(dirNames[i].data, sizeof(dirNames[i].data), "capacity_dir_%zu", i);
        mounts[i].user_id = 1;
        mounts[i].dir_name = &dirNames[i];
        mounts[i].mount_mode = MountModeRdWr | MountModeCreate2;

        Require(sceSaveDataMount3(&mounts[i], &results[i]) == SaveDataOk);
        const std::string expectedPoint = "/savedata" + std::to_string(i);
        Require(std::string_view(results[i].mount_point.data) == expectedPoint);
    }

    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == MaxSlots);

    SceSaveDataDirName overflowDir{};
    std::memcpy(overflowDir.data, "overflow_dir", sizeof("overflow_dir"));
    SaveDataMount3 overflowMount{};
    overflowMount.user_id = 1;
    overflowMount.dir_name = &overflowDir;
    overflowMount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult overflowResult{};
    Require(sceSaveDataMount3(&overflowMount, &overflowResult) == SaveDataErrorMountFull);

    Require(sceSaveDataUmount(&results[7].mount_point) == SaveDataOk);
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == MaxSlots - 1);

    Require(sceSaveDataMount3(&overflowMount, &overflowResult) == SaveDataOk);
    Require(std::string_view(overflowResult.mount_point.data) == results[7].mount_point.data);
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == MaxSlots);

    Require(sceSaveDataUmount2(0, &overflowResult.mount_point) == SaveDataOk);
    for (std::size_t i = 0; i < MaxSlots; ++i) {
        if (i == 7) continue;
        Require(sceSaveDataUmount2(0, &results[i].mount_point) == SaveDataOk);
    }

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);
}

void TestUmountWithBackupLifecycle() {
    SceSaveDataDirName backupDir{};
    std::memcpy(backupDir.data, "backup_point_dir", sizeof("backup_point_dir"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &backupDir;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    std::uint32_t count = 0;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 1);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataOk);
    Require(count == 0);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataErrorNotMounted);
}

void TestPostTerminationBounds() {
    std::uint32_t count = 999;
    Require(sceSaveDataGetMountedSaveDataCount(0, &count) == SaveDataErrorNotInitialized);

    SaveDataMountPoint point{};
    std::memcpy(point.data, "/savedata0", sizeof("/savedata0"));
    Require(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotInitialized);
    Require(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&point) == SaveDataErrorNotMounted);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-mountpoint-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestPreInitializationBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestNullAndUnterminatedMountPointBounds();
    TestUnmountedMountPointBounds();
    TestActiveMountLifecycleAndOperations();
    TestSlotCapacityExhaustionAndRecycling();
    TestUmountWithBackupLifecycle();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    TestPostTerminationBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
