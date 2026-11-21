#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <source_location>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;

constexpr std::uint32_t SortKeyDirName = 0;
constexpr std::uint32_t SortKeyUserParam = 1;
constexpr std::uint32_t SortKeyBlocks = 2;
constexpr std::uint32_t SortKeyMtime = 3;
constexpr std::uint32_t SortKeyFreeBlocks = 5;

constexpr std::uint32_t SortOrderAscending = 0;
constexpr std::uint32_t SortOrderDescending = 1;

constexpr std::uint32_t ParamTypeUserParam = 4;
constexpr std::uint32_t ParamTypeMtime = 5;

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest_savedata_dirname_search_bounds check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void TestPreInitializationAndNullBounds() {
    SaveDataDirNameSearchResult result{};
    Require(sceSaveDataDirNameSearch(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(nullptr, &result) == SaveDataErrorParameter);

    SaveDataDirNameSearchCond cond{};
    Require(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter);

    Require(sceSaveDataDirNameSearch2(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(nullptr, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, nullptr) == SaveDataErrorParameter);

    Require(sceSaveDataDirNameSearchPs4(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearchPs4(nullptr, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearchPs4(&cond, nullptr) == SaveDataErrorParameter);

    Require(sceSaveDataCreateTransactionResource(1024) == SaveDataErrorNotInitialized);
    Require(sceSaveDataAbort() == SaveDataErrorNotInitialized);
}

void TestSearchCriteriaValidation() {
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;

    SaveDataDirNameSearchResult result{};
    std::array<SceSaveDataDirName, 4> names{};
    result.dir_names = names.data();
    result.dir_names_num = static_cast<std::uint32_t>(names.size());

    for (const std::uint32_t invalidKey : {4u, 6u, 7u, 10u, 100u, 0xFFFFFFFFu}) {
        cond.key = invalidKey;
        cond.order = SortOrderAscending;
        Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
        Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);
    }

    for (const std::uint32_t invalidOrder : {2u, 3u, 5u, 10u, 100u, 0xFFFFFFFFu}) {
        cond.key = SortKeyDirName;
        cond.order = invalidOrder;
        Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
        Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);
    }

    cond.key = SortKeyDirName;
    cond.order = SortOrderAscending;

    SceSaveDataDirName unterminatedPattern{};
    std::memset(unterminatedPattern.data, 'a', sizeof(unterminatedPattern.data));
    cond.dir_name = &unterminatedPattern;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter);
    cond.dir_name = nullptr;

    SceSaveDataTitleId foreignTitle{};
    std::memcpy(foreignTitle.data, "CUSA99999", sizeof("CUSA99999"));
    cond.title_id = &foreignTitle;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 0u);
    Require(result.set_num == 0u);

    Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 0u);
    Require(result.set_num == 0u);

    Require(sceSaveDataDirNameSearchPs4(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 0u);
    Require(result.set_num == 0u);
    cond.title_id = nullptr;
}

void CreateSaveDirectoryWithParams(const char* name, std::uint32_t userParam, std::int64_t mtime) {
    SceSaveDataDirName dirName{};
    std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mounted{};
    Require(sceSaveDataMount3(&mount, &mounted) == SaveDataOk);
    Require(sceSaveDataSetParam(&mounted.mount_point, ParamTypeUserParam, &userParam, sizeof(userParam)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mounted.mount_point, ParamTypeMtime, &mtime, sizeof(mtime)) == SaveDataOk);
    Require(sceSaveDataUmount2(0, &mounted.mount_point) == SaveDataOk);
}

