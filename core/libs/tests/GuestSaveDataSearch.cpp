#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);
int APS5_VABI sceSaveDataDirNameSearch2(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
int APS5_VABI sceSaveDataSetParam(const SaveDataMountPoint*, std::uint32_t, const void*, std::size_t);
int APS5_VABI sceSaveDataGetMountInfo(const SaveDataMountPoint*, SaveDataMountInfo*);
}

static constexpr int SaveDataErrorParameter = -2137063424;

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}


static std::vector<std::string> Search(std::uint32_t key, std::uint32_t order, std::uint32_t capacity, std::uint32_t* hits) {
    SceSaveDataDirName pattern{};
    std::snprintf(pattern.data, sizeof(pattern.data), "SORT_%%");
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.dir_name = &pattern;
    cond.key = key;
    cond.order = order;
    std::array<SceSaveDataDirName, 3> names{};
    SaveDataDirNameSearchResult result{};
    result.dir_names = names.data();
    result.dir_names_num = capacity;
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0, "sorted search failed");
    *hits = result.hit_num;
    std::vector<std::string> found;
    for (std::uint32_t i = 0; i < result.set_num; ++i) found.emplace_back(names[i].data);
    return found;
}

static void SortByKey() {
    struct Save {
        const char* name;
        std::uint32_t userParam;
        std::int64_t mtime;
    };
    const std::array<Save, 3> saves{{{"SORT_B", 1, 1000}, {"SORT_A", 3, 2000}, {"SORT_C", 2, 3000}}};
    for (const auto& save : saves) {
        std::filesystem::create_directories(std::filesystem::path("_sd") / save.name);
        SceSaveDataDirName dirName{};
        std::snprintf(dirName.data, sizeof(dirName.data), "%s", save.name);
        SaveDataMount3 mount{};
        mount.user_id = 1;
        mount.dir_name = &dirName;
        mount.mount_mode = 2;
        SaveDataMountResult mounted{};
        Require(sceSaveDataMount3(&mount, &mounted) == 0, "mount for parameters failed");
        Require(sceSaveDataSetParam(&mounted.mount_point, 4, &save.userParam, sizeof(save.userParam)) == 0, "user parameter was not set");
        Require(sceSaveDataSetParam(&mounted.mount_point, 5, &save.mtime, sizeof(save.mtime)) == 0, "mtime was not set");
        Require(sceSaveDataUmount2(0, &mounted.mount_point) == 0, "unmount failed");
    }
    using Names = std::vector<std::string>;
    std::uint32_t hits = 0;
    Require(Search(0, 0, 3, &hits) == Names{"SORT_A", "SORT_B", "SORT_C"} && hits == 3, "directory names must sort ascending");
    Require(Search(0, 1, 3, &hits) == Names{"SORT_C", "SORT_B", "SORT_A"}, "directory names must sort descending");
    Require(Search(1, 0, 3, &hits) == Names{"SORT_B", "SORT_C", "SORT_A"}, "user parameters must sort ascending");
    Require(Search(1, 1, 3, &hits) == Names{"SORT_A", "SORT_C", "SORT_B"}, "user parameters must sort descending");
    Require(Search(3, 0, 3, &hits) == Names{"SORT_B", "SORT_A", "SORT_C"}, "modification times must sort ascending");
    Require(Search(3, 1, 1, &hits) == Names{"SORT_C"} && hits == 3, "the newest save must come first in descending modification time");
    SceSaveDataDirName pattern{};
    std::snprintf(pattern.data, sizeof(pattern.data), "SORT_%%");
    SaveDataDirNameSearchCond second{};
    second.user_id = 1;
    second.dir_name = &pattern;
    second.key = 1;
    second.order = 1;
    std::array<SceSaveDataDirName, 3> secondNames{};
    SaveDataDirNameSearchResult secondResult{};
    secondResult.dir_names = secondNames.data();
    secondResult.dir_names_num = 3;
    Require(sceSaveDataDirNameSearch2(&second, &secondResult) == 0, "the second search failed");
    Require(secondResult.hit_num == 3 && secondResult.set_num == 3, "the second search must return all three saves");
    Require(std::string(secondNames[0].data) == "SORT_A" && std::string(secondNames[1].data) == "SORT_C" && std::string(secondNames[2].data) == "SORT_B", "the second search must sort like the first");
    second.key = 6;
    Require(sceSaveDataDirNameSearch2(&second, &secondResult) == SaveDataErrorParameter, "the second search must reject an unknown sort key");
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.key = 6;
    SaveDataDirNameSearchResult result{};
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter, "an unknown sort key must be rejected");
    cond.key = 0;
    cond.order = 2;
    Require(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter, "an unknown sort order must be rejected");
}

