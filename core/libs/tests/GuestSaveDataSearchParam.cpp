#include "prx/libc/include/Api.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    SaveDataDirNameSearchCond cond{};
    SaveDataDirNameSearchResult result{};

    SceSaveDataDirName dirNames[4]{};
    result.dir_names = dirNames;
    result.dir_names_num = 0;
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0);
    Require(result.set_num == 0);

    result.dir_names_num = 1;
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0);
    Require(result.set_num <= 1);
    Require(result.hit_num >= result.set_num);

    result.dir_names = reinterpret_cast<SceSaveDataDirName*>(0x10);
    result.dir_names_num = 4;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearchPs4(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);

    result.dir_names = dirNames;
    result.dir_names_num = 4;
    result.params = reinterpret_cast<SaveDataParam*>(0x10);
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter || sceSaveDataDirNameSearch(&cond, &result) == 0);
    result.params = nullptr;

    result.infos = reinterpret_cast<SaveDataSearchInfo*>(0x10);
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter || sceSaveDataDirNameSearch(&cond, &result) == 0);
    result.infos = nullptr;

    const std::uint32_t invalidOrders[] = {2, 3, 10, static_cast<std::uint32_t>(-1)};
    for (const auto order : invalidOrders) {
        cond.order = order;
        Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
        Require(sceSaveDataDirNameSearchPs4(&cond, &result) == SaveDataErrorParameter);
        Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);
    }
    cond.order = 0;

    const std::uint32_t invalidKeys[] = {5, 6, 7, 100, static_cast<std::uint32_t>(-1)};
    for (const auto key : invalidKeys) {
        cond.key = key;
        Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
        Require(sceSaveDataDirNameSearchPs4(&cond, &result) == SaveDataErrorParameter);
        Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);
    }
    cond.key = 0;

    SceSaveDataDirName unterminatedName{};
    std::memset(unterminatedName.data, 'X', sizeof(unterminatedName.data));
    cond.dir_name = &unterminatedName;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearchPs4(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);

    return 0;
}
