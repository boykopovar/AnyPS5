#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate();
int APS5_VABI sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2*);
int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2*);
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2*, SaveDataMemorySetupResult*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Save-data memory journal check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static void Write(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file));
}

static std::vector<char> Journal(std::uint64_t offset, const std::string& text) {
    std::vector<char> journal{'A', 'P', 'S', '5', 'M', 'J', 'N', '1'};
    const std::uint64_t size = text.size();
    const std::size_t at = journal.size();
    journal.resize(at + sizeof(offset) + sizeof(size) + text.size());
    std::memcpy(journal.data() + at, &offset, sizeof(offset));
    std::memcpy(journal.data() + at + sizeof(offset), &size, sizeof(size));
    std::memcpy(journal.data() + at + sizeof(offset) + sizeof(size), text.data(), text.size());
    return journal;
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-memory-journal-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::current_path(root);
    constexpr std::int32_t userId = 7531;
    const auto memoryPath = std::filesystem::path("_sd_mem/u7531/slot0.bin");
    const auto journalPath = std::filesystem::path("_sd_mem/u7531/slot0.journal");
    const auto temporaryPath = std::filesystem::path("_sd_mem/u7531/slot0.bin.tmp");
    const auto aliasPath = std::filesystem::path("alias.bin");

    Require(sceSaveDataInitialize3(nullptr) == 0);
    SaveDataMemorySetup2 setup{};
    setup.user_id = userId;
    setup.memory_size = 64;
    SaveDataMemorySetupResult result{};
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
    std::vector<char> expected(64, 0);
    Require(Read(memoryPath) == expected);
    std::filesystem::create_hard_link(memoryPath, aliasPath);

    std::array<char, 4> first{'a', 'b', 'c', 'd'};
    std::array<char, 3> second{'x', 'y', 'z'};
    std::array<SaveDataMemoryData, 2> data{{{first.data(), first.size(), 8}, {second.data(), second.size(), 61}}};
    SaveDataMemorySet2 set{};
    set.user_id = userId;
    set.data = data.data();
    set.data_num = static_cast<std::uint32_t>(data.size());
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    std::copy(first.begin(), first.end(), expected.begin() + 8);
    std::copy(second.begin(), second.end(), expected.begin() + 61);
    Require(Read(memoryPath) == expected);
    Require(Read(aliasPath) == expected);
    Require(!std::filesystem::exists(journalPath));
    Require(!std::filesystem::exists(temporaryPath));

    Require(std::filesystem::create_directory(temporaryPath));
    first.fill('q');
    Require(sceSaveDataSetSaveDataMemory2(&set) == static_cast<int>(0x809F000Bu));
    Require(Read(memoryPath) == expected);
    Require(!std::filesystem::exists(journalPath));
    Require(!std::filesystem::exists(temporaryPath));

    Write(journalPath, Journal(16, "jour"));
    std::array<char, 4> got{};
    SaveDataMemoryData range{got.data(), got.size(), 16};
    SaveDataMemoryGet2 get{};
    get.user_id = userId;
    get.data = &range;
    Require(sceSaveDataGetSaveDataMemory2(&get) == 0);
    Require(std::string(got.data(), got.size()) == "jour");
    std::copy_n("jour", 4, expected.begin() + 16);
    Require(Read(memoryPath) == expected);
    Require(Read(aliasPath) == expected);
    Require(!std::filesystem::exists(journalPath));

    Require(sceSaveDataTerminate() == 0);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
