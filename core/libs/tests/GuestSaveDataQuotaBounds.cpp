#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorInvalidLoginUser = -2137063407;
constexpr int SaveDataErrorMemoryNotReady = -2137063406;

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;

constexpr std::uint64_t SaveDataBlocksMin2 = 96;
constexpr std::uint64_t SaveDataBlocksMax = 16384;
constexpr std::size_t SaveDataMemMaxSize = 32u * 1024u * 1024u;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedState() {
    SaveDataMountResult result{};
    SaveDataMount2 mountReq{};
    mountReq.user_id = 0x10000000;
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "uninitquota", sizeof("uninitquota"));
    mountReq.dir_name = &dirName;
    mountReq.mount_mode = MountReadWrite | MountCreate;
    mountReq.blocks = SaveDataBlocksMin2;
    Require(sceSaveDataMount2(&mountReq, &result) == SaveDataErrorNotInitialized);

    SaveDataMemorySetup2 memSetup{};
    memSetup.user_id = 0x10000000;
    memSetup.memory_size = 1024;
    SaveDataMemorySetupResult memResult{};
    Require(sceSaveDataSetupSaveDataMemory2(&memSetup, &memResult) == SaveDataErrorNotInitialized);

    std::uint32_t syncParam[2] = {0x10000000u, 0};
    Require(sceSaveDataSyncSaveDataMemory(syncParam) == SaveDataErrorNotInitialized);
}

void TestMountQuotaBounds() {
    SaveDataMountResult result{};
    Require(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter);

    SaveDataMount2 nullResultMount{};
    Require(sceSaveDataMount2(&nullResultMount, nullptr) == SaveDataErrorParameter);

    SaveDataMount2 zeroModeMount{};
    zeroModeMount.user_id = 0x10000000;
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "zeromode", sizeof("zeromode"));
    zeroModeMount.dir_name = &dirName;
    zeroModeMount.mount_mode = 0;
    zeroModeMount.blocks = SaveDataBlocksMin2;
    Require(sceSaveDataMount2(&zeroModeMount, &result) == SaveDataErrorParameter);

    zeroModeMount.mount_mode = MountReadWrite | MountCreate;
    zeroModeMount.user_id = 0;
    Require(sceSaveDataMount2(&zeroModeMount, &result) == SaveDataErrorInvalidLoginUser);

    zeroModeMount.user_id = -1;
    Require(sceSaveDataMount2(&zeroModeMount, &result) == SaveDataErrorInvalidLoginUser);

    const std::uint64_t underMinBlocks[] = {0, 1, 32, 48, 95};
    for (const auto b : underMinBlocks) {
        zeroModeMount.user_id = 0x10000000;
        zeroModeMount.blocks = b;
        Require(sceSaveDataMount2(&zeroModeMount, &result) == SaveDataErrorParameter);
    }

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "quotamin", sizeof("quotamin"));
    SaveDataMount2 validMount{};
    validMount.user_id = 0x10000000;
    validMount.dir_name = &validDir;
    validMount.mount_mode = MountReadWrite | MountCreate;
    validMount.blocks = SaveDataBlocksMin2;
    Require(sceSaveDataMount2(&validMount, &result) == SaveDataOk);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&result.mount_point, &info) == SaveDataOk);
    Require(info.blocks == SaveDataBlocksMin2);
    Require(info.free_blocks <= info.blocks);
    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataOk);

    SceSaveDataDirName maxDir{};
    std::memcpy(maxDir.data, "quotamax", sizeof("quotamax"));
    SaveDataMount2 maxMount{};
    maxMount.user_id = 0x10000000;
    maxMount.dir_name = &maxDir;
    maxMount.mount_mode = MountReadWrite | MountCreate;
    maxMount.blocks = SaveDataBlocksMax + 4096;
    Require(sceSaveDataMount2(&maxMount, &result) == SaveDataOk);

    Require(sceSaveDataGetMountInfo(&result.mount_point, &info) == SaveDataOk);
    Require(info.blocks == SaveDataBlocksMax);
    Require(info.free_blocks <= info.blocks);
    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataOk);
}

