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
int APS5_VABI sceSaveDataGetMountPointStatus(const SaveDataMountPoint* mountPoint, int32_t* status);
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
    std::snprintf(point.data, sizeof(point.data), "/savedata0");
    int32_t status = 0;

    Require(sceSaveDataGetMountPointStatus(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountPointStatus(&point, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountPointStatus(nullptr, &status) == SaveDataErrorParameter);

    for (const char* invalidName : {"", ".", "..", "invalid/path", "invalid\\path", "invalid:path"}) {
        SaveDataMountPoint invalidPoint{};
        std::strncpy(invalidPoint.data, invalidName, sizeof(invalidPoint.data));
        Require(sceSaveDataGetMountPointStatus(&invalidPoint, nullptr) == SaveDataErrorParameter);
    }

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'a', sizeof(unterminatedPoint.data));
    Require(sceSaveDataGetMountPointStatus(&unterminatedPoint, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountPointStatus(&unterminatedPoint, &status) == SaveDataErrorParameter);

    return 0;
}
