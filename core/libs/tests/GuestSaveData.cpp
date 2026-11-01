#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

struct SaveDataRestore {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint8_t reserved[32];
};

extern "C" {
int APS5_VABI sceSaveDataRestore(const SaveDataRestore* restore);
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
    Require(sceSaveDataRestore(nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);

    SaveDataRestore restore{};
    restore.user_id = 0x10000000;
    restore.dir_name = nullptr;
    Require(sceSaveDataRestore(&restore) == SCE_SAVE_DATA_ERROR_PARAMETER);

    for (const std::int32_t userId : {-100, -1, 0, 1, 0x10000000, 0x10000005}) {
        SaveDataRestore invalidUser{};
        invalidUser.user_id = userId;
        invalidUser.dir_name = nullptr;
        Require(sceSaveDataRestore(&invalidUser) == SCE_SAVE_DATA_ERROR_PARAMETER);
    }

    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    restore.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminated);
    Require(sceSaveDataRestore(&restore) == SCE_SAVE_DATA_ERROR_PARAMETER);

    for (const char fill : {'A', 'z', '0', '_', '%', 'x', '!'}) {
        SceSaveDataDirName unterminatedPattern{};
        std::memset(unterminatedPattern.data, fill, sizeof(unterminatedPattern.data));
        restore.dir_name = &unterminatedPattern;
        Require(sceSaveDataRestore(&restore) == SCE_SAVE_DATA_ERROR_PARAMETER);
    }

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    restore.dir_name = &emptyDir;
    Require(sceSaveDataRestore(&restore) == SCE_SAVE_DATA_ERROR_PARAMETER);

    for (const char* invalidName : {".", "..", "invalid/dir", "invalid\\dir", "invalid:dir"}) {
        SceSaveDataDirName invalidDir{};
        std::strncpy(invalidDir.data, invalidName, sizeof(invalidDir.data));
        restore.dir_name = &invalidDir;
        Require(sceSaveDataRestore(&restore) == SCE_SAVE_DATA_ERROR_PARAMETER);
    }

    return 0;
}
