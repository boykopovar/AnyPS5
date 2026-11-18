#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
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

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr int SaveDataErrorMountFull = -2137063412;
constexpr int SaveDataErrorMemoryNotReady = -2137063406;

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

struct MemorySyncParam {
    std::int32_t user_id;
    std::uint32_t slot_id;
};

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

    char buf[64]{};
    Require(sceSaveDataSetupSaveDataMemory(1, 1024, nullptr) == SaveDataErrorNotInitialized);
    Require(sceSaveDataGetSaveDataMemory(1, buf, sizeof(buf), 0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataSetSaveDataMemory(1, buf, sizeof(buf), 0) == SaveDataErrorNotInitialized);

    MemorySyncParam sync{1, 0};
    Require(sceSaveDataSyncSaveDataMemory(&sync) == SaveDataErrorNotInitialized);
}

void TestBackupAndRestoreParameterBounds() {
    Require(sceSaveDataInitialize3(nullptr) == 0);

    Require(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "unmounted", sizeof("unmounted"));
    Require(sceSaveDataUmountWithBackup(&unmountedPoint) == SaveDataErrorNotMounted);

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'x', sizeof(unterminatedPoint.data));
    Require(sceSaveDataUmountWithBackup(&unterminatedPoint) == SaveDataErrorParameter);

    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataCheckBackupData nullDirCheck{};
    Require(sceSaveDataCheckBackupData(&nullDirCheck) == SaveDataErrorParameter);

    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataRestoreBackupData nullDirRestore{};
    Require(sceSaveDataRestoreBackupData(&nullDirRestore) == SaveDataErrorParameter);

    Require(sceSaveDataBackup(nullptr) == SaveDataErrorParameter);

    SaveDataBackup nullDirBackup{};
    Require(sceSaveDataBackup(&nullDirBackup) == SaveDataErrorParameter);

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
    std::memset(unterminatedDir.data, 'z', sizeof(unterminatedDir.data));
    SaveDataCheckBackupData unterminatedCheck{};
    unterminatedCheck.dir_name = &unterminatedDir;
    Require(sceSaveDataCheckBackupData(&unterminatedCheck) == SaveDataErrorParameter);

    SaveDataRestoreBackupData unterminatedRestore{};
    unterminatedRestore.dir_name = &unterminatedDir;
    Require(sceSaveDataRestoreBackupData(&unterminatedRestore) == SaveDataErrorParameter);

    SaveDataBackup unterminatedBackup{};
    unterminatedBackup.dir_name = &unterminatedDir;
    Require(sceSaveDataBackup(&unterminatedBackup) == SaveDataErrorParameter);
}

void TestBackupCheckAndRestoreTitleIdAndExistence() {
    SceSaveDataDirName missingDir{};
    std::memcpy(missingDir.data, "missing_save", sizeof("missing_save"));

    SaveDataCheckBackupData missingCheck{};
    missingCheck.dir_name = &missingDir;
    Require(sceSaveDataCheckBackupData(&missingCheck) == SaveDataErrorNotFound);

    SaveDataRestoreBackupData missingRestore{};
    missingRestore.dir_name = &missingDir;
    Require(sceSaveDataRestoreBackupData(&missingRestore) == SaveDataErrorNotFound);

    SceSaveDataTitleId mismatchedTitleId{};
    std::memcpy(mismatchedTitleId.data, "CUSA99999", sizeof("CUSA99999"));
    missingCheck.title_id = &mismatchedTitleId;
    Require(sceSaveDataCheckBackupData(&missingCheck) == SaveDataErrorNotFound);

    missingRestore.title_id = &mismatchedTitleId;
    Require(sceSaveDataRestoreBackupData(&missingRestore) == SaveDataErrorNotFound);
}

void TestBackupLifecycleAndBusyState() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "backup_quota", sizeof("backup_quota"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;
    mount.blocks = 96;
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

    SaveDataBackup backupOp{};
    backupOp.dir_name = &dirName;
    Require(sceSaveDataBackup(&backupOp) == 0);
}

