#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

static_assert(offsetof(SaveDataMount2, dir_name) == 8 && offsetof(SaveDataMount2, blocks) == 16 && offsetof(SaveDataMount2, mount_mode) == 24 && sizeof(SaveDataMount2) == 64, "SaveDataMount2 has the PS4 layout");

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorInvalidLoginUser = -2137063407;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata ps4 check failed: %s\n", what.c_str());
    ++failures;
}

void TestTransferringMountPs4() {
    SceSaveDataTitleId title{};
    std::strcpy(title.data, "CUSA00001");
    SceSaveDataDirName dir{};
    std::strcpy(dir.data, "kept");
    SaveDataTransferringMount mount{};
    mount.user_id = 1;
    mount.title_id = &title;
    mount.dir_name = &dir;
    SaveDataMountResult result{};
    std::memset(&result, 0xAA, sizeof(result));
    Check(sceSaveDataTransferringMountPs4(&mount, &result) == SaveDataErrorNotFound, "TransferringMountPs4 returns NOT_FOUND");
    Check(result.mount_point.data[0] == '\0', "TransferringMountPs4 reports no mount point");
    std::memset(&result, 0xAA, sizeof(result));
    Check(sceSaveDataTransferringMount(&mount, &result) == SaveDataErrorNotFound, "TransferringMount returns NOT_FOUND");
    Check(result.mount_point.data[0] == '\0', "TransferringMount reports no mount point");
    Check(sceSaveDataTransferringMount(nullptr, nullptr) == SaveDataErrorNotFound, "TransferringMount handles null arguments");
    Check(sceSaveDataTransferringMountPs4(nullptr, nullptr) == SaveDataErrorNotFound, "TransferringMountPs4 handles null arguments");
}

void TestDirNameSearchPs4() {
    SceSaveDataDirName names[4]{};
    std::memset(names, 0x5A, sizeof(names));
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    SaveDataDirNameSearchResult result{};
    result.hit_num = 7;
    result.set_num = 7;
    result.dir_names = names;
    result.dir_names_num = 4;

    Check(sceSaveDataDirNameSearch(&cond, &result) == 0 && result.hit_num == 1 && result.set_num == 1, "the PS5 search finds the PS5 save");

    std::memset(names, 0x5A, sizeof(names));
    result.hit_num = 7;
    result.set_num = 7;
    Check(sceSaveDataDirNameSearchPs4(&cond, &result) == 0, "DirNameSearchPs4 succeeds");
    Check(result.hit_num == 0, "DirNameSearchPs4 writes zero hits");
    Check(result.set_num == 0, "DirNameSearchPs4 writes zero set entries");
    Check(static_cast<unsigned char>(names[0].data[0]) == 0x5A, "DirNameSearchPs4 leaves the name buffer untouched");

    Check(sceSaveDataDirNameSearchPs4(nullptr, &result) == SaveDataErrorParameter, "null cond returns PARAMETER in sceSaveDataDirNameSearchPs4");
    Check(sceSaveDataDirNameSearchPs4(&cond, nullptr) == SaveDataErrorParameter, "null result returns PARAMETER in sceSaveDataDirNameSearchPs4");
}

