#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceSaveDataMount(const SaveDataMountPs4* mount, SaveDataMountResult* mountResult);
int APS5_VABI sceSaveDataMountWithSlot(std::int32_t slotIndex, const SaveDataMountPs4* mount, SaveDataMountResult* mountResult);
}

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    SceSaveDataDirName dirName{};
    std::strncpy(dirName.data, "test_slot_save", sizeof(dirName.data));

    SaveDataMountPs4 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = 2;
    SaveDataMountResult result{};

    Require(sceSaveDataMount(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter);
    Require(sceSaveDataMount(&mount, nullptr) == SaveDataErrorParameter);

    for (const std::int32_t invalidSlot : {-1000, -100, -10, -2, -1, 16, 17, 32, 64, 100, 1000}) {
        Require(sceSaveDataMountWithSlot(invalidSlot, &mount, &result) == SaveDataErrorParameter);
        Require(sceSaveDataMountWithSlot(invalidSlot, nullptr, &result) == SaveDataErrorParameter);
        Require(sceSaveDataMountWithSlot(invalidSlot, &mount, nullptr) == SaveDataErrorParameter);
        Require(sceSaveDataMountWithSlot(invalidSlot, nullptr, nullptr) == SaveDataErrorParameter);
    }

    for (const int slotIndex : {-100, -10, -1, 16, 17, 32, 100}) {
        SaveDataMountPoint invalidPoint{};
        std::snprintf(invalidPoint.data, sizeof(invalidPoint.data), "/savedata%d", slotIndex);
        Require(sceSaveDataUmount(&invalidPoint) != 0);
    }

    return 0;
}
