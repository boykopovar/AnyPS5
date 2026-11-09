#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;
constexpr int SaveDataErrorParameter = -2137063424;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata mount point alphanumeric check failed: %s\n", what.c_str());
    ++failures;
}

} // namespace

int main() {
    try {
        sceSaveDataInitialize(nullptr);
    } catch (...) {
    }

    const char* invalidNames[] = {
        "save_data",
        "save.data",
        "save/data",
        "save\\data",
        "save:data",
        "save data",
        "save@data",
        "SAVE_DATA",
        "_savedata",
        "savedata_"
    };

    for (const char* name : invalidNames) {
        SaveDataMount3 mount{};
        mount.mount_mode = MountReadWrite | MountCreate2;
        SceSaveDataDirName dirName{};
        std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);
        mount.dir_name = &dirName;
        SaveDataMountResult result{};
        int rc = sceSaveDataMount3(&mount, &result);
        Check(rc == SaveDataErrorParameter, std::string("mount with non-alphanumeric/special char dir_name '") + name + "' must return PARAMETER");
    }

    const char* validNames[] = {
        "savedata01",
        "SAVE123DATA",
        "a",
        "1234567890abcdefghijklmnopqrstuvwxyz"
    };

    for (const char* name : validNames) {
        SaveDataMount3 mount{};
        mount.mount_mode = MountReadWrite | MountCreate2;
        SceSaveDataDirName dirName{};
        std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);
        mount.dir_name = &dirName;
        SaveDataMountResult result{};
        int rc = sceSaveDataMount3(&mount, &result);
        if (rc == 0) {
            sceSaveDataUmount2(0, &result.mount_point);
        }
    }

    if (failures > 0) {
        std::fprintf(stderr, "savedata mount point alphanumeric tests failed with %d errors\n", failures);
        return 1;
    }

    std::printf("savedata mount point alphanumeric tests passed\n");
    return 0;
}