void TestQuotaAllocationLimits() {
    Require(sceSaveDataSetupSaveDataMemory(1, 0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSetupSaveDataMemory(1, (32u * 1024u * 1024u) + 1u, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSetupSaveDataMemory(1, 4096, nullptr) == 0);

    std::array<char, 64> testBuf{};
    testBuf.fill(0x5A);
    Require(sceSaveDataSetSaveDataMemory(1, testBuf.data(), testBuf.size(), 0) == 0);

    std::array<char, 64> readBuf{};
    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), readBuf.size(), 0) == 0);
    for (char b : readBuf) {
        Require(b == 0x5A);
    }

    Require(sceSaveDataSetSaveDataMemory(1, testBuf.data(), testBuf.size(), 4050) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), readBuf.size(), 4050) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(1, testBuf.data(), 1, 4096) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), 1, 4096) == SaveDataErrorParameter);

    Require(sceSaveDataSyncSaveDataMemory(nullptr) == SaveDataErrorParameter);

    MemorySyncParam sync{1, 0};
    Require(sceSaveDataSyncSaveDataMemory(&sync) == 0);

    MemorySyncParam unreadySync{99, 0};
    Require(sceSaveDataSyncSaveDataMemory(&unreadySync) == SaveDataErrorMemoryNotReady);
}

void TestMountSlotQuotaLimits() {
    std::array<SaveDataMountPoint, 16> mountedPoints{};
    for (std::size_t i = 0; i < 16; ++i) {
        SceSaveDataDirName slotDir{};
        std::snprintf(slotDir.data, sizeof(slotDir.data), "slot_dir_%zu", i);
        SaveDataMount3 slotMount{};
        slotMount.dir_name = &slotDir;
        slotMount.mount_mode = MountReadWrite | MountCreate2;
        slotMount.blocks = 96;
        SaveDataMountResult slotResult{};
        Require(sceSaveDataMount3(&slotMount, &slotResult) == 0);
        mountedPoints[i] = slotResult.mount_point;
    }

    SceSaveDataDirName overflowDir{};
    std::memcpy(overflowDir.data, "overflow_slot", sizeof("overflow_slot"));
    SaveDataMount3 overflowMount{};
    overflowMount.dir_name = &overflowDir;
    overflowMount.mount_mode = MountReadWrite | MountCreate2;
    overflowMount.blocks = 96;
    SaveDataMountResult overflowResult{};
    Require(sceSaveDataMount3(&overflowMount, &overflowResult) == SaveDataErrorMountFull);

    for (std::size_t i = 0; i < 16; ++i) {
        Require(sceSaveDataUmount2(0, &mountedPoints[i]) == 0);
    }
}

void TestUnimplementedDebtPlaceholders() {
    RequireThrowsUnimplemented([] { (void)sceSaveDataCheckBackupDataForCdlg(); }, "sceSaveDataCheckBackupDataForCdlg");
    RequireThrowsUnimplemented([] { (void)sceSaveDataCheckBackupDataInternal(); }, "sceSaveDataCheckBackupDataInternal");
    RequireThrowsUnimplemented([] { (void)sceSaveDataCheckBackupDataLight(); }, "sceSaveDataCheckBackupDataLight");
    RequireThrowsUnimplemented([] { (void)sceSaveDataCheckSaveDataBroken(); }, "sceSaveDataCheckSaveDataBroken");
    RequireThrowsUnimplemented([] { (void)sceSaveDataCheckSaveDataVersion(); }, "sceSaveDataCheckSaveDataVersion");
    RequireThrowsUnimplemented([] { (void)sceSaveDataCheckSaveDataVersionLatest(); }, "sceSaveDataCheckSaveDataVersionLatest");
    RequireThrowsUnimplemented([] { (void)sceSaveDataRestoreBackupDataForCdlg(); }, "sceSaveDataRestoreBackupDataForCdlg");
    RequireThrowsUnimplemented([] { (void)sceSaveDataRestoreLoadSaveDataMemory(); }, "sceSaveDataRestoreLoadSaveDataMemory");
    RequireThrowsUnimplemented([] { (void)sceSaveDataGetCloudQuotaInfo(); }, "sceSaveDataGetCloudQuotaInfo");
    RequireThrowsUnimplemented([] { (void)sceSaveDataGetAllSizeProspero(); }, "sceSaveDataGetAllSizeProspero");
    std::uint64_t allSize = 0;
    RequireThrowsUnimplemented([&] { (void)sceSaveDataGetAllSize(0, &allSize); }, "sceSaveDataGetAllSize");
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-backup-quota-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestUninitializedBounds();
    TestBackupAndRestoreParameterBounds();
    TestBackupCheckAndRestoreTitleIdAndExistence();
    TestBackupLifecycleAndBusyState();
    TestQuotaAllocationLimits();
    TestMountSlotQuotaLimits();
    TestUnimplementedDebtPlaceholders();

    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
