#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

struct SaveDataCheckBackupData {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    SaveDataParam* param;
    SaveDataIcon* icon;
    std::uint8_t reserved[32];
};

extern "C" {
int APS5_VABI sceSaveDataCheckBackupData(const SaveDataCheckBackupData* check);
}

namespace {

constexpr int SCE_SAVE_DATA_ERROR_PARAMETER = static_cast<int>(0x809f0000);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    Require(sceSaveDataCheckBackupData(nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);

    SaveDataCheckBackupData check{};
    check.user_id = 0x10000000;
    check.dir_name = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == SCE_SAVE_DATA_ERROR_PARAMETER);

    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    check.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminated);
    Require(sceSaveDataCheckBackupData(&check) == SCE_SAVE_DATA_ERROR_PARAMETER);

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    check.dir_name = &emptyDir;
    Require(sceSaveDataCheckBackupData(&check) == SCE_SAVE_DATA_ERROR_PARAMETER);

    for (const char* invalidName : {".", "..", "invalid/dir", "invalid\\dir", "invalid:dir"}) {
        SceSaveDataDirName invalidDir{};
        std::strncpy(invalidDir.data, invalidName, sizeof(invalidDir.data));
        check.dir_name = &invalidDir;
        Require(sceSaveDataCheckBackupData(&check) == SCE_SAVE_DATA_ERROR_PARAMETER);
    }

    return 0;
}
