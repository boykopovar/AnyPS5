#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

constexpr int SaveDataErrorNotFound = -2137063416;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr std::uint32_t MountReadWrite = 2;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata ps4 check failed: %s\n", what.c_str());
    ++failures;
}

template <typename TFunction>
bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
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

    Check(ThrowsRuntimeError([&] { sceSaveDataDirNameSearchPs4(nullptr, &result); }), "null cond throws");
    Check(ThrowsRuntimeError([&] { sceSaveDataDirNameSearchPs4(&cond, nullptr); }), "null result throws");
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

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata ps4 tests passed\n");
    return 0;
}
