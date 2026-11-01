#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

extern "C" {
int APS5_VABI sceSaveDataGetMountPoint(const SceSaveDataDirName* dirName, SaveDataMountPoint* mountPoint);
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
    SaveDataMountPoint point{};
    SceSaveDataDirName validDir{};
    std::snprintf(validDir.data, sizeof(validDir.data), "savedata0");

    Require(sceSaveDataGetMountPoint(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountPoint(&validDir, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountPoint(nullptr, &point) == SaveDataErrorParameter);

    for (const char* invalidName : {"", ".", "..", "dir/sub", "dir\\sub", "dir:name"}) {
        SceSaveDataDirName invalidDir{};
        std::strncpy(invalidDir.data, invalidName, sizeof(invalidDir.data));
        Require(sceSaveDataGetMountPoint(&invalidDir, &point) == SaveDataErrorParameter);
        Require(sceSaveDataGetMountPoint(&invalidDir, nullptr) == SaveDataErrorParameter);
    }

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'a', sizeof(unterminatedDir.data));
    Require(sceSaveDataGetMountPoint(&unterminatedDir, &point) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountPoint(&unterminatedDir, nullptr) == SaveDataErrorParameter);

    return 0;
}
