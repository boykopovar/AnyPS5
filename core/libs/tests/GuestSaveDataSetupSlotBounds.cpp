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
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorMountFull = -2137063412;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSetupMemoryBounds() {
    SaveDataMemorySetup2 setup{};
    setup.memory_size = 1024;
    SaveDataMemorySetupResult result{};

    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == SaveDataErrorNotInitialized);

    Require(sceSaveDataInitialize3(nullptr) == 0);

    Require(sceSaveDataSetupSaveDataMemory2(nullptr, &result) == SaveDataErrorParameter);

    setup.memory_size = 0;
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == SaveDataErrorParameter);

    setup.memory_size = (32u * 1024u * 1024u) + 1u;
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == SaveDataErrorParameter);

    setup.user_id = 1;
    setup.slot_id = 0;
    setup.memory_size = 4096;
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
    Require(result.existed_memory_size == 0);
}

void TestPathTraversalDefense() {
    SaveDataMountResult mountResult{};

    for (const char* invalid : {"../escape", "..\\escape", ".", "..", "bad/dir", "bad\\dir", "bad:name", ""}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        SaveDataMount3 invalidMount{};
        invalidMount.dir_name = &invalidDir;
        invalidMount.mount_mode = MountReadWrite | MountCreate2;
        Require(sceSaveDataMount3(&invalidMount, &mountResult) == SaveDataErrorParameter);
    }

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'x', sizeof(unterminatedDir.data));
    SaveDataMount3 unterminatedMount{};
    unterminatedMount.dir_name = &unterminatedDir;
    unterminatedMount.mount_mode = MountReadWrite | MountCreate2;
    Require(sceSaveDataMount3(&unterminatedMount, &mountResult) == SaveDataErrorParameter);
}

void TestSlotCapacityLimits() {
    std::array<SceSaveDataDirName, 16> dirNames{};
    std::array<SaveDataMountResult, 16> results{};

    for (std::size_t i = 0; i < 16; ++i) {
        std::snprintf(dirNames[i].data, sizeof(dirNames[i].data), "slot_%zu", i);
        SaveDataMount3 mount{};
        mount.dir_name = &dirNames[i];
        mount.mount_mode = MountReadWrite | MountCreate2;
        Require(sceSaveDataMount3(&mount, &results[i]) == 0);
        Require(results[i].mount_status == 1u);
    }

    SceSaveDataDirName overflowDir{};
    std::memcpy(overflowDir.data, "slot_16", sizeof("slot_16"));
    SaveDataMount3 overflowMount{};
    overflowMount.dir_name = &overflowDir;
    overflowMount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult overflowResult{};
    Require(sceSaveDataMount3(&overflowMount, &overflowResult) == SaveDataErrorMountFull);

    Require(sceSaveDataUmount2(0, &results[0].mount_point) == 0);

    Require(sceSaveDataMount3(&overflowMount, &overflowResult) == 0);
    Require(overflowResult.mount_status == 1u);

    Require(sceSaveDataUmount2(0, &overflowResult.mount_point) == 0);
    for (std::size_t i = 1; i < 16; ++i) {
        Require(sceSaveDataUmount2(0, &results[i].mount_point) == 0);
    }
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-setup-slot-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestSetupMemoryBounds();
    TestPathTraversalDefense();
    TestSlotCapacityLimits();

    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