static std::vector<std::string> SearchPattern(const char* text) {
    SceSaveDataDirName pattern{};
    std::snprintf(pattern.data, sizeof(pattern.data), "%s", text);
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.dir_name = &pattern;
    std::array<SceSaveDataDirName, 4> names{};
    SaveDataDirNameSearchResult result{};
    result.dir_names = names.data();
    result.dir_names_num = static_cast<std::uint32_t>(names.size());
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0, "pattern search failed");
    std::vector<std::string> found;
    for (std::uint32_t i = 0; i < result.set_num; ++i) found.emplace_back(names[i].data);
    return found;
}

static void TrailingWildcards() {
    for (const char* name : std::array{"TRAIL", "TRAIL1"}) std::filesystem::create_directories(std::filesystem::path("_sd") / name);
    using Names = std::vector<std::string>;
    Require(SearchPattern("TRAIL%") == Names{"TRAIL", "TRAIL1"}, "a trailing % must also match no characters");
    Require(SearchPattern("TRAIL%%") == Names{"TRAIL", "TRAIL1"}, "trailing %% must also match no characters");
    Require(SearchPattern("TRAI_%") == Names{"TRAIL", "TRAIL1"}, "_ then % must match one character and then none");
    Require(SearchPattern("TRAIL_%") == Names{"TRAIL1"}, "_ must still match exactly one character");
}

static std::uint32_t SearchTitle(const char* titleId, bool second) {
    SceSaveDataTitleId title{};
    std::snprintf(title.data, sizeof(title.data), "%s", titleId != nullptr ? titleId : "");
    SceSaveDataDirName pattern{};
    std::snprintf(pattern.data, sizeof(pattern.data), "OTHER_%%");
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.title_id = titleId != nullptr ? &title : nullptr;
    cond.dir_name = &pattern;
    std::array<SceSaveDataDirName, 1> names{};
    names[0].data[0] = '!';
    SaveDataDirNameSearchResult result{};
    result.dir_names = names.data();
    result.dir_names_num = 1;
    result.hit_num = 7;
    result.set_num = 7;
    Require((second ? sceSaveDataDirNameSearch2(&cond, &result) : sceSaveDataDirNameSearch(&cond, &result)) == 0, "title search failed");
    Require(result.set_num == std::min<std::uint32_t>(result.hit_num, 1), "title search must set the names it found");
    Require(result.set_num == 1 ? std::string(names[0].data) == "OTHER_A" : names[0].data[0] == '!', "title search returned the wrong name");
    return result.hit_num;
}

static void OtherTitle() {
    std::filesystem::create_directories("app0/sce_sys");
    std::FILE* param = std::fopen("app0/sce_sys/param.json", "wb");
    Require(param != nullptr, "param.json was not created");
    std::fputs("{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}}}", param);
    std::fclose(param);
    std::filesystem::create_directories(std::filesystem::path("_sd") / "OTHER_A");
    for (const bool second : {false, true}) {
        Require(SearchTitle(nullptr, second) == 1, "a search without a title id must find the running title's save");
        Require(SearchTitle("PPSA00000", second) == 1, "a search for the running title must find its save");
        Require(SearchTitle("PPSA17221", second) == 0, "a search for another title must find none of the running title's saves");
    }
}

static void SearchAndMount() {
    const std::array names{std::string(31, 'A'), std::string(31, 'B')};
    for (const auto& name : names) std::filesystem::create_directories(std::filesystem::path("_sd") / name);

    alignas(SceSaveDataDirName) std::array<char, 80> buffer;
    buffer.fill('!');
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    SaveDataDirNameSearchResult result{};
    result.dir_names = reinterpret_cast<SceSaveDataDirName*>(buffer.data());
    result.dir_names_num = 2;
    alignas(SaveDataSearchInfo) std::array<unsigned char, 112> infos;
    infos.fill(0x21);
    result.infos = reinterpret_cast<SaveDataSearchInfo*>(infos.data());
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0, "search failed");
    Require(result.hit_num == 2 && result.set_num == 2, "search must return both saves");
    for (std::size_t i = 64; i < buffer.size(); ++i) {
        Require(buffer[i] == '!', "directory search overran two 32-byte guest records");
    }
    for (std::size_t i = 96; i < infos.size(); ++i) {
        Require(infos[i] == 0x21, "directory search overran two 48-byte search infos");
    }
    for (std::size_t i = 0; i < 2; ++i) {
        SaveDataSearchInfo info{};
        std::memcpy(&info, infos.data() + i * 48, sizeof(info));
        Require(info.blocks == 16384 && info.free_blocks == 16384, "search info must report the save's blocks and free blocks");
        for (const auto byte : info.reserved) Require(byte == 0, "search info reserved bytes must be zero");
    }
    std::array<bool, 2> found{};
    for (int i = 0; i < 2; ++i) {
        const char* record = buffer.data() + i * 32;
        Require(record[31] == '\0', "directory record must terminate within 32 bytes");
        const bool first = std::strcmp(record, names[0].c_str()) == 0;
        Require(first || std::strcmp(record, names[1].c_str()) == 0, "returned directory name is corrupted");
        const int index = first ? 0 : 1;
        Require(!found[index], "search returned a duplicate directory");
        found[index] = true;

        SaveDataMount3 mount{};
        mount.user_id = cond.user_id;
        mount.dir_name = reinterpret_cast<const SceSaveDataDirName*>(record);
        mount.mount_mode = 1;
        SaveDataMountResult mounted{};
        Require(sceSaveDataMount3(&mount, &mounted) == 0, "returned directory must be mountable");
        Require(sceSaveDataUmount2(0, &mounted.mount_point) == 0, "unmount failed");
    }
}

