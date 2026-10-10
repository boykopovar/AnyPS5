#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
}

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
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
    Require(sceSaveDataDirNameSearch(&cond, &result) == 0, "search failed");
    Require(result.hit_num == 2 && result.set_num == 2, "search must return both saves");
    for (std::size_t i = 64; i < buffer.size(); ++i) {
        Require(buffer[i] == '!', "directory search overran two 32-byte guest records");
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

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-savedata-search-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    int status = 0;
    try {
        SearchAndMount();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        status = 1;
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return status;
}
