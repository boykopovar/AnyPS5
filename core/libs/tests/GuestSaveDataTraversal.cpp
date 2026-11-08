#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
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
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate = 4;
constexpr std::uint32_t MountModeCreate2 = 32;

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotMounted = -2137063420;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", message.c_str());
    ++g_failures;
}

void CheckNullPointers() {
    SaveDataMountResult result{};
    Check(sceSaveDataMount3(nullptr, &result) == SaveDataErrorParameter, "mount3 null mount");
    SaveDataMount3 nullResultMount{};
    Check(sceSaveDataMount3(&nullResultMount, nullptr) == SaveDataErrorParameter, "mount3 null result");
    Check(sceSaveDataMount3(&nullResultMount, &result) == SaveDataErrorParameter, "mount3 null dir_name");

    SaveDataMount2 nullMount2{};
    Check(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter, "mount2 null mount");
    Check(sceSaveDataMount2(&nullMount2, nullptr) == SaveDataErrorParameter, "mount2 null result");
    Check(sceSaveDataMount2(&nullMount2, &result) == SaveDataErrorParameter, "mount2 null dir_name");

    SaveDataMountPs4 nullMountPs4{};
    Check(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter, "mount ps4 null mount");
    Check(sceSaveDataMount(&nullMountPs4, nullptr) == SaveDataErrorParameter, "mount ps4 null result");
    Check(sceSaveDataMount(&nullMountPs4, &result) == SaveDataErrorParameter, "mount ps4 null dir_name");

    Check(sceSaveDataDelete(nullptr) == SaveDataErrorParameter, "delete null param");
    SaveDataDelete nullDirDel{};
    Check(sceSaveDataDelete(&nullDirDel) == SaveDataErrorParameter, "delete null dir_name");

    Check(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter, "checkBackupData null param");
    SaveDataCheckBackupData nullDirCheck{};
    Check(sceSaveDataCheckBackupData(&nullDirCheck) == SaveDataErrorParameter, "checkBackupData null dir_name");

    Check(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter, "restoreBackupData null param");
    SaveDataRestoreBackupData nullDirRestore{};
    Check(sceSaveDataRestoreBackupData(&nullDirRestore) == SaveDataErrorParameter, "restoreBackupData null dir_name");
}

void CheckTraversalAndInvalidSequences() {
    const std::vector<std::string> traversalNames = {
        "..",
        ".",
        "../",
        "..\\",
        "/root",
        "\\windows",
        "../..",
        "../../root",
        "../escape",
        "..\\escape",
        "/absolute",
        "\\backslash",
        "dir/../outside",
        "kept/..",
        "nested/dir",
        "nested\\dir",
        "../nested/path",
        "..\\nested\\path",
        "a/b",
        "a\\b",
        "c:d",
        "device:dir",
        "colon:save",
        ":root",
        "save:",
        ""
    };

    for (const auto& invalid : traversalNames) {
        SceSaveDataDirName dirName{};
        if (invalid.size() < sizeof(dirName.data)) {
            std::memcpy(dirName.data, invalid.c_str(), invalid.size() + 1);
        } else {
            std::memcpy(dirName.data, invalid.data(), sizeof(dirName.data));
        }

        SaveDataMount3 mount3Req{};
        mount3Req.user_id = 0x10000000;
        mount3Req.dir_name = &dirName;
        mount3Req.mount_mode = MountModeRdWr | MountModeCreate2;
        SaveDataMountResult result3{};
        Check(sceSaveDataMount3(&mount3Req, &result3) == SaveDataErrorParameter,
              "mount3 rejected traversal: " + invalid);

        SaveDataMount2 mount2Req{};
        mount2Req.user_id = 0x10000000;
        mount2Req.dir_name = &dirName;
        mount2Req.mount_mode = MountModeRdWr | MountModeCreate2;
        mount2Req.blocks = 96;
        SaveDataMountResult result2{};
        Check(sceSaveDataMount2(&mount2Req, &result2) == SaveDataErrorParameter,
              "mount2 rejected traversal: " + invalid);

        SaveDataMountPs4 mountPs4Req{};
        mountPs4Req.user_id = 0x10000000;
        mountPs4Req.dir_name = &dirName;
        mountPs4Req.mount_mode = MountModeRdWr | MountModeCreate2;
        SaveDataMountResult resultPs4{};
        Check(sceSaveDataMount(&mountPs4Req, &resultPs4) == SaveDataErrorParameter,
              "mount ps4 rejected traversal: " + invalid);

        SaveDataDelete delReq{};
        delReq.user_id = 0x10000000;
        delReq.dir_name = &dirName;
        Check(sceSaveDataDelete(&delReq) == SaveDataErrorParameter,
              "delete rejected traversal: " + invalid);

        SaveDataCheckBackupData checkReq{};
        checkReq.user_id = 0x10000000;
        checkReq.dir_name = &dirName;
        Check(sceSaveDataCheckBackupData(&checkReq) == SaveDataErrorParameter,
              "checkBackupData rejected traversal: " + invalid);

        SaveDataRestoreBackupData restoreReq{};
        restoreReq.user_id = 0x10000000;
        restoreReq.dir_name = &dirName;
        Check(sceSaveDataRestoreBackupData(&restoreReq) == SaveDataErrorParameter,
              "restoreBackupData rejected traversal: " + invalid);
    }
}

