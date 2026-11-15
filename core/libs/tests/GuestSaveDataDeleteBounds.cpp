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

    for (const char* invalid : {".", "..", "../escape", "..\\escape", "sub/dir", "sub\\dir", "path:colon", "/rooted", "\\rooted"}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        del.dir_name = &invalidDir;
        Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);
    }
}

void TestActiveMountBusyAndUnmountedDeletion() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "del_target_save", sizeof("del_target_save"));

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

    SaveDataMount3 openMount{};
    openMount.user_id = 1;
    openMount.dir_name = &dirName;
    openMount.mount_mode = MountModeRdWr;
    SaveDataMountResult openResult{};
    Require(sceSaveDataMount3(&openMount, &openResult) == SaveDataErrorNotFound);

    Require(sceSaveDataDelete(&del) == SaveDataOk);
}

void TestDeleteWithBackupCleanup() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "backup_del_save", sizeof("backup_del_save"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataOk);

    SaveDataCheckBackupData check{};
    check.dir_name = &dirName;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &dirName;
    Require(sceSaveDataDelete(&del) == SaveDataOk);

    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
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

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-delete-bounds-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestPreInitializationBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(1) == SaveDataOk);
    TestParameterValidationAndPathTraversal();
    TestActiveMountBusyAndUnmountedDeletion();
    TestDeleteWithBackupCleanup();
    TestUnimplementedDeleteVariants();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
