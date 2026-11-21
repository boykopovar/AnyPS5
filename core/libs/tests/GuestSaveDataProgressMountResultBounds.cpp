#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>

static_assert(offsetof(SaveDataMountResult, mount_point) == 0);
static_assert(offsetof(SaveDataMountResult, required_blocks) == 16);
static_assert(offsetof(SaveDataMountResult, unused) == 24);
static_assert(offsetof(SaveDataMountResult, mount_status) == 28);
static_assert(offsetof(SaveDataMountResult, reserved) == 32);
static_assert(sizeof(SaveDataMountResult) == 64);

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestProgressPollingBounds() {
    float prog = 0.0f;
    Require(sceSaveDataGetProgress(&prog) == SaveDataErrorNotInitialized);
    Require(sceSaveDataCancel(0) == SaveDataErrorNotInitialized);

    Require(sceSaveDataInitialize3(nullptr) == 0);

    Require(sceSaveDataGetProgress(nullptr) == SaveDataErrorParameter);
    prog = -1.0f;
    Require(sceSaveDataGetProgress(&prog) == 0);
    Require(prog == 0.0f);

    Require(sceSaveDataCancel(0) == 0);
    Require(sceSaveDataCancel(1) == 0);
}

void TestCancelledMountStatusRecovery() {
    SaveDataMountResult transferResult{};
    std::memset(&transferResult, 0xAB, sizeof(transferResult));
    Require(sceSaveDataTransferringMount(nullptr, &transferResult) == SaveDataErrorNotFound);
    Require(transferResult.required_blocks == 0);
    Require(transferResult.mount_status == 0);
    Require(transferResult.mount_point.data[0] == '\0');

    SaveDataTransferringMount transferMount{};
    Require(sceSaveDataTransferringMount(&transferMount, &transferResult) == SaveDataErrorNotFound);
    Require(transferResult.required_blocks == 0);
    Require(transferResult.mount_status == 0);
    Require(transferResult.mount_point.data[0] == '\0');

    SceSaveDataDirName invalidDir{};
    std::snprintf(invalidDir.data, sizeof(invalidDir.data), "../escape");
    SaveDataMount3 badMount{};
    badMount.dir_name = &invalidDir;
    badMount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult badResult{};
    Require(sceSaveDataMount3(&badMount, &badResult) == SaveDataErrorParameter);

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "valid_save", sizeof("valid_save"));
    SaveDataMount3 validMount{};
    validMount.dir_name = &validDir;
    validMount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult validResult{};
    Require(sceSaveDataMount3(&validMount, &validResult) == 0);
    Require(validResult.mount_status == 1u);
    Require(std::string_view(validResult.mount_point.data).starts_with("/savedata"));

    Require(sceSaveDataUmount2(0, &validResult.mount_point) == 0);
}

}

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-progress-mountresult-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestProgressPollingBounds();
    TestCancelledMountStatusRecovery();

    float prog = 0.0f;
    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    Require(sceSaveDataGetProgress(&prog) == SaveDataErrorNotInitialized);
    Require(sceSaveDataCancel(0) == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
