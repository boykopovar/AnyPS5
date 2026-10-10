#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "GuestSaveDataFixture.hpp"

#include <Testing/Test.hpp>

#include <cstring>
#include <string>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;

void MountAndCheck(const char* name, std::uint32_t mode, std::uint32_t expectedStatus, const std::string& what) {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, name, std::strlen(name) + 1);
    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = mode;
    SaveDataMountResult result{};
    RequireEqual(sceSaveDataMount3(&mount, &result), 0, what + ": mounts");
    const auto status = result.mount_status;
    RequireEqual(sceSaveDataUmount2(0, &result.mount_point), 0, what + ": unmounts");
    RequireEqual(status, expectedStatus, what + ": status");
}

void Initialize() {
    RequireEqual(sceSaveDataInitialize3(nullptr), 0, "SaveData initializes");
}

const Case create2Missing{"Mount3_Create2OfMissingSave_ReportsCreated", [] {
    const SaveDataWorkingDirectory directory;
    Initialize();
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 1u, "CREATE2 of a missing save");
}};

const Case create2Existing{"Mount3_Create2OfExistingSave_ReportsExisting", [] {
    const SaveDataWorkingDirectory directory;
    Initialize();
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 1u, "CREATE2 of a missing save");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 0u, "CREATE2 of an existing save");
}};

const Case createMissing{"Mount3_CreateOfMissingSave_ReportsCreated", [] {
    const SaveDataWorkingDirectory directory;
    Initialize();
    MountAndCheck("made", MountReadWrite | MountCreate, 1u, "CREATE of a missing save");
}};

const Case openExisting{"Mount3_OpenOfExistingSave_ReportsExisting", [] {
    const SaveDataWorkingDirectory directory;
    Initialize();
    MountAndCheck("made", MountReadWrite | MountCreate, 1u, "CREATE of a missing save");
    MountAndCheck("made", MountReadWrite, 0u, "open of an existing save");
}};

} // namespace
