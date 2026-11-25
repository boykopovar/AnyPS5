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

SaveDataParam MakeSentinelParam() {
    SaveDataParam param{};
    std::memset(&param, 0xA5, sizeof(param));
    return param;
}

bool ParamMatches(const SaveDataParam& lhs, const SaveDataParam& rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(SaveDataParam)) == 0;
}

SceSaveDataDirName MakeDirName(const char* name) {
    SceSaveDataDirName dir{};
    std::strncpy(dir.data, name, sizeof(dir.data) - 1);
    return dir;
}

SceSaveDataTitleId MakeTitleId(const char* title) {
    SceSaveDataTitleId id{};
    std::strncpy(id.data, title, sizeof(id.data) - 1);
    return id;
}

void TestPreInitializationBounds() {
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorNotInitialized);
    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorNotInitialized);

    SaveDataCheckBackupData check{};
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);

    SaveDataRestoreBackupData restore{};
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    const SceSaveDataDirName validDir = MakeDirName("pre_init_dir");
    check.dir_name = &validDir;
    restore.dir_name = &validDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    const SceSaveDataTitleId foreignTitle = MakeTitleId("CUSA99999");
    check.title_id = &foreignTitle;
    restore.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    const SceSaveDataTitleId foreignTitlePpsa = MakeTitleId("PPSA99999");
    check.title_id = &foreignTitlePpsa;
    restore.title_id = &foreignTitlePpsa;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized);

    SaveDataParam param = MakeSentinelParam();
    const SaveDataParam untouched = param;
    check.param = &param;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);
    Require(ParamMatches(param, untouched));

    SaveDataIcon icon{};
    check.icon = &icon;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized);
    Require(ParamMatches(param, untouched));
}

void TestParameterValidationAndErrorPrecedence() {
    Require(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter);

    SaveDataParam param = MakeSentinelParam();
    const SaveDataParam untouched = param;

    SaveDataCheckBackupData check{};
    check.param = &param;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
    Require(ParamMatches(param, untouched));

    SaveDataRestoreBackupData restore{};
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    check.dir_name = &emptyDir;
    restore.dir_name = &emptyDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'x', sizeof(unterminatedDir.data));
    check.dir_name = &unterminatedDir;
    restore.dir_name = &unterminatedDir;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);

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
        const SceSaveDataDirName badDir = MakeDirName(invalidName);
        check.dir_name = &badDir;
        restore.dir_name = &badDir;
        Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter);
        Require(ParamMatches(param, untouched));
        Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter);
    }

    const SceSaveDataDirName validDir = MakeDirName("precedence_save");
    check.dir_name = &validDir;
    restore.dir_name = &validDir;

    const SceSaveDataTitleId foreignTitle = MakeTitleId("CUSA99999");
    check.title_id = &foreignTitle;
    restore.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    const SceSaveDataTitleId foreignTitlePpsa = MakeTitleId("PPSA99999");
    check.title_id = &foreignTitlePpsa;
    restore.title_id = &foreignTitlePpsa;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    const SceSaveDataTitleId localTitlePpsa = MakeTitleId("PPSA00000");
    check.title_id = &localTitlePpsa;
    restore.title_id = &localTitlePpsa;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    check.title_id = nullptr;
    restore.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &validDir;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    check.title_id = &foreignTitle;
    restore.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    check.title_id = &foreignTitlePpsa;
    restore.title_id = &foreignTitlePpsa;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    check.title_id = &localTitlePpsa;
    restore.title_id = &localTitlePpsa;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorBusy);

    check.title_id = nullptr;
    restore.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorBusy);

    SaveDataIcon icon{};
    check.icon = &icon;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);
    Require(ParamMatches(param, untouched));

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    check.param = &param;
    check.icon = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(param, untouched));
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    SaveDataDelete deleteReq{};
    deleteReq.dir_name = &validDir;
    Require(sceSaveDataDelete(&deleteReq) == SaveDataOk);
}

