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
constexpr int SaveDataErrorNotFound = -2137063416;

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
    Require(sceSaveDataDeleteTransactionResource(0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataDeleteTransactionResource(1) == SaveDataErrorNotInitialized);
    Require(sceSaveDataDelete(nullptr) == SaveDataErrorParameter);

    SaveDataDelete del{};
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);
}

void TestParameterValidationAndPathTraversal() {
    SaveDataDelete del{};
    del.user_id = 1;

    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'x', sizeof(unterminated.data));
    del.dir_name = &unterminated;
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    del.dir_name = &emptyDir;
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);

    for (const char* invalid : {".", "..", "../escape", "..\\escape", "sub/dir", "sub\\dir", "path:colon", "/rooted", "\\rooted", "a/b/c"}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        del.dir_name = &invalidDir;
        Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);
    }
}

void TestActiveMountBusyAndLifecycle() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "del_active_save", sizeof("del_active_save"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);
    Require(mountResult.mount_status == 1u);

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &dirName;
    Require(sceSaveDataDelete(&del) == SaveDataErrorBusy);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataDelete(&del) == SaveDataOk);

    SaveDataMount3 absentMount{};
    absentMount.user_id = 1;
    absentMount.dir_name = &dirName;
    absentMount.mount_mode = MountModeRdWr;
    SaveDataMountResult absentResult{};
    Require(sceSaveDataMount3(&absentMount, &absentResult) == SaveDataErrorNotFound);

    Require(sceSaveDataDelete(&del) == SaveDataOk);
}

void TestFilesystemTreeCleanupAndIsolation() {
    const std::filesystem::path saveDir = "_sd/clean_save";
    const std::filesystem::path saveParam = "_sd/clean_save.param";
    const std::filesystem::path saveBlocks = "_sd/clean_save.blocks";
    const std::filesystem::path backupDir = "_sd_backup/clean_save";
    const std::filesystem::path victimFile = "victim_outside.txt";

    std::filesystem::create_directories(saveDir);
    std::filesystem::create_directories(backupDir);

    {
        std::ofstream(saveDir / "save_data.bin") << "save-data";
        std::ofstream(saveParam) << "param-data";
        std::ofstream(saveBlocks) << "blocks-data";
        std::ofstream(backupDir / "save_backup.bin") << "backup-data";
        std::ofstream(victimFile) << "keep-me";
    }

    Require(std::filesystem::exists(saveDir));
    Require(std::filesystem::exists(saveParam));
    Require(std::filesystem::exists(saveBlocks));
    Require(std::filesystem::exists(backupDir));
    Require(std::filesystem::exists(victimFile));

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "clean_save", sizeof("clean_save"));

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &dirName;

    Require(sceSaveDataDelete(&del) == SaveDataOk);

    Require(!std::filesystem::exists(saveDir));
    Require(!std::filesystem::exists(saveParam));
    Require(!std::filesystem::exists(saveBlocks));
    Require(!std::filesystem::exists(backupDir));
    Require(std::filesystem::exists(victimFile));

    std::error_code error;
    std::filesystem::remove(victimFile, error);
}

void TestTransactionResourceLifecycle() {
    Require(sceSaveDataDeleteTransactionResource(0) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(1) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(100) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(-1) == SaveDataOk);
}

void TestUnimplementedDeleteVariants() {
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteBackupData()); }, "sceSaveDataDeleteBackupData");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteAllUser()); }, "sceSaveDataDeleteAllUser");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteCloudData()); }, "sceSaveDataDeleteCloudData");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteProspero()); }, "sceSaveDataDeleteProspero");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteTitle()); }, "sceSaveDataDeleteTitle");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteUser()); }, "sceSaveDataDeleteUser");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataManualDelete()); }, "sceSaveDataManualDelete");
}

void TestPostTerminationBounds() {
    Require(sceSaveDataDeleteTransactionResource(0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataDelete(nullptr) == SaveDataErrorParameter);

    SaveDataDelete del{};
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-delete-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestPreInitializationBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestTransactionResourceLifecycle();
    TestParameterValidationAndPathTraversal();
    TestActiveMountBusyAndLifecycle();
    TestFilesystemTreeCleanupAndIsolation();
    TestUnimplementedDeleteVariants();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    TestPostTerminationBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
