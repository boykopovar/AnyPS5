#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
}

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata mount status check failed: %s\n", what.c_str());
    ++failures;
}

void MountAndCheck(const char* name, std::uint32_t mode, std::uint32_t expectedStatus, const std::string& what) {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, name, std::strlen(name) + 1);
    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = mode;
    SaveDataMountResult result{};
    const int code = sceSaveDataMount3(&mount, &result);
    Check(code == 0, what + ": mounts");
    if (code != 0) return;
    Check(result.mount_status == expectedStatus, what + ": status is " + std::to_string(result.mount_status) + ", expected " + std::to_string(expectedStatus));
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, what + ": unmounts");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount-status-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 1u, "CREATE2 of a missing save");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 0u, "CREATE2 of an existing save");
    MountAndCheck("made", MountReadWrite | MountCreate, 1u, "CREATE of a missing save");
    MountAndCheck("made", MountReadWrite, 0u, "open of an existing save");
    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata mount status tests passed\n");
    return 0;
}
