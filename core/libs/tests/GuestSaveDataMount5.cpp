#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr int SaveDataErrorParameter = -2137063424;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata mount5 check failed: %s\n", what.c_str());
    ++failures;
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount5-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    SaveDataMount3 request{};
    SaveDataMountResult result{};
    SceSaveDataDirName name{};
    std::memcpy(name.data, "mount5-save", sizeof("mount5-save"));
    request.user_id = 1;
    request.dir_name = &name;
    request.mount_mode = MountReadWrite | MountCreate;
    request.blocks = 8;

    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    Check(sceSaveDataMount5(nullptr, &result) == SaveDataErrorParameter, "a null mount is PARAMETER");
    Check(sceSaveDataMount5(&request, nullptr) == SaveDataErrorParameter, "a null result is PARAMETER");
    Check(sceSaveDataMount5(&request, &result) == 0, "Mount5 creates the save");
    Check(result.mount_point.data[0] != '\0', "Mount5 writes a mount point");
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the created mount unmounts");

    request.mount_mode = MountReadWrite;
    SaveDataMountResult reopened{};
    Check(sceSaveDataMount5(&request, &reopened) == 0, "Mount5 opens the existing save");
    Check(std::memcmp(reopened.mount_point.data, result.mount_point.data, sizeof(reopened.mount_point.data)) == 0, "Mount5 reuses the mount point");
    Check(sceSaveDataUmount2(0, &reopened.mount_point) == 0, "the reopened mount unmounts");
    Check(sceSaveDataTerminate() == 0, "SaveData terminates");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return failures == 0 ? 0 : 1;
}