void TestDirectorySearchBoundsAndPatternMatching() {
    CreateSaveDirectoryWithParams("SEARCH_A", 300u, 2000);
    CreateSaveDirectoryWithParams("SEARCH_B", 100u, 1000);
    CreateSaveDirectoryWithParams("SEARCH_C", 200u, 3000);

    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.key = SortKeyDirName;
    cond.order = SortOrderAscending;

    SceSaveDataDirName patternAll{};
    std::snprintf(patternAll.data, sizeof(patternAll.data), "SEARCH_%%");
    cond.dir_name = &patternAll;

    std::array<SceSaveDataDirName, 3> names{};
    std::array<SaveDataParam, 3> params{};
    std::array<SaveDataSearchInfo, 3> infos{};

    SaveDataDirNameSearchResult result{};
    result.dir_names = names.data();
    result.params = params.data();
    result.infos = infos.data();
    result.dir_names_num = static_cast<std::uint32_t>(names.size());

    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);
    Require(std::strcmp(names[0].data, "SEARCH_A") == 0);
    Require(std::strcmp(names[1].data, "SEARCH_B") == 0);
    Require(std::strcmp(names[2].data, "SEARCH_C") == 0);

    cond.order = SortOrderDescending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);
    Require(std::strcmp(names[0].data, "SEARCH_C") == 0);
    Require(std::strcmp(names[1].data, "SEARCH_B") == 0);
    Require(std::strcmp(names[2].data, "SEARCH_A") == 0);

    cond.key = SortKeyUserParam;
    cond.order = SortOrderAscending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);
    Require(std::strcmp(names[0].data, "SEARCH_B") == 0);
    Require(params[0].user_param == 100u);
    Require(std::strcmp(names[1].data, "SEARCH_C") == 0);
    Require(params[1].user_param == 200u);
    Require(std::strcmp(names[2].data, "SEARCH_A") == 0);
    Require(params[2].user_param == 300u);

    cond.order = SortOrderDescending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);
    Require(std::strcmp(names[0].data, "SEARCH_A") == 0);
    Require(std::strcmp(names[1].data, "SEARCH_C") == 0);
    Require(std::strcmp(names[2].data, "SEARCH_B") == 0);

    cond.key = SortKeyMtime;
    cond.order = SortOrderAscending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);
    Require(std::strcmp(names[0].data, "SEARCH_B") == 0);
    Require(params[0].mtime == 1000);
    Require(std::strcmp(names[1].data, "SEARCH_A") == 0);
    Require(params[1].mtime == 2000);
    Require(std::strcmp(names[2].data, "SEARCH_C") == 0);
    Require(params[2].mtime == 3000);

    cond.order = SortOrderDescending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);
    Require(std::strcmp(names[0].data, "SEARCH_C") == 0);
    Require(std::strcmp(names[1].data, "SEARCH_A") == 0);
    Require(std::strcmp(names[2].data, "SEARCH_B") == 0);

    cond.key = SortKeyBlocks;
    cond.order = SortOrderAscending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);

    cond.key = SortKeyFreeBlocks;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);

    SceSaveDataDirName singlePattern{};
    std::snprintf(singlePattern.data, sizeof(singlePattern.data), "SEARCH_B");
    cond.dir_name = &singlePattern;
    cond.key = SortKeyDirName;
    cond.order = SortOrderAscending;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 1u);
    Require(result.set_num == 1u);
    Require(std::strcmp(names[0].data, "SEARCH_B") == 0);

    SceSaveDataDirName singleCharWildcard{};
    std::snprintf(singleCharWildcard.data, sizeof(singleCharWildcard.data), "SEARCH__");
    cond.dir_name = &singleCharWildcard;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 3u);

    SceSaveDataDirName noMatchPattern{};
    std::snprintf(noMatchPattern.data, sizeof(noMatchPattern.data), "ABSENT_%%");
    cond.dir_name = &noMatchPattern;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 0u);
    Require(result.set_num == 0u);

    cond.dir_name = &patternAll;
    result.dir_names_num = 1u;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 1u);
    Require(std::strcmp(names[0].data, "SEARCH_A") == 0);

    result.dir_names_num = 2u;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 2u);

    result.dir_names = nullptr;
    result.params = nullptr;
    result.infos = nullptr;
    result.dir_names_num = 0u;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 0u);

    Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 3u);
    Require(result.set_num == 0u);

    cond.dir_name = nullptr;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataOk);
    Require(result.hit_num >= 3u);
    Require(result.set_num == 0u);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-search-bounds-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}}}";
    }

    TestPreInitializationAndNullBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestSearchCriteriaValidation();
    TestDirectorySearchBoundsAndPatternMatching();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
