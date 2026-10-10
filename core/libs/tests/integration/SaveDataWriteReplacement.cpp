#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataTerminate(void);
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2*, SaveDataMemorySetupResult*);
#ifndef SAVEDATA_NATIVE_BACKEND
int APS5_VABI sceSaveDataSyncSaveDataMemory(const void*);
#endif
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::int32_t userId = 7531;

class WorkingDirectory {
public:
    WorkingDirectory() : previous(std::filesystem::current_path()) {
        std::filesystem::current_path(directory.Path());
    }

    ~WorkingDirectory() {
        std::error_code ignored;
        std::filesystem::current_path(previous, ignored);
    }

    WorkingDirectory(const WorkingDirectory&) = delete;
    WorkingDirectory& operator=(const WorkingDirectory&) = delete;

private:
    Testing::TemporaryDirectory directory;
    std::filesystem::path previous;
};

std::filesystem::path SavePath() {
#ifdef SAVEDATA_NATIVE_BACKEND
    return std::filesystem::path("_sd_mem") / ("u" + std::to_string(userId)) / "slot0.bin";
#else
    return std::filesystem::path("_sd") / "sce_sdmemory" / std::to_string(userId) / "memory.dat";
#endif
}

void WriteFile(const std::filesystem::path& path, const std::vector<char>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    Require(file.is_open(), "open the existing save file for writing");
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    Require(static_cast<bool>(file), "write the existing save file");
}

std::vector<char> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const Case growExisting{"SetupSaveDataMemory_ExistingSmallerFile_KeepsDataAndZeroFillsGrowth", [] {
    const WorkingDirectory workingDirectory;
    const auto savePath = SavePath();
    const std::vector<char> oldData{'o', 'l', 'd'};
    WriteFile(savePath, oldData);
    RequireEqual(sceSaveDataInitialize3(nullptr), 0, "initialize");
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
    if (status == 0) status = sceSaveDataSyncSaveDataMemory(&sync);
#endif
    RequireEqual(result.existed_memory_size, static_cast<decltype(result.existed_memory_size)>(oldData.size()),
        "existing memory size");
    RequireEqual(status, 0, "setup and sync status");
    const auto savedData = ReadFile(savePath);
    RequireEqual(savedData.size(), static_cast<std::size_t>(setup.memory_size), "saved size");
    Require(std::equal(oldData.begin(), oldData.end(), savedData.begin()), "existing bytes are kept");
    Require(std::all_of(savedData.begin() + static_cast<std::ptrdiff_t>(oldData.size()), savedData.end(),
                        [](char byte) { return byte == 0; }), "grown bytes are zero");
#ifdef SAVEDATA_NATIVE_BACKEND
    RequireEqual(sceSaveDataTerminate(), 0, "terminate");
#endif
}};

} // namespace
