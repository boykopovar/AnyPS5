#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate(void);
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2*, SaveDataMemorySetupResult*);
#ifndef SAVEDATA_NATIVE_BACKEND
int APS5_VABI sceSaveDataSyncSaveDataMemory(const void*);
#endif
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Save-data replacement check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    const auto originalDirectory = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-savedata-replacement-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::current_path(root);

    constexpr std::int32_t userId = 7531;
#ifdef SAVEDATA_NATIVE_BACKEND
    const auto savePath = std::filesystem::path("_sd_mem") / ("u" + std::to_string(userId)) / "slot0.bin";
#else
    const auto savePath = std::filesystem::path("_sd") / "sce_sdmemory" /
        std::to_string(userId) / "memory.dat";
#endif
    std::filesystem::create_directories(savePath.parent_path());
    const std::vector<char> oldData{'o', 'l', 'd'};
    {
        std::ofstream file(savePath, std::ios::binary);
        Require(file.is_open());
        file.write(oldData.data(), static_cast<std::streamsize>(oldData.size()));
        Require(static_cast<bool>(file));
    }

    Require(sceSaveDataInitialize3(nullptr) == 0);
    SaveDataMemorySetup2 setup{};
    setup.user_id = userId;
    setup.memory_size = oldData.size() + 3;
    SaveDataMemorySetupResult result{};
    int status = sceSaveDataSetupSaveDataMemory2(&setup, &result);

#ifndef SAVEDATA_NATIVE_BACKEND
    struct MemorySyncParam {
        std::int32_t user_id;
        std::uint32_t slot_id;
        std::uint32_t option;
    } sync{userId, 0, 0};
    if (status == 0) {
        status = sceSaveDataSyncSaveDataMemory(&sync);
    }
#endif

    Require(result.existed_memory_size == oldData.size());
    Require(status == 0);
    std::vector<char> savedData;
    {
        std::ifstream savedFile(savePath, std::ios::binary);
        savedData.assign(std::istreambuf_iterator<char>(savedFile), std::istreambuf_iterator<char>());
    }
    Require(savedData.size() == setup.memory_size);
    Require(std::equal(oldData.begin(), oldData.end(), savedData.begin()));
    Require(std::all_of(savedData.begin() + static_cast<std::ptrdiff_t>(oldData.size()),
                        savedData.end(), [](char byte) { return byte == 0; }));

#ifdef SAVEDATA_NATIVE_BACKEND
    Require(sceSaveDataTerminate() == 0);
#endif
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(root);
}
