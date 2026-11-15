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
constexpr int SaveDataErrorNotInitialized = -2137063423;
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

void TestDatabaseAndUserDebtStubs() {
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataRebuildDatabase()); }, "sceSaveDataRebuildDatabase");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataChangeDatabase()); }, "sceSaveDataChangeDatabase");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteAllUser()); }, "sceSaveDataDeleteAllUser");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteTitle()); }, "sceSaveDataDeleteTitle");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteUser()); }, "sceSaveDataDeleteUser");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteProspero()); }, "sceSaveDataDeleteProspero");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDeleteBackupData()); }, "sceSaveDataDeleteBackupData");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataChangeInternal()); }, "sceSaveDataChangeInternal");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDirNameSearchInternal()); }, "sceSaveDataDirNameSearchInternal");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataIsDeletingUsbDb()); }, "sceSaveDataIsDeletingUsbDb");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataManualDelete()); }, "sceSaveDataManualDelete");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataMount5()); }, "sceSaveDataMount5");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataMountInternal()); }, "sceSaveDataMountInternal");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataMountSys()); }, "sceSaveDataMountSys");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataRestoreLoadSaveDataMemory()); }, "sceSaveDataRestoreLoadSaveDataMemory");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataShutdownStart()); }, "sceSaveDataShutdownStart");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataSupportedFakeBrokenStatus()); }, "sceSaveDataSupportedFakeBrokenStatus");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataUmountSys()); }, "sceSaveDataUmountSys");
}

void TestTitleAndUserBoundsDuringActiveSession() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "db_user_title_save", sizeof("db_user_title_save"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    char foreignTitle[16] = "CUSA11111";
    SaveDataMountPs4 foreignMountPs4{};
    foreignMountPs4.title_id = foreignTitle;
    foreignMountPs4.dir_name = &dirName;
    foreignMountPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    RequireThrowsUnimplemented([&] { static_cast<void>(sceSaveDataMount(&foreignMountPs4, &mountResult)); }, "sceSaveDataMount of another title's save data");

    SceSaveDataTitleId foreignTitleId{};
    std::memcpy(foreignTitleId.data, "CUSA11111", sizeof("CUSA11111"));
    SaveDataCheckBackupData check{};
    check.dir_name = &dirName;
    check.title_id = &foreignTitleId;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    TestDatabaseAndUserDebtStubs();

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    SaveDataDelete delReq{};
    delReq.dir_name = &dirName;
    Require(sceSaveDataDelete(&delReq) == SaveDataOk);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-db-user-title-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestDatabaseAndUserDebtStubs();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);

    TestTitleAndUserBoundsDuringActiveSession();

    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    TestDatabaseAndUserDebtStubs();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
