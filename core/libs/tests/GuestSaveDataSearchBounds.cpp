#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

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

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function function, std::string_view expectedName) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(expectedName) != std::string_view::npos);
        Require(std::string_view(error.what()).find("not implemented") != std::string_view::npos);
        return;
    }
    std::abort();
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

    Require(sceSaveDataDirNameSearch(reinterpret_cast<const SaveDataDirNameSearchCond*>(0x10), &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch(&cond, reinterpret_cast<SaveDataDirNameSearchResult*>(0x10)) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(reinterpret_cast<const SaveDataDirNameSearchCond*>(0x10), &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, reinterpret_cast<SaveDataDirNameSearchResult*>(0x10)) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearchPs4(reinterpret_cast<const SaveDataDirNameSearchCond*>(0x10), &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearchPs4(&cond, reinterpret_cast<SaveDataDirNameSearchResult*>(0x10)) == SaveDataErrorParameter);
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

    SaveDataDirNameSearchCond unmappedNameCond = cond;
    unmappedNameCond.dir_name = reinterpret_cast<const SceSaveDataDirName*>(0x10);
    Require(sceSaveDataDirNameSearch(&unmappedNameCond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&unmappedNameCond, &result) == SaveDataErrorParameter);

    SaveDataDirNameSearchCond unmappedTitleCond = cond;
    unmappedTitleCond.title_id = reinterpret_cast<const SceSaveDataTitleId*>(0x10);
    Require(sceSaveDataDirNameSearch(&unmappedTitleCond, &result) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&unmappedTitleCond, &result) == SaveDataErrorParameter);

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
    cond.title_id = nullptr;

    Require(sceSaveDataDirNameSearchPs4(&cond, &result) == SaveDataOk);
    Require(result.hit_num == 0u);
    Require(result.set_num == 0u);
}

void CreateSaveDirectoryWithParams(const char* name, std::uint32_t userParam, std::int64_t mtime) {
    std::filesystem::create_directories(std::filesystem::path("_sd") / name);

    SceSaveDataDirName dirName{};
    std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr;

    SaveDataMountResult mounted{};
    Require(sceSaveDataMount3(&mount, &mounted) == SaveDataOk);
    Require(sceSaveDataSetParam(&mounted.mount_point, ParamTypeUserParam, &userParam, sizeof(userParam)) == SaveDataOk);
    Require(sceSaveDataSetParam(&mounted.mount_point, ParamTypeMtime, &mtime, sizeof(mtime)) == SaveDataOk);
    Require(sceSaveDataUmount2(0, &mounted.mount_point) == SaveDataOk);
}

void TestDirectorySearchBoundsAndPatternMatching() {
    CreateSaveDirectoryWithParams("SEARCH_B", 100u, 1000);
    CreateSaveDirectoryWithParams("SEARCH_A", 300u, 2000);
    CreateSaveDirectoryWithParams("SEARCH_C", 200u, 3000);

    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;

    SceSaveDataDirName patternAll{};
    std::snprintf(patternAll.data, sizeof(patternAll.data), "SEARCH_%%");
    cond.dir_name = &patternAll;

    std::array<SceSaveDataDirName, 4> names{};
    std::array<SaveDataParam, 4> params{};
    std::array<SaveDataSearchInfo, 4> infos{};

    SaveDataDirNameSearchResult result{};
    result.dir_names = names.data();
    result.dir_names_num = 3u;
    result.params = params.data();
    result.infos = infos.data();

    cond.key = SortKeyDirName;
    cond.order = SortOrderAscending;
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
    Require(params[0].user_param == 300u);
    Require(std::strcmp(names[1].data, "SEARCH_C") == 0);
    Require(params[1].user_param == 200u);
    Require(std::strcmp(names[2].data, "SEARCH_B") == 0);
    Require(params[2].user_param == 100u);

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
    Require(params[0].mtime == 3000);
    Require(std::strcmp(names[1].data, "SEARCH_A") == 0);
    Require(params[1].mtime == 2000);
    Require(std::strcmp(names[2].data, "SEARCH_B") == 0);
    Require(params[2].mtime == 1000);

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

    SaveDataDirNameSearchResult unmappedNamesResult = result;
    unmappedNamesResult.dir_names_num = 1u;
    unmappedNamesResult.dir_names = reinterpret_cast<SceSaveDataDirName*>(0x10);
    Require(sceSaveDataDirNameSearch(&cond, &unmappedNamesResult) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, &unmappedNamesResult) == SaveDataErrorParameter);

    SaveDataDirNameSearchResult unmappedParamsResult = result;
    unmappedParamsResult.dir_names_num = 1u;
    unmappedParamsResult.dir_names = names.data();
    unmappedParamsResult.params = reinterpret_cast<SaveDataParam*>(0x10);
    Require(sceSaveDataDirNameSearch(&cond, &unmappedParamsResult) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, &unmappedParamsResult) == SaveDataErrorParameter);

    SaveDataDirNameSearchResult unmappedInfosResult = result;
    unmappedInfosResult.dir_names_num = 1u;
    unmappedInfosResult.dir_names = names.data();
    unmappedInfosResult.params = nullptr;
    unmappedInfosResult.infos = reinterpret_cast<SaveDataSearchInfo*>(0x10);
    Require(sceSaveDataDirNameSearch(&cond, &unmappedInfosResult) == SaveDataErrorParameter);
    Require(sceSaveDataDirNameSearch2(&cond, &unmappedInfosResult) == SaveDataErrorParameter);
}

void TestUnimplementedInternalSearch() {
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataDirNameSearchInternal()); }, "sceSaveDataDirNameSearchInternal");
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-search-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestPreInitializationAndNullBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestSearchCriteriaValidation();
    TestDirectorySearchBoundsAndPatternMatching();
    TestUnimplementedInternalSearch();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
