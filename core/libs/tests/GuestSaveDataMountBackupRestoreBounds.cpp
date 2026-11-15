#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedBounds() {
    SaveDataMountPoint point{};
    std::memcpy(point.data, "savedata0", sizeof("savedata0"));
    Require(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotInitialized);

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "test_dir", sizeof("test_dir"));
    SaveDataCheckBackupData check{};
    check.dir_name = &dirName;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    SaveDataRestoreBackupData restore{};
    restore.dir_name = &dirName;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);
}

void TestParameterBounds() {
    Require(sceSaveDataInitialize3(nullptr) == 0);

    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "unmounted", sizeof("unmounted"));
    Require(sceSaveDataUmountWithBackup(&unmountedPoint) == SaveDataErrorNotMounted);

    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataCheckBackupData nullDirCheck{};
    Require(sceSaveDataCheckBackupData(&nullDirCheck) == SaveDataErrorParameter);

    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataRestoreBackupData nullDirRestore{};
    Require(sceSaveDataRestoreBackupData(&nullDirRestore) == SaveDataErrorParameter);

    for (const char* invalid : {"../escape", "..\\escape", ".", "..", "bad/dir", "bad\\dir", "bad:name", ""}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        SaveDataCheckBackupData invalidCheck{};
        invalidCheck.dir_name = &invalidDir;
        Require(sceSaveDataCheckBackupData(&invalidCheck) == SaveDataErrorParameter);

        SaveDataRestoreBackupData invalidRestore{};
        invalidRestore.dir_name = &invalidDir;
        Require(sceSaveDataRestoreBackupData(&invalidRestore) == SaveDataErrorParameter);
    }

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'x', sizeof(unterminatedDir.data));
    SaveDataCheckBackupData unterminatedCheck{};
    unterminatedCheck.dir_name = &unterminatedDir;
    Require(sceSaveDataCheckBackupData(&unterminatedCheck) == SaveDataErrorParameter);

    SaveDataRestoreBackupData unterminatedRestore{};
    unterminatedRestore.dir_name = &unterminatedDir;
    Require(sceSaveDataRestoreBackupData(&unterminatedRestore) == SaveDataErrorParameter);
}

void TestNonExistentBackup() {
    SceSaveDataDirName nonExistentDir{};
    std::memcpy(nonExistentDir.data, "missing_backup", sizeof("missing_backup"));

    SaveDataCheckBackupData missingCheck{};
    missingCheck.dir_name = &nonExistentDir;
    Require(sceSaveDataCheckBackupData(&missingCheck) == SaveDataErrorNotFound);

    SaveDataRestoreBackupData missingRestore{};
    missingRestore.dir_name = &nonExistentDir;
    Require(sceSaveDataRestoreBackupData(&missingRestore) == SaveDataErrorNotFound);
}

void TestMountUmountBackupAndRestoreLifecycle() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "backup_save", sizeof("backup_save"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == 0);
    Require(mountResult.mount_status == 1u);

    SaveDataCheckBackupData busyCheck{};
    busyCheck.dir_name = &dirName;
    Require(sceSaveDataCheckBackupData(&busyCheck) == SaveDataErrorBusy);

    SaveDataRestoreBackupData busyRestore{};
    busyRestore.dir_name = &dirName;
    Require(sceSaveDataRestoreBackupData(&busyRestore) == SaveDataErrorBusy);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == 0);

    SaveDataParam param{};
    busyCheck.param = &param;
    Require(sceSaveDataCheckBackupData(&busyCheck) == 0);

    Require(sceSaveDataRestoreBackupData(&busyRestore) == 0);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount-backup-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    std::filesystem::create_directories("sce_sys");
    std::ofstream stream("sce_sys/param.json", std::ios::binary);
    stream << "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Recovery fixture\"}}}";
    stream.close();
    AddPathAlias_nid_no_patch("/app0", std::filesystem::current_path().string().c_str());

    TestUninitializedBounds();
    TestParameterBounds();
    TestNonExistentBackup();
    TestMountUmountBackupAndRestoreLifecycle();

    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
