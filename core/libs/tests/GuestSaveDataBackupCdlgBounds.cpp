#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotFound = -2137063416;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

} // namespace

int main() {
    bool throws = false;
    try {
        sceSaveDataCheckBackupDataForCdlg();
    } catch (const std::runtime_error&) {
        throws = true;
    }
    Require(throws);

    throws = false;
    try {
        sceSaveDataRestoreBackupDataForCdlg();
    } catch (const std::runtime_error&) {
        throws = true;
    }
    Require(throws);

    Require(sceSaveDataInitialize3(nullptr) == 0);

    throws = false;
    try {
        sceSaveDataCheckBackupDataForCdlg();
    } catch (const std::runtime_error&) {
        throws = true;
    }
    Require(throws);

    throws = false;
    try {
        sceSaveDataRestoreBackupDataForCdlg();
    } catch (const std::runtime_error&) {
        throws = true;
    }
    Require(throws);

    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataCheckBackupData check{};
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    SceSaveDataDirName dirName{};
    check.dir_name = &dirName;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    std::memset(dirName.data, 'a', sizeof(dirName.data));
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    const char* const invalidNames[] = {
        ".",
        "..",
        "../traversal",
        "..\\traversal",
        "/absolute",
        "\\absolute",
        "dir/slash",
        "dir\\backslash",
        "c:colon",
        ":colon"
    };
    for (const auto* invalidName : invalidNames) {
        std::memset(dirName.data, 0, sizeof(dirName.data));
        std::strncpy(dirName.data, invalidName, sizeof(dirName.data) - 1);
        Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
    }

    std::memset(dirName.data, 0, sizeof(dirName.data));
    std::strncpy(dirName.data, "valid_save_dir", sizeof(dirName.data) - 1);
    SceSaveDataTitleId badTitleId{};
    std::strncpy(badTitleId.data, "CUSA99999", sizeof(badTitleId.data) - 1);
    check.title_id = &badTitleId;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    check.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataRestoreBackupData restore{};
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    restore.dir_name = &dirName;
    std::memset(dirName.data, 0, sizeof(dirName.data));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    std::memset(dirName.data, 'x', sizeof(dirName.data));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    for (const auto* invalidName : invalidNames) {
        std::memset(dirName.data, 0, sizeof(dirName.data));
        std::strncpy(dirName.data, invalidName, sizeof(dirName.data) - 1);
        Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);
    }

    std::memset(dirName.data, 0, sizeof(dirName.data));
    std::strncpy(dirName.data, "valid_save_dir", sizeof(dirName.data) - 1);
    restore.title_id = &badTitleId;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    restore.title_id = nullptr;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    Require(sceSaveDataTerminate() == 0);

    std::memset(dirName.data, 0, sizeof(dirName.data));
    std::strncpy(dirName.data, "valid_save_dir", sizeof(dirName.data) - 1);
    check.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    restore.title_id = nullptr;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    return 0;
}
