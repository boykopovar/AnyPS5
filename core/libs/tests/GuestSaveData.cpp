#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    SaveDataDirNameSearchResult result{};
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;

    Require(sceSaveDataDirNameSearch(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(nullptr, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter);

    cond.dir_name = nullptr;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter);

    SceSaveDataDirName oversizedDir{};
    std::memset(oversizedDir.data, 'a', sizeof(oversizedDir.data));
    cond.dir_name = &oversizedDir;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter);

    for (const char fill : {'A', 'z', '0', '_', '%'}) {
        SceSaveDataDirName invalidDir{};
        std::memset(invalidDir.data, fill, sizeof(invalidDir.data));
        cond.dir_name = &invalidDir;
        Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
    }

    return 0;
}
