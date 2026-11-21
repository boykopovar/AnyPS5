#include "SceTypes.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
void RequireThrows(F&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) {
        std::abort();
    }
}

} // namespace

int main() {
    RequireThrows([] {
        sceSaveDataGetSaveDataRootDir();
    });
    RequireThrows([] {
        sceSaveDataGetSaveDataRootPath();
    });
    RequireThrows([] {
        sceSaveDataGetSaveDataRootUsbPath();
    });

    Require(sceSaveDataGetMountInfo(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount3(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(nullptr, nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint invalidPoint{};
    std::memset(invalidPoint.data, 'X', sizeof(invalidPoint.data));
    Require(sceSaveDataUmount2(0, &invalidPoint) == SaveDataErrorParameter);

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(nullptr, &mountResult) == SaveDataErrorParameter);

    SaveDataMount3 mount{};
    Require(sceSaveDataMount3(&mount, nullptr) == SaveDataErrorParameter);

    SaveDataDirNameSearchResult searchResult{};
    Require(sceSaveDataDirNameSearch(nullptr, &searchResult) == SaveDataErrorParameter);

    SaveDataDirNameSearchCond searchCond{};
    Require(sceSaveDataDirNameSearch(&searchCond, nullptr) == SaveDataErrorParameter);

    return 0;
}
