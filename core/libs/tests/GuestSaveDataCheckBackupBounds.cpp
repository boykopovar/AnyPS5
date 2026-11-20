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
#include <source_location>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

constexpr std::uint32_t ParamTypeTitle = 1;
constexpr std::uint32_t ParamTypeSubTitle = 2;
constexpr std::uint32_t ParamTypeDetail = 3;
constexpr std::uint32_t ParamTypeUserParam = 4;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void TestPreInitializationBounds() {
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorNotInitialized);

    SaveDataCheckBackupData check{};
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "pre_init_dir", sizeof("pre_init_dir"));
    check.dir_name = &validDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    SceSaveDataTitleId foreignTitle{};
    std::memcpy(foreignTitle.data, "CUSA99999", sizeof("CUSA99999"));
    check.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    SaveDataParam param{};
    check.param = &param;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    SaveDataIcon icon{};
    check.icon = &icon;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);
}

void TestParameterValidationAndErrorPrecedence() {
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataCheckBackupData check{};
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    check.dir_name = &emptyDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'x', sizeof(unterminatedDir.data));
    check.dir_name = &unterminatedDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);

    const char* const invalidNames[] = {
        ".",
        "..",
        "nested/path",
        "nested\\path",
        "colon:name",
        "../parent_dir",
        "traversal/../dir"
    };
    for (const char* invalidName : invalidNames) {
        SceSaveDataDirName badDir{};
        std::strncpy(badDir.data, invalidName, sizeof(badDir.data) - 1);
        check.dir_name = &badDir;
        Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
    }

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "precedence_save", sizeof("precedence_save"));
    check.dir_name = &validDir;

    SceSaveDataTitleId foreignTitle{};
    std::memcpy(foreignTitle.data, "CUSA99999", sizeof("CUSA99999"));
    check.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    SceSaveDataTitleId foreignTitlePpsa{};
    std::memcpy(foreignTitlePpsa.data, "PPSA00000", sizeof("PPSA00000"));
    check.title_id = &foreignTitlePpsa;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    check.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &validDir;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    check.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    check.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);

    SaveDataParam param{};
    check.param = &param;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);

    SaveDataIcon icon{};
    check.icon = &icon;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    check.param = nullptr;
    check.icon = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    SaveDataDelete deleteReq{};
    deleteReq.dir_name = &validDir;
    Require(sceSaveDataDelete(&deleteReq) == SaveDataOk);
}

void TestBackupCheckLifecycleAndParamRetrieval() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "backup_lifecycle_dir", sizeof("backup_lifecycle_dir"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    const char titleText[] = "LifecycleBackupTitle";
    const char subTitleText[] = "LifecycleBackupSubTitle";
    const char detailText[] = "Detailed save backup checkpoint description.";
    const std::uint32_t userParamVal = 0x87654321u;

    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeTitle, titleText, sizeof(titleText)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeSubTitle, subTitleText, sizeof(subTitleText)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeDetail, detailText, sizeof(detailText)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeUserParam, &userParamVal, sizeof(userParamVal)) == SaveDataOk);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataOk);

    SaveDataEvent event{};
    Require(sceSaveDataGetEventResult(nullptr, &event) == SaveDataOk);

    SaveDataCheckBackupData check{};
    check.dir_name = &dirName;
    check.title_id = nullptr;
    check.param = nullptr;
    check.icon = nullptr;

    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);

    SaveDataParam retrievedParam{};
    check.param = &retrievedParam;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);
    Require(std::strcmp(retrievedParam.title, titleText) == 0);
    Require(std::strcmp(retrievedParam.sub_title, subTitleText) == 0);
    Require(std::strcmp(retrievedParam.detail, detailText) == 0);
    Require(retrievedParam.user_param == userParamVal);

    SaveDataIcon dummyIcon{};
    check.icon = &dummyIcon;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);

    SceSaveDataTitleId foreignTitle{};
    std::memcpy(foreignTitle.data, "PPSA99999", sizeof("PPSA99999"));
    check.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    check.title_id = nullptr;

    SaveDataMountResult remountResult{};
    Require(sceSaveDataMount3(&mount, &remountResult) == SaveDataOk);
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);
    Require(sceSaveDataUmount2(0, &remountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);

    SaveDataDelete deleteReq{};
    deleteReq.dir_name = &dirName;
    Require(sceSaveDataDelete(&deleteReq) == SaveDataOk);
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-check-backup-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox / "app0" / "sce_sys");
    std::ofstream paramFile(sandbox / "app0" / "sce_sys" / "param.json");
    paramFile << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"BackupCheckTest"}}})";
    paramFile.close();

    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestPreInitializationBounds();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);

    TestParameterValidationAndErrorPrecedence();
    TestBackupCheckLifecycleAndParamRetrieval();

    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    TestPreInitializationBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