static void CreateSized(const char* name, std::uint64_t blocks, std::size_t bytes) {
    SceSaveDataDirName dirName{};
    std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);
    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.blocks = blocks;
    mount.mount_mode = 2 | 4;
    SaveDataMountResult mounted{};
    Require(sceSaveDataMount3(&mount, &mounted) == 0, "creating a sized save failed");
    if (bytes != 0) {
        std::ofstream file(std::filesystem::path("_sd") / name / "data", std::ios::binary);
        file << std::string(bytes, 'x');
    }
    Require(sceSaveDataUmount2(0, &mounted.mount_point) == 0, "unmount failed");
}

static std::vector<SaveDataSearchInfo> SizeSearch(std::uint32_t key, std::vector<std::string>& found) {
    SceSaveDataDirName pattern{};
    std::snprintf(pattern.data, sizeof(pattern.data), "SIZE_%%");
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.dir_name = &pattern;
    cond.key = key;
    std::array<SceSaveDataDirName, 2> names{};
    std::array<SaveDataSearchInfo, 2> infos{};
    SaveDataDirNameSearchResult result{};
    result.dir_names = names.data();
    result.dir_names_num = 2;
    result.infos = infos.data();
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0 && result.set_num == 2, "the sized search must return both saves");
    found.clear();
    for (const auto& name : names) found.emplace_back(name.data);
    return {infos.begin(), infos.end()};
}

static SaveDataMountInfo MountInfo(const char* name) {
    SceSaveDataDirName dirName{};
    std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);
    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = 1;
    SaveDataMountResult mounted{};
    Require(sceSaveDataMount3(&mount, &mounted) == 0, "mounting the sized save failed");
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&mounted.mount_point, &info) == 0, "mount info failed");
    Require(sceSaveDataUmount2(0, &mounted.mount_point) == 0, "unmount failed");
    return info;
}

static void SaveSize() {
    CreateSized("SIZE_A", 200, 0);
    CreateSized("SIZE_B", 96, 40000);
    std::vector<std::string> found;
    auto infos = SizeSearch(0, found);
    Require(found == std::vector<std::string>{"SIZE_A", "SIZE_B"}, "the sized saves must sort by name");
    Require(infos[0].blocks == 200 && infos[0].free_blocks == 200, "an empty save must report all its blocks free");
    Require(infos[1].blocks == 96 && infos[1].free_blocks == 95, "a save must report the blocks it was created with, less those its files take");
    SizeSearch(2, found);
    Require(found == std::vector<std::string>{"SIZE_B", "SIZE_A"}, "saves must sort by blocks");
    infos = SizeSearch(5, found);
    Require(found == std::vector<std::string>{"SIZE_B", "SIZE_A"} && infos[0].free_blocks < infos[1].free_blocks, "saves must sort by free blocks");
    const auto info = MountInfo("SIZE_B");
    Require(info.blocks == 96 && info.free_blocks == 95, "mount info must report the save's blocks and free blocks");
}

static void CheckNullArguments() {
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    SaveDataDirNameSearchResult result{};
    Require(sceSaveDataDirNameSearch(nullptr, &result) == SaveDataErrorParameter, "null cond returns PARAMETER in sceSaveDataDirNameSearch");
    Require(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter, "null result returns PARAMETER in sceSaveDataDirNameSearch");
    Require(sceSaveDataDirNameSearch2(nullptr, &result) == SaveDataErrorParameter, "null cond returns PARAMETER in sceSaveDataDirNameSearch2");
    Require(sceSaveDataDirNameSearch2(&cond, nullptr) == SaveDataErrorParameter, "null result returns PARAMETER in sceSaveDataDirNameSearch2");
    cond.order = 2;
    Require(sceSaveDataDirNameSearch2(&cond, &result) == SaveDataErrorParameter, "invalid order must be rejected in sceSaveDataDirNameSearch2");
}

static void BlockSize() {
    CreateSized("BLOCK_A", 48, 70000);
    auto info = MountInfo("BLOCK_A");
    Require(info.blocks == 48 && info.free_blocks == 46, "a file must take whole 64 KiB blocks");
    CreateSized("BLOCK_B", 32768, 0);
    info = MountInfo("BLOCK_B");
    Require(info.blocks == 16384 && info.free_blocks == 16384, "a save must hold at most 16384 blocks");
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-savedata-search-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    int status = 0;
    try {
        SearchAndMount();
        SortByKey();
        OtherTitle();
        TrailingWildcards();
        SaveSize();
        BlockSize();
        CheckNullArguments();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        status = 1;
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return status;
}