void TestMemoryQuotaBounds() {
    Require(sceSaveDataSetupSaveDataMemory2(nullptr, nullptr) == SaveDataErrorParameter);

    SaveDataMemorySetupResult memResult{};
    Require(sceSaveDataSetupSaveDataMemory2(nullptr, &memResult) == SaveDataErrorParameter);

    SaveDataMemorySetup2 zeroSize{};
    zeroSize.user_id = 0x10000000;
    zeroSize.memory_size = 0;
    Require(sceSaveDataSetupSaveDataMemory2(&zeroSize, &memResult) == SaveDataErrorParameter);

    SaveDataMemorySetup2 overLimit{};
    overLimit.user_id = 0x10000000;
    overLimit.memory_size = SaveDataMemMaxSize + 1;
    Require(sceSaveDataSetupSaveDataMemory2(&overLimit, &memResult) == SaveDataErrorParameter);

    overLimit.memory_size = SaveDataMemMaxSize * 2;
    Require(sceSaveDataSetupSaveDataMemory2(&overLimit, &memResult) == SaveDataErrorParameter);

    overLimit.memory_size = static_cast<std::size_t>(-1);
    Require(sceSaveDataSetupSaveDataMemory2(&overLimit, &memResult) == SaveDataErrorParameter);

    const std::size_t validSizes[] = {512, 4096, 65536, 1048576, SaveDataMemMaxSize};
    for (const auto sz : validSizes) {
        SaveDataMemorySetup2 setup{};
        setup.user_id = 0x10000001;
        setup.slot_id = 1;
        setup.memory_size = sz;
        Require(sceSaveDataSetupSaveDataMemory2(&setup, &memResult) == SaveDataOk);
    }

    char buffer[128]{};
    Require(sceSaveDataGetSaveDataMemory(0x10000001, nullptr, sizeof(buffer), 0) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(0x10000001, nullptr, sizeof(buffer), 0) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataMemory(0x19999999, buffer, sizeof(buffer), 0) == SaveDataErrorMemoryNotReady);

    Require(sceSaveDataSetSaveDataMemory(0x10000001, buffer, sizeof(buffer), 0) == SaveDataOk);
    Require(sceSaveDataGetSaveDataMemory(0x10000001, buffer, sizeof(buffer), 0) == SaveDataOk);

    Require(sceSaveDataSyncSaveDataMemory(nullptr) == SaveDataErrorParameter);

    std::uint32_t invalidSync[2] = {0x19999999u, 0};
    Require(sceSaveDataSyncSaveDataMemory(invalidSync) == SaveDataErrorMemoryNotReady);

    std::uint32_t validSync[2] = {0x10000001u, 1};
    Require(sceSaveDataSyncSaveDataMemory(validSync) == SaveDataOk);
}

void TestCloudQuotaExport() {
    bool threw = false;
    try {
        sceSaveDataGetCloudQuotaInfo();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestPostTerminationState() {
    SaveDataMountResult result{};
    SaveDataMount2 mountReq{};
    mountReq.user_id = 0x10000000;
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "postterm", sizeof("postterm"));
    mountReq.dir_name = &dirName;
    mountReq.mount_mode = MountReadWrite | MountCreate;
    mountReq.blocks = SaveDataBlocksMin2;
    Require(sceSaveDataMount2(&mountReq, &result) == SaveDataErrorNotInitialized);

    SaveDataMemorySetup2 memSetup{};
    memSetup.user_id = 0x10000000;
    memSetup.memory_size = 1024;
    SaveDataMemorySetupResult memResult{};
    Require(sceSaveDataSetupSaveDataMemory2(&memSetup, &memResult) == SaveDataErrorNotInitialized);

    std::uint32_t syncParam[2] = {0x10000000u, 0};
    Require(sceSaveDataSyncSaveDataMemory(syncParam) == SaveDataErrorNotInitialized);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-quota-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestUninitializedState();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestMountQuotaBounds();
    TestMemoryQuotaBounds();
    TestCloudQuotaExport();
    Require(sceSaveDataTerminate() == SaveDataOk);
    TestPostTerminationState();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