void CheckUnterminatedDirectoryName() {
    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'x', sizeof(unterminated.data));

    SaveDataMount3 mount3Req{};
    mount3Req.user_id = 0x10000000;
    mount3Req.dir_name = &unterminated;
    mount3Req.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult result3{};
    Check(sceSaveDataMount3(&mount3Req, &result3) == SaveDataErrorParameter,
          "mount3 rejected unterminated dir_name");

    SaveDataMount2 mount2Req{};
    mount2Req.user_id = 0x10000000;
    mount2Req.dir_name = &unterminated;
    mount2Req.mount_mode = MountModeRdWr | MountModeCreate2;
    mount2Req.blocks = 96;
    SaveDataMountResult result2{};
    Check(sceSaveDataMount2(&mount2Req, &result2) == SaveDataErrorParameter,
          "mount2 rejected unterminated dir_name");

    SaveDataMountPs4 mountPs4Req{};
    mountPs4Req.user_id = 0x10000000;
    mountPs4Req.dir_name = &unterminated;
    mountPs4Req.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult resultPs4{};
    Check(sceSaveDataMount(&mountPs4Req, &resultPs4) == SaveDataErrorParameter,
          "mount ps4 rejected unterminated dir_name");

    SaveDataDelete delReq{};
    delReq.user_id = 0x10000000;
    delReq.dir_name = &unterminated;
    Check(sceSaveDataDelete(&delReq) == SaveDataErrorParameter,
          "delete rejected unterminated dir_name");

    SaveDataCheckBackupData checkReq{};
    checkReq.user_id = 0x10000000;
    checkReq.dir_name = &unterminated;
    Check(sceSaveDataCheckBackupData(&checkReq) == SaveDataErrorParameter,
          "checkBackupData rejected unterminated dir_name");

    SaveDataRestoreBackupData restoreReq{};
    restoreReq.user_id = 0x10000000;
    restoreReq.dir_name = &unterminated;
    Check(sceSaveDataRestoreBackupData(&restoreReq) == SaveDataErrorParameter,
          "restoreBackupData rejected unterminated dir_name");
}

void CheckUnderscoreRejectionInMount2() {
    const std::vector<std::string> underscoredNames = {
        "my_save",
        "survey_test",
        "SURVEY_TEST",
        "_hidden",
        "save_data",
        "slot_0",
        "overflow_slot"
    };

    for (const auto& name : underscoredNames) {
        SceSaveDataDirName dirName{};
        std::memcpy(dirName.data, name.c_str(), name.size() + 1);
        SaveDataMount2 mount2Req{};
        mount2Req.user_id = 0x10000000;
        mount2Req.dir_name = &dirName;
        mount2Req.mount_mode = MountModeRdWr | MountModeCreate2;
        mount2Req.blocks = 96;
        SaveDataMountResult result2{};
        Check(sceSaveDataMount2(&mount2Req, &result2) == SaveDataErrorParameter,
              "mount2 rejected underscored name: " + name);
    }
}

void CheckValidDirectoryNameLifecycle() {
    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "validSave01", sizeof("validSave01"));

    SaveDataMount3 mount3Req{};
    mount3Req.user_id = 0x10000000;
    mount3Req.dir_name = &validDir;
    mount3Req.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult result3{};
    Check(sceSaveDataMount3(&mount3Req, &result3) == SaveDataOk,
          "mount3 valid save data created and mounted");
    Check(result3.mount_status == 1u, "mount3 reported newly created status");

    Check(sceSaveDataUmount2(0, &result3.mount_point) == SaveDataOk,
          "umount2 unmounted valid save data");

    SceSaveDataDirName validDir2{};
    std::memcpy(validDir2.data, "validSave02", sizeof("validSave02"));

    SaveDataMount2 mount2Req{};
    mount2Req.user_id = 0x10000000;
    mount2Req.dir_name = &validDir2;
    mount2Req.mount_mode = MountModeRdWr | MountModeCreate2;
    mount2Req.blocks = 96;
    SaveDataMountResult result2{};
    Check(sceSaveDataMount2(&mount2Req, &result2) == SaveDataOk,
          "mount2 valid save data created and mounted");

    Check(sceSaveDataUmount2(0, &result2.mount_point) == SaveDataOk,
          "umount2 unmounted second valid save data");

    SaveDataDelete delReq{};
    delReq.user_id = 0x10000000;
    delReq.dir_name = &validDir;
    Check(sceSaveDataDelete(&delReq) == SaveDataOk,
          "delete removed first valid save data");

    delReq.dir_name = &validDir2;
    Check(sceSaveDataDelete(&delReq) == SaveDataOk,
          "delete removed second valid save data");
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-traversal-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    Check(sceSaveDataAbort() == SaveDataErrorNotInitialized, "abort before init");
    Check(sceSaveDataClearProgress() == SaveDataErrorNotInitialized, "clearProgress before init");

    Check(sceSaveDataInitialize3(nullptr) == SaveDataOk, "SaveData initializes");

    CheckNullPointers();
    CheckTraversalAndInvalidSequences();
    CheckUnterminatedDirectoryName();
    CheckUnderscoreRejectionInMount2();
    CheckValidDirectoryNameLifecycle();

    Check(sceSaveDataTerminate() == SaveDataOk, "SaveData terminates");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);

    if (g_failures != 0) return 1;
    std::printf("savedata traversal tests passed\n");
    return 0;
}
