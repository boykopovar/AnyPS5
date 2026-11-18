#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;
constexpr std::uint32_t SaveDataParamTypeTitle = 1;
constexpr std::uint32_t SaveDataEventTypeUmountBackup = 1;
constexpr std::uint32_t SaveDataEventTypeBackup = 2;

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
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorNotInitialized);
    SaveDataCheckBackupData check{};
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorNotInitialized);
    SaveDataRestoreBackupData restore{};
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorNotInitialized);
    SaveDataMountPoint point{};
    Require(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotInitialized);

    Require(sceSaveDataBackup(nullptr) == SaveDataErrorParameter);
    SaveDataBackup backup{};
    Require(sceSaveDataBackup(&backup) == SaveDataErrorParameter);
}

void TestBackupParameterBounds() {
    Require(sceSaveDataBackup(nullptr) == SaveDataErrorParameter);

    SaveDataBackup backup{};
    backup.user_id = 1;

    backup.dir_name = nullptr;
    Require(sceSaveDataBackup(&backup) == SaveDataErrorParameter);

    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'b', sizeof(unterminated.data));
    backup.dir_name = &unterminated;
    Require(sceSaveDataBackup(&backup) == SaveDataErrorParameter);

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "bk_param_dir", sizeof("bk_param_dir"));
    backup.dir_name = &validDir;
    backup.title_id = nullptr;
    Require(sceSaveDataBackup(&backup) == SaveDataOk);

    SaveDataEvent event{};
    Require(sceSaveDataGetEventResult(nullptr, &event) == SaveDataOk);
    Require(event.type == SaveDataEventTypeBackup);
    Require(event.error_code == SaveDataOk);
    Require(event.user_id == 1);
    Require(std::strcmp(event.dir_name.data, "bk_param_dir") == 0);

    SceSaveDataTitleId titleId{};
    std::memcpy(titleId.data, "CUSA00001", sizeof("CUSA00001"));
    backup.title_id = &titleId;
    Require(sceSaveDataBackup(&backup) == SaveDataOk);

    Require(sceSaveDataGetEventResult(nullptr, &event) == SaveDataOk);
    Require(event.type == SaveDataEventTypeBackup);
    Require(event.error_code == SaveDataOk);
    Require(std::strcmp(event.title_id.data, "CUSA00001") == 0);
}

void TestCheckBackupParameterAndPathBounds() {
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataCheckBackupData check{};
    check.user_id = 1;

    check.dir_name = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'c', sizeof(unterminated.data));
    check.dir_name = &unterminated;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    check.dir_name = &emptyDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    for (const char* invalid : {".", "..", "../escape", "..\\escape", "sub/dir", "sub\\dir", "path:colon", "/rooted", "\\rooted", "a/b/c"}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        check.dir_name = &invalidDir;
        Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
    }

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "bk_check_save", sizeof("bk_check_save"));
    check.dir_name = &validDir;

    SceSaveDataTitleId foreignTitle{};
    std::memcpy(foreignTitle.data, "CUSA99999", sizeof("CUSA99999"));
    check.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    check.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
}

void TestRestoreBackupParameterAndPathBounds() {
    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataRestoreBackupData restore{};
    restore.user_id = 1;

    restore.dir_name = nullptr;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'r', sizeof(unterminated.data));
    restore.dir_name = &unterminated;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    restore.dir_name = &emptyDir;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    for (const char* invalid : {".", "..", "../escape", "..\\escape", "sub/dir", "sub\\dir", "path:colon", "/rooted", "\\rooted", "a/b/c"}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        restore.dir_name = &invalidDir;
        Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);
    }

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "bk_restore_save", sizeof("bk_restore_save"));
    restore.dir_name = &validDir;

    SceSaveDataTitleId foreignTitle{};
    std::memcpy(foreignTitle.data, "CUSA99999", sizeof("CUSA99999"));
    restore.title_id = &foreignTitle;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    restore.title_id = nullptr;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);
}

