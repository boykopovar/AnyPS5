#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "guest savedata search param check failed: %s\n", what);
    ++failures;
}

} // namespace

int main() {
    SaveDataDirNameSearchCond cond{};
    SaveDataDirNameSearchResult result{};
    Check(sceSaveDataDirNameSearch(nullptr, &result) == SaveDataErrorParameter, "null cond returns parameter error");
    Check(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter, "null result returns parameter error");
    Check(sceSaveDataDirNameSearchPs4(nullptr, &result) == SaveDataErrorParameter, "ps4 null cond returns parameter error");
    Check(sceSaveDataDirNameSearchPs4(&cond, nullptr) == SaveDataErrorParameter, "ps4 null result returns parameter error");
    Check(sceSaveDataDirNameSearch2(nullptr, &result) == SaveDataErrorParameter, "search2 null cond returns parameter error");
    Check(sceSaveDataDirNameSearch2(&cond, nullptr) == SaveDataErrorParameter, "search2 null result returns parameter error");
    return failures == 0 ? 0 : 1;
}
