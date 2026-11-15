#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <cstdint>
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

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;
constexpr std::uint32_t ParamTypeTitle = 1;

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

void TestDeleteDebtStubs() {
    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataDeleteBackupData());
    }, "sceSaveDataDeleteBackupData");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataDeleteProspero());
    }, "sceSaveDataDeleteProspero");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataDeleteUser());
    }, "sceSaveDataDeleteUser");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataManualDelete());
    }, "sceSaveDataManualDelete");
}

void TestPreInitializationState() {
    Require(sceSaveDataDeleteTransactionResource(0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataDeleteTransactionResource(42) == SaveDataErrorNotInitialized);
    Require(sceSaveDataCreateTransactionResource(0) == SaveDataErrorNotInitialized);

    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "unopened_preinit", sizeof("unopened_preinit"));
    SaveDataMountInfo mountInfo{};
    Require(sceSaveDataGetMountInfo(&unmountedPoint, &mountInfo) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &unmountedPoint) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmountWithBackup(&unmountedPoint) == SaveDataErrorNotInitialized);
}

void TestDeleteParameterBounds() {
    Require(sceSaveDataDelete(nullptr) == SaveDataErrorParameter);

    SaveDataDelete del{};
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    del.dir_name = &emptyDir;
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'd', sizeof(unterminatedDir.data));
    del.dir_name = &unterminatedDir;
    Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);

    for (const char* invalidName : {".", "..", "escape/../bad", "../parent", "bad\\backslash", "bad/slash", "bad:colon"}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalidName);
        del.dir_name = &invalidDir;
        Require(sceSaveDataDelete(&del) == SaveDataErrorParameter);
    }

    SceSaveDataDirName nonExistentDir{};
    std::memcpy(nonExistentDir.data, "absent_dir", sizeof("absent_dir"));
    del.dir_name = &nonExistentDir;
    for (const std::int32_t userId : {0, 1, -1, 0x10000000, 0x7FFFFFFF}) {
        del.user_id = userId;
        Require(sceSaveDataDelete(&del) == SaveDataOk);
    }
}

void TestActiveMountBusyAndUnopenedPaths() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "DELETE_MOUNT_DIR", sizeof("DELETE_MOUNT_DIR"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    const char sampleTitle[] = "DeleteTargetTitle";
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeTitle, sampleTitle, sizeof(sampleTitle)) == SaveDataOk);

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &dirName;
    Require(sceSaveDataDelete(&del) == SaveDataErrorBusy);

    char titleBuf[128]{};
    std::size_t gotSize = 0;
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeTitle, titleBuf, sizeof(titleBuf), &gotSize) == SaveDataOk);
    Require(std::strcmp(titleBuf, sampleTitle) == 0);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeTitle, titleBuf, sizeof(titleBuf), &gotSize) == SaveDataErrorNotMounted);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeTitle, titleBuf, sizeof(titleBuf)) == SaveDataErrorNotMounted);

    SaveDataMountPoint unopenedPoint{};
    std::memcpy(unopenedPoint.data, "unopened_slot_test", sizeof("unopened_slot_test"));
    Require(sceSaveDataGetMountInfo(&unopenedPoint, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &unopenedPoint) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmountWithBackup(&unopenedPoint) == SaveDataErrorNotMounted);

    Require(sceSaveDataDelete(&del) == SaveDataOk);
    Require(sceSaveDataDelete(&del) == SaveDataOk);
}

void TestTransactionResourceBounds() {
    const int res1 = sceSaveDataCreateTransactionResource(0);
    Require(res1 > 0);
    const int res2 = sceSaveDataCreateTransactionResource(1024);
    Require(res2 > res1);

    Require(sceSaveDataDeleteTransactionResource(res1) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(res2) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(0) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(-1) == SaveDataOk);
    Require(sceSaveDataDeleteTransactionResource(9999) == SaveDataOk);
}

void TestTerminationLifecycle() {
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    Require(sceSaveDataDeleteTransactionResource(0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataCreateTransactionResource(0) == SaveDataErrorNotInitialized);

    TestDeleteDebtStubs();
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-delete-unopened-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestDeleteDebtStubs();
    TestPreInitializationState();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);

    TestDeleteDebtStubs();
    TestDeleteParameterBounds();
    TestActiveMountBusyAndUnopenedPaths();
    TestTransactionResourceBounds();
    TestTerminationLifecycle();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