void TestMount2() {
    SceSaveDataDirName created{};
    std::strcpy(created.data, "mount2-new");
    SaveDataMount2 mount{};
    mount.user_id = 1;
    mount.dir_name = &created;
    mount.blocks = 96;
    mount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult result{};
    Check(sceSaveDataMount2(&mount, &result) == SaveDataErrorNotInitialized, "Mount2 before initialization is NOT_INITIALIZED");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Check(sceSaveDataMount2(&mount, &result) == 0, "Mount2 creates a missing save");
    Check(result.mount_status == 1u, "Mount2 reports the save as created");
    Check(result.mount_point.data[0] != '\0', "Mount2 reports a mount point");
    Check(std::filesystem::is_directory(std::filesystem::path("_sd") / "mount2-new"), "Mount2 creates the save directory");
    SaveDataMountResult busy{};
    Check(sceSaveDataMount2(&mount, &busy) == SaveDataErrorBusy, "Mount2 of a mounted save is BUSY");
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the Mount2 mount point unmounts");

    SceSaveDataDirName kept{};
    std::strcpy(kept.data, "kept");
    mount.dir_name = &kept;
    mount.mount_mode = MountReadWrite;
    Check(sceSaveDataMount2(&mount, &result) == 0, "Mount2 opens an existing save");
    Check(result.mount_status == 0u, "Mount2 reports the existing save as opened");
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the opened save unmounts");
    mount.blocks = 0;
    mount.mount_mode = MountReadWrite | MountCreate2;
    Check(sceSaveDataMount2(&mount, &result) == 0, "Mount2 CREATE2 with 0 blocks opens an existing save");
    Check(result.mount_status == 0u, "Mount2 CREATE2 reports the existing save as opened");
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the CREATE2 mount point unmounts");
    mount.blocks = 96;
    mount.mount_mode = MountReadWrite;

    SceSaveDataDirName absent{};
    std::strcpy(absent.data, "absent");
    mount.dir_name = &absent;
    Check(sceSaveDataMount2(&mount, &result) == SaveDataErrorNotFound, "Mount2 open of a missing save is NOT_FOUND");
    Check(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter, "Mount2 with a null mount is PARAMETER");
    Check(sceSaveDataMount2(&mount, nullptr) == SaveDataErrorParameter, "Mount2 with a null result is PARAMETER");

    SceSaveDataDirName small{};
    std::strcpy(small.data, "mount2-small");
    mount.dir_name = &small;
    mount.mount_mode = 0;
    Check(sceSaveDataMount2(&mount, &result) == SaveDataErrorParameter, "Mount2 with mount mode 0 is PARAMETER");
    mount.blocks = 1;
    mount.mount_mode = MountReadWrite | MountCreate;
    Check(sceSaveDataMount2(&mount, &result) == SaveDataErrorParameter, "Mount2 creating a save of fewer than 96 blocks is PARAMETER");
    Check(!std::filesystem::exists(std::filesystem::path("_sd") / "mount2-small"), "a refused Mount2 creates no directory");

    mount.blocks = 96;
    for (const char* name : {"survey_test", "SURVEY_TEST"}) {
        SceSaveDataDirName underscored{};
        std::strcpy(underscored.data, name);
        mount.dir_name = &underscored;
        Check(sceSaveDataMount2(&mount, &result) == SaveDataErrorParameter, std::string("Mount2 of ") + name + " is PARAMETER");
        Check(!std::filesystem::exists(std::filesystem::path("_sd") / name), std::string("a refused Mount2 of ") + name + " creates no directory");
    }
    SceSaveDataDirName mixed{};
    std::strcpy(mixed.data, "surveyTestAuto0");
    mount.dir_name = &mixed;
    Check(sceSaveDataMount2(&mount, &result) == 0, "Mount2 creates a mixed-case save");
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the mixed-case save unmounts");
    for (const int user : {-1, 0}) {
        mount.user_id = user;
        Check(sceSaveDataMount2(&mount, &result) == SaveDataErrorInvalidLoginUser, "Mount2 of user " + std::to_string(user) + " is INVALID_LOGIN_USER");
    }
}

void TestSaveDataMemoryV1() {
    Check(sceSaveDataSetupSaveDataMemory(7, 0x1000, nullptr) == SaveDataErrorNotInitialized, "v1 SetupSaveDataMemory before v1 Initialize");
    Check(sceSaveDataInitialize(nullptr) == 0, "v1 Initialize");
    Check(sceSaveDataSetupSaveDataMemory(7, 0x1000, nullptr) == 0, "v1 SetupSaveDataMemory");
    char written[16] = "settings-v1";
    Check(sceSaveDataSetSaveDataMemory(7, written, sizeof(written), 0x100) == 0, "v1 SetSaveDataMemory at an offset");
    char read[16] = {};
    Check(sceSaveDataGetSaveDataMemory(7, read, sizeof(read), 0x100) == 0, "v1 GetSaveDataMemory");
    Check(std::memcmp(read, written, sizeof(read)) == 0, "v1 GetSaveDataMemory returns the bytes v1 set");
    std::ifstream slot("_sd_mem/u7/slot0.bin", std::ios::binary);
    slot.seekg(0x100);
    char stored[16] = {};
    Check(static_cast<bool>(slot.read(stored, sizeof(stored))) && std::memcmp(stored, written, sizeof(stored)) == 0, "v1 SetSaveDataMemory stores into slot 0 of the user");
    Check(sceSaveDataTerminate() == 0, "Terminate after v1 Initialize");
}

void TestMountPs4() {
    SaveDataMountResult result{};
    Check(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter, "null mount is PARAMETER");
    SaveDataMountPs4 mount{};
    Check(sceSaveDataMount(&mount, &result) == SaveDataErrorParameter, "null dir_name is PARAMETER");
    Check(sceSaveDataUmount(nullptr) == SaveDataErrorParameter, "umount null point is PARAMETER");
    SceSaveDataDirName dir{};
    std::strcpy(dir.data, "kept");
    mount.dir_name = &dir;
    mount.mount_mode = MountReadWrite;
    Check(sceSaveDataMount(&mount, nullptr) == SaveDataErrorParameter, "null mount_result is PARAMETER");
    Check(sceSaveDataMount(&mount, &result) == 0, "mount existing save directory succeeds");
    Check(result.mount_point.data[0] != '\0', "mount writes mount point");
    Check(sceSaveDataUmount(&result.mount_point) == 0, "umount mounted point succeeds");
    Check(sceSaveDataUmount(&result.mount_point) == SaveDataErrorNotMounted, "umount unmounted point is NOT_MOUNTED");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-ps4-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "_sd" / "kept");
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    TestTransferringMountPs4();
    TestDirNameSearchPs4();
    TestSaveDataMemoryV1();
    TestMountPs4();
    TestMount2();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata ps4 tests passed\n");
    return 0;
}