void TestBackupCheckLifecycleAndParamRetrieval() {
    const SceSaveDataDirName dirName = MakeDirName("backup_lifecycle_dir");

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

    SaveDataParam retrievedParam = MakeSentinelParam();
    check.param = &retrievedParam;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);
    Require(std::strcmp(retrievedParam.title, titleText) == 0);
    Require(std::strcmp(retrievedParam.sub_title, subTitleText) == 0);
    Require(std::strcmp(retrievedParam.detail, detailText) == 0);
    Require(retrievedParam.user_param == userParamVal);

    const SceSaveDataTitleId localTitle = MakeTitleId("PPSA00000");
    SaveDataParam localRetrievedParam = MakeSentinelParam();
    check.title_id = &localTitle;
    check.param = &localRetrievedParam;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);
    Require(std::strcmp(localRetrievedParam.title, titleText) == 0);
    Require(std::strcmp(localRetrievedParam.sub_title, subTitleText) == 0);
    Require(std::strcmp(localRetrievedParam.detail, detailText) == 0);
    Require(localRetrievedParam.user_param == userParamVal);

    SaveDataIcon dummyIcon{};
    check.icon = &dummyIcon;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);

    const SceSaveDataTitleId foreignTitle = MakeTitleId("PPSA99999");
    SaveDataParam sentinelParam = MakeSentinelParam();
    const SaveDataParam untouched = sentinelParam;
    check.title_id = &foreignTitle;
    check.param = &sentinelParam;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(sentinelParam, untouched));
    check.title_id = nullptr;

    SaveDataMountResult remountResult{};
    Require(sceSaveDataMount3(&mount, &remountResult) == SaveDataOk);
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);
    Require(ParamMatches(sentinelParam, untouched));
    check.title_id = &localTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy);
    Require(ParamMatches(sentinelParam, untouched));
    check.title_id = &foreignTitle;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(sentinelParam, untouched));
    check.title_id = nullptr;
    Require(sceSaveDataUmount2(0, &remountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataCheckBackupData(&check) == SaveDataOk);

    SaveDataDelete deleteReq{};
    deleteReq.dir_name = &dirName;
    Require(sceSaveDataDelete(&deleteReq) == SaveDataOk);
    sentinelParam = MakeSentinelParam();
    const SaveDataParam postDeleteUntouched = sentinelParam;
    check.param = &sentinelParam;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);
    Require(ParamMatches(sentinelParam, postDeleteUntouched));
}

