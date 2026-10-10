#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>
#ifndef _WIN32
#include <csignal>
#include <sys/resource.h>
#endif

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate();
int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Save-data memory check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-metadata-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::current_path(root);
    const auto path = std::filesystem::path("_sd_mem/u7531/slot0.param");
    const auto memoryPath = std::filesystem::path("_sd_mem/u7531/slot0.bin");
    std::filesystem::create_directories(path.parent_path());
    const std::vector<char> memoryOriginal(32, 's');
    {
        std::ofstream file(memoryPath, std::ios::binary);
        file.write(memoryOriginal.data(), static_cast<std::streamsize>(memoryOriginal.size()));
        Require(static_cast<bool>(file));
    }
    Require(sceSaveDataInitialize3(nullptr) == 0);
    Require(sceSaveDataInitialize3(nullptr) == 0);
    Require(sceSaveDataTerminate() == 0);
    SaveDataParam param{};
    SaveDataMemorySet2 set{};
    set.user_id = 7531;
    set.param = &param;
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    const auto original = Read(path);
    Require(original.size() == sizeof(param));
    param.user_param = 42;
    const auto temporary = path.string() + ".tmp";
    Require(std::filesystem::create_directory(temporary));
    const int status = sceSaveDataSetSaveDataMemory2(&set);
    Require(Read(path) == original);
    Require(status == static_cast<int>(0x809F000Bu));
    Require(!std::filesystem::exists(temporary));
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    const auto* bytes = reinterpret_cast<const char*>(&param);
    Require(Read(path) == std::vector<char>(bytes, bytes + sizeof(param)));
    set.param = nullptr;
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    std::array<char, 8> first{'a', '\0', 'b', 'c', 'd', 'e', 'f', 'g'};
    std::array<char, 8> second{'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o'};
    std::array<SaveDataMemoryData, 3> data{{
        {first.data(), first.size(), 4},
        {second.data(), second.size(), 24},
        {nullptr, 0, std::numeric_limits<std::size_t>::max()}
    }};
    set.data = data.data();
    set.data_num = static_cast<std::uint32_t>(data.size());
    data[1].offset = memoryOriginal.size();
    Require(sceSaveDataSetSaveDataMemory2(&set) == static_cast<int>(0x809F0000u));
    Require(Read(memoryPath) == memoryOriginal);
    data[1].offset = 24;
    const auto memoryTemporary = memoryPath.string() + ".tmp";
    Require(std::filesystem::create_directory(memoryTemporary));
    Require(sceSaveDataSetSaveDataMemory2(&set) == static_cast<int>(0x809F000Bu));
    Require(Read(memoryPath) == memoryOriginal);
    Require(!std::filesystem::exists(memoryTemporary));
#ifndef _WIN32
    rlimit previousLimit{};
    Require(getrlimit(RLIMIT_FSIZE, &previousLimit) == 0);
    auto writeLimit = previousLimit;
    writeLimit.rlim_cur = 16;
    const auto previousHandler = std::signal(SIGXFSZ, SIG_IGN);
    Require(previousHandler != SIG_ERR);
    Require(setrlimit(RLIMIT_FSIZE, &writeLimit) == 0);
    const int writeStatus = sceSaveDataSetSaveDataMemory2(&set);
    Require(setrlimit(RLIMIT_FSIZE, &previousLimit) == 0);
    Require(std::signal(SIGXFSZ, previousHandler) != SIG_ERR);
    Require(writeStatus == static_cast<int>(0x809F000Bu));
    Require(Read(memoryPath) == memoryOriginal);
    Require(!std::filesystem::exists(memoryTemporary));
#endif
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    auto memoryExpected = memoryOriginal;
    std::copy(first.begin(), first.end(), memoryExpected.begin() + 4);
    std::copy(second.begin(), second.end(), memoryExpected.begin() + 24);
    Require(Read(memoryPath) == memoryExpected);
    first.fill('z');
    set.data_num = 0;
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    std::copy(first.begin(), first.end(), memoryExpected.begin() + 4);
    Require(Read(memoryPath) == memoryExpected);
    Require(sceSaveDataTerminate() == 0);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
