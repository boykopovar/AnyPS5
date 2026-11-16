#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <algorithm>
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

void CheckCloudPsnStubs() {
    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetAllSizeProspero());
    }, "sceSaveDataGetAllSizeProspero");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataDownload());
    }, "sceSaveDataDownload");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataUpload());
    }, "sceSaveDataUpload");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataSyncCloudList());
    }, "sceSaveDataSyncCloudList");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataBindPsnAccount());
    }, "sceSaveDataBindPsnAccount");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataBindPsnAccountForSystemBackup());
    }, "sceSaveDataBindPsnAccountForSystemBackup");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataCheckCloudData());
    }, "sceSaveDataCheckCloudData");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataCheckPremiumMember());
    }, "sceSaveDataCheckPremiumMember");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataCheckSyncStatus());
    }, "sceSaveDataCheckSyncStatus");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataCreateUploadData());
    }, "sceSaveDataCreateUploadData");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataDeleteCloudData());
    }, "sceSaveDataDeleteCloudData");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetAutoSyncInfo());
    }, "sceSaveDataGetAutoSyncInfo");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetAutoUploadConditions());
    }, "sceSaveDataGetAutoUploadConditions");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetAutoUploadRequestInfo());
    }, "sceSaveDataGetAutoUploadRequestInfo");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetAutoUploadSetting());
    }, "sceSaveDataGetAutoUploadSetting");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetCloudInfo());
    }, "sceSaveDataGetCloudInfo");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataGetCloudQuotaInfo());
    }, "sceSaveDataGetCloudQuotaInfo");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataManualDownload());
    }, "sceSaveDataManualDownload");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataManualUpload());
    }, "sceSaveDataManualUpload");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataNotifySaveDataDownload());
    }, "sceSaveDataNotifySaveDataDownload");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataSetAutoSyncSettings());
    }, "sceSaveDataSetAutoSyncSettings");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataSetAutoUploadSetting());
    }, "sceSaveDataSetAutoUploadSetting");
}

void TestActiveMountLifecycle() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "CLOUD_TEST_DIR", sizeof("CLOUD_TEST_DIR"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    CheckCloudPsnStubs();

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &dirName;
    Require(sceSaveDataDelete(&del) == SaveDataOk);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-cloud-psn-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    CheckCloudPsnStubs();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    CheckCloudPsnStubs();

    TestActiveMountLifecycle();
    CheckCloudPsnStubs();

    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    CheckCloudPsnStubs();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