void TestUmountWithBackupParameterBounds() {
    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "/savedata0", sizeof("/savedata0"));
    Require(sceSaveDataUmountWithBackup(&unmountedPoint) == SaveDataErrorNotMounted);

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'u', sizeof(unterminatedPoint.data));
    Require(sceSaveDataUmountWithBackup(&unterminatedPoint) == SaveDataErrorParameter);

    SaveDataMountPoint invalidPoint{};
    std::memcpy(invalidPoint.data, "/invalid/mount", sizeof("/invalid/mount"));
    Require(sceSaveDataUmountWithBackup(&invalidPoint) == SaveDataErrorNotMounted);
}

void TestActiveMountBusyAndBackupLifecycle() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "bk_active_cycle", sizeof("bk_active_cycle"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);
    Require(mountResult.mount_status == 1u);

    SaveDataCheckBackupData check{};
    check.user_id = 1;
    check.dir_name = &dirName;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);

    SaveDataRestoreBackupData restore{};
    restore.user_id = 1;
    restore.dir_name = &dirName;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorBusy);

    const char titleText[] = "BackupLifecycleTitle";
    Require(sceSaveDataSetParam(&mountResult.mount_point, SaveDataParamTypeTitle, titleText, sizeof(titleText)) == SaveDataOk);

    const std::filesystem::path saveFilePath = std::string(mountResult.mount_point.data) + "/data.bin";
    {
        std::ofstream out(saveFilePath);
        out << "saved-payload-content";
    }
    Require(std::filesystem::exists(saveFilePath));

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataOk);

    SaveDataEvent event{};
    Require(sceSaveDataGetEventResult(nullptr, &event) == SaveDataOk);
    Require(event.type == SaveDataEventTypeUmountBackup);
    Require(event.error_code == SaveDataOk);
    Require(event.user_id == 1);
    Require(std::strcmp(event.dir_name.data, "bk_active_cycle") == 0);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataErrorNotMounted);

    SaveDataParam paramOut{};
    check.param = &paramOut;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);
    Require(std::strcmp(paramOut.title, titleText) == 0);

    const std::filesystem::path realSavePath = std::filesystem::path("_sd/bk_active_cycle") / "data.bin";
    {
        std::ofstream out(realSavePath);
        out << "corrupted-payload";
    }
    {
        std::ifstream in(realSavePath);
        std::string content;
        in >> content;
        Require(content == "corrupted-payload");
    }

    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataOk);

    {
        std::ifstream in(realSavePath);
        std::string content;
        in >> content;
        Require(content == "saved-payload-content");
    }

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &dirName;
    Require(sceSaveDataDelete(&del) == SaveDataOk);

    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
}

void TestUnimplementedBackupVariants() {
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataCheckBackupDataForCdlg()); }, "sceSaveDataCheckBackupDataForCdlg");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataCheckBackupDataInternal()); }, "sceSaveDataCheckBackupDataInternal");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataCheckBackupDataLight()); }, "sceSaveDataCheckBackupDataLight");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataRestoreBackupDataForCdlg()); }, "sceSaveDataRestoreBackupDataForCdlg");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteBackupData()); }, "sceSaveDataDeleteBackupData");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataBindPsnAccountForSystemBackup()); }, "sceSaveDataBindPsnAccountForSystemBackup");
}

void TestPostTerminationBounds() {
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorNotInitialized);
    SaveDataCheckBackupData check{};
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorNotInitialized);
    SaveDataRestoreBackupData restore{};
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorNotInitialized);
    SaveDataMountPoint point{};
    Require(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotInitialized);

    Require(sceSaveDataBackup(nullptr) == SaveDataErrorParameter);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-backup-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestPreInitializationBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestBackupParameterBounds();
    TestCheckBackupParameterAndPathBounds();
    TestRestoreBackupParameterAndPathBounds();
    TestUmountWithBackupParameterBounds();
    TestActiveMountBusyAndBackupLifecycle();
    TestUnimplementedBackupVariants();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    TestPostTerminationBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