void TestRestoreBackupLifecycle() {
    const SceSaveDataDirName dirName = MakeDirName("restore_lifecycle_dir");
    const SceSaveDataTitleId localTitle = MakeTitleId("PPSA00000");
    const SceSaveDataTitleId foreignTitle = MakeTitleId("PPSA99999");

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    const char v1Title[] = "BackupV1Title";
    const char v1SubTitle[] = "BackupV1SubTitle";
    const char v1Detail[] = "Backed up save metadata v1.";
    const std::uint32_t v1UserParam = 0x11223344u;

    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeTitle, v1Title, sizeof(v1Title)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeSubTitle, v1SubTitle, sizeof(v1SubTitle)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeDetail, v1Detail, sizeof(v1Detail)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeUserParam, &v1UserParam, sizeof(v1UserParam)) == SaveDataOk);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == SaveDataOk);

    SaveDataEvent event{};
    Require(sceSaveDataGetEventResult(nullptr, &event) == SaveDataOk);

    SaveDataMount3 remount{};
    remount.user_id = 1;
    remount.dir_name = &dirName;
    remount.mount_mode = MountModeRdWr;
    SaveDataMountResult remountResult{};
    Require(sceSaveDataMount3(&remount, &remountResult) == SaveDataOk);

    const char v2Title[] = "ModifiedV2Title";
    const char v2SubTitle[] = "ModifiedV2SubTitle";
    const char v2Detail[] = "Modified live save metadata v2.";
    const std::uint32_t v2UserParam = 0xAABBCCDDu;

    Require(sceSaveDataSetParam(&remountResult.mount_point, ParamTypeTitle, v2Title, sizeof(v2Title)) == SaveDataOk);
    Require(sceSaveDataSetParam(&remountResult.mount_point, ParamTypeSubTitle, v2SubTitle, sizeof(v2SubTitle)) == SaveDataOk);
    Require(sceSaveDataSetParam(&remountResult.mount_point, ParamTypeDetail, v2Detail, sizeof(v2Detail)) == SaveDataOk);
    Require(sceSaveDataSetParam(&remountResult.mount_point, ParamTypeUserParam, &v2UserParam, sizeof(v2UserParam)) == SaveDataOk);

    SaveDataRestoreBackupData restore{};
    restore.dir_name = &dirName;
    restore.title_id = &foreignTitle;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    restore.title_id = &localTitle;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorBusy);

    restore.title_id = nullptr;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorBusy);

    char liveTitle[128]{};
    std::size_t gotSize = 0;
    Require(sceSaveDataGetParam(&remountResult.mount_point, ParamTypeTitle, liveTitle, sizeof(liveTitle), &gotSize) == SaveDataOk);
    Require(std::strcmp(liveTitle, v2Title) == 0);

    Require(sceSaveDataUmount2(0, &remountResult.mount_point) == SaveDataOk);

    restore.title_id = &foreignTitle;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);

    restore.title_id = &localTitle;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataOk);

    SaveDataMountResult restoredMount{};
    Require(sceSaveDataMount3(&remount, &restoredMount) == SaveDataOk);

    char gotTitle[128]{};
    char gotSubTitle[128]{};
    char gotDetail[1024]{};
    std::uint32_t gotUserParam = 0;

    Require(sceSaveDataGetParam(&restoredMount.mount_point, ParamTypeTitle, gotTitle, sizeof(gotTitle), &gotSize) == SaveDataOk);
    Require(std::strcmp(gotTitle, v1Title) == 0);

    Require(sceSaveDataGetParam(&restoredMount.mount_point, ParamTypeSubTitle, gotSubTitle, sizeof(gotSubTitle), &gotSize) == SaveDataOk);
    Require(std::strcmp(gotSubTitle, v1SubTitle) == 0);

    Require(sceSaveDataGetParam(&restoredMount.mount_point, ParamTypeDetail, gotDetail, sizeof(gotDetail), &gotSize) == SaveDataOk);
    Require(std::strcmp(gotDetail, v1Detail) == 0);

    Require(sceSaveDataGetParam(&restoredMount.mount_point, ParamTypeUserParam, &gotUserParam, sizeof(gotUserParam), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(gotUserParam));
    Require(gotUserParam == v1UserParam);

    Require(sceSaveDataSetParam(&restoredMount.mount_point, ParamTypeTitle, v2Title, sizeof(v2Title)) == SaveDataOk);
    Require(sceSaveDataUmount2(0, &restoredMount.mount_point) == SaveDataOk);

    restore.title_id = nullptr;
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataOk);

    SaveDataMountResult secondRestoreMount{};
    Require(sceSaveDataMount3(&remount, &secondRestoreMount) == SaveDataOk);
    std::memset(gotTitle, 0, sizeof(gotTitle));
    Require(sceSaveDataGetParam(&secondRestoreMount.mount_point, ParamTypeTitle, gotTitle, sizeof(gotTitle), &gotSize) == SaveDataOk);
    Require(std::strcmp(gotTitle, v1Title) == 0);
    Require(sceSaveDataUmount2(0, &secondRestoreMount.mount_point) == SaveDataOk);

    SaveDataDelete deleteReq{};
    deleteReq.dir_name = &dirName;
    Require(sceSaveDataDelete(&deleteReq) == SaveDataOk);
    Require(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound);
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
    TestRestoreBackupLifecycle();

    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    TestPreInitializationBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
