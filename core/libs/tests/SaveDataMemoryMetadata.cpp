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
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2*, SaveDataMemorySetupResult*);
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

static void CheckMetadataRecovery(std::size_t requestedSize, std::size_t expectedSize) {
    const auto base = std::filesystem::path("_sd_mem/u99") / ("slot" + std::to_string(requestedSize));
    const auto paramPath = base.string() + ".param";
    const auto binPath = base.string() + ".bin";
    std::filesystem::create_directories(base.parent_path());
    std::vector<char> original(64);
    for (std::size_t index = 0; index < original.size(); ++index) {
        original[index] = static_cast<char>(index * 3 + 7);
    }
    {
        std::ofstream file(binPath, std::ios::binary);
        file.write(original.data(), static_cast<std::streamsize>(original.size()));
        Require(static_cast<bool>(file));
    }
    SaveDataParam param{};
    param.user_param = 42;
    SaveDataMemorySetup2 setup{};
    setup.user_id = 99;
    setup.slot_id = static_cast<std::uint32_t>(requestedSize);
    setup.memory_size = requestedSize;
    setup.option = 1;
    setup.init_param = &param;
    SaveDataMemorySetupResult result{};
    result.existed_memory_size = 333;
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
    Require(result.existed_memory_size == original.size());
    auto expected = original;
    expected.resize(expectedSize, 0);
    const auto actual = Read(binPath);
    if (actual.size() != expectedSize) {
        std::fprintf(stderr, "Metadata recovery requested %zu bytes: expected %zu bytes, got %zu\n",
                     requestedSize, expectedSize, actual.size());
    }
    Require(actual == expected);
    const auto* paramBytes = reinterpret_cast<const char*>(&param);
    const std::vector<char> expectedParam(paramBytes, paramBytes + sizeof(param));
    Require(Read(paramPath) == expectedParam);
    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataInitialize3(nullptr) == 0);
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == 0);
    Require(result.existed_memory_size == expectedSize);
    Require(Read(binPath) == expected);
    Require(Read(paramPath) == expectedParam);
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
    const auto setupParamPath = std::filesystem::path("_sd_mem/u42/slot0.param");
    const auto setupBinPath = std::filesystem::path("_sd_mem/u42/slot0.bin");
    const auto setupParamTemp = setupParamPath.string() + ".tmp";
    Require(std::filesystem::create_directories(setupParamPath.parent_path()));
    Require(std::filesystem::create_directory(setupParamTemp));
    SaveDataParam setupParam{};
    setupParam.user_param = 100;
    SaveDataMemorySetup2 setup2{};
    setup2.user_id = 42;
    setup2.slot_id = 0;
    setup2.memory_size = 128;
    setup2.option = 1;
    setup2.init_param = &setupParam;
    SaveDataMemorySetupResult setupResult{};
    setupResult.existed_memory_size = 555;
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == static_cast<int>(0x809F000Bu));
    Require(setupResult.existed_memory_size == 555);
    Require(!std::filesystem::exists(setupBinPath));
    Require(!std::filesystem::exists(setupParamPath));
    Require(!std::filesystem::exists(setupParamTemp));
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 0);
    Require(Read(setupBinPath) == std::vector<char>(128, 0));
    const auto* setupBytes = reinterpret_cast<const char*>(&setupParam);
    Require(Read(setupParamPath) == std::vector<char>(setupBytes, setupBytes + sizeof(setupParam)));
    std::vector<char> seededBin(128);
    for (std::size_t i = 0; i < seededBin.size(); ++i) {
        seededBin[i] = static_cast<char>(static_cast<unsigned char>(i * 3 + 7));
    }
    {
        std::ofstream seededFile(setupBinPath, std::ios::binary);
        seededFile.write(seededBin.data(), static_cast<std::streamsize>(seededBin.size()));
        Require(static_cast<bool>(seededFile));
    }
    const auto growthParamTemp = setupParamPath.string() + ".tmp";
    Require(std::filesystem::create_directory(growthParamTemp));
    SaveDataParam previousParam = setupParam;
    setupParam.user_param = 200;
    setup2.memory_size = 256;
    setupResult.existed_memory_size = 999;
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == static_cast<int>(0x809F000Bu));
    Require(setupResult.existed_memory_size == 999);
    Require(Read(setupBinPath) == seededBin);
    const auto* previousBytes = reinterpret_cast<const char*>(&previousParam);
    Require(Read(setupParamPath) == std::vector<char>(previousBytes, previousBytes + sizeof(previousParam)));
    Require(!std::filesystem::exists(growthParamTemp));
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 128);
    std::vector<char> expectedGrownBin = seededBin;
    expectedGrownBin.resize(256, 0);
    Require(Read(setupBinPath) == expectedGrownBin);
    const auto* updatedBytes = reinterpret_cast<const char*>(&setupParam);
    Require(Read(setupParamPath) == std::vector<char>(updatedBytes, updatedBytes + sizeof(setupParam)));
    const auto growthBinTemp = setupBinPath.string() + ".tmp";
    Require(std::filesystem::create_directory(growthBinTemp));
    SaveDataParam beforeBlobFailParam = setupParam;
    setupParam.user_param = 300;
    setup2.memory_size = 512;
    setupResult.existed_memory_size = 888;
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == static_cast<int>(0x809F000Bu));
    Require(setupResult.existed_memory_size == 888);
    Require(Read(setupBinPath) == expectedGrownBin);
    const auto* beforeBlobFailBytes = reinterpret_cast<const char*>(&beforeBlobFailParam);
    Require(Read(setupParamPath) == std::vector<char>(beforeBlobFailBytes, beforeBlobFailBytes + sizeof(beforeBlobFailParam)));
    Require(!std::filesystem::exists(growthBinTemp));
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 256);
    std::vector<char> expectedBlob512 = expectedGrownBin;
    expectedBlob512.resize(512, 0);
    Require(Read(setupBinPath) == expectedBlob512);
    const auto* param300Bytes = reinterpret_cast<const char*>(&setupParam);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    CheckMetadataRecovery(32, 64);
    CheckMetadataRecovery(64, 64);
    CheckMetadataRecovery(96, 96);
    Require(sceSaveDataTerminate() == 0);
    Require(Read(setupBinPath) == expectedBlob512);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    Require(sceSaveDataInitialize3(nullptr) == 0);
    setupResult.existed_memory_size = 111;
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 512);
    Require(Read(setupBinPath) == expectedBlob512);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    Require(sceSaveDataTerminate() == 0);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
