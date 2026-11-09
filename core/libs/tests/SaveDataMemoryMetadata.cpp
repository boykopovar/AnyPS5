#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
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
#include <limits>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/resource.h>
#endif

namespace {
constexpr int kSaveDataNotInitialized = -2137063423;
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

static void RequireNoTemporary(const std::filesystem::path& path) {
    const auto prefix = path.filename().string() + ".tmp";
    for (const auto& entry : std::filesystem::directory_iterator(path.parent_path())) {
        Require(!entry.path().filename().string().starts_with(prefix));
    }
}

template <typename Operation>
static int BlockDestination(const std::filesystem::path& path, Operation&& operation) {
    Require(std::filesystem::create_directory(path));
    const int result = operation();
    Require(std::filesystem::is_directory(path));
    Require(std::filesystem::remove(path));
    RequireNoTemporary(path);
    return result;
}

template <typename Operation>
static int FailWrite(const std::filesystem::path& path, [[maybe_unused]] std::size_t sizeLimit,
                     Operation&& operation) {
#ifdef _WIN32
    const auto locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(locked != INVALID_HANDLE_VALUE);
    const int result = operation();
    Require(CloseHandle(locked) != 0);
#else
    rlimit previousLimit{};
    Require(getrlimit(RLIMIT_FSIZE, &previousLimit) == 0);
    auto writeLimit = previousLimit;
    writeLimit.rlim_cur = sizeLimit;
    const auto previousHandler = std::signal(SIGXFSZ, SIG_IGN);
    Require(previousHandler != SIG_ERR);
    Require(setrlimit(RLIMIT_FSIZE, &writeLimit) == 0);
    const int result = operation();
    Require(setrlimit(RLIMIT_FSIZE, &previousLimit) == 0);
    Require(std::signal(SIGXFSZ, previousHandler) != SIG_ERR);
#endif
    RequireNoTemporary(path);
    return result;
}

int main() {
    Require(sceSaveDataCreateTransactionResource(1024) == kSaveDataNotInitialized);
    Require(sceSaveDataDeleteTransactionResource(1) == kSaveDataNotInitialized);
    const std::uint32_t syncParamUninit[2] = {7531u, 0u};
    Require(sceSaveDataSyncSaveDataMemory(nullptr) == static_cast<int>(0x809F0000u));
    Require(sceSaveDataSyncSaveDataMemory(syncParamUninit) == static_cast<int>(0x809F0001u));
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
    const int tx0 = sceSaveDataCreateTransactionResource(0);
    Require(tx0 > 0);
    Require(sceSaveDataDeleteTransactionResource(tx0) == 0);
    const int tx = sceSaveDataCreateTransactionResource(1024);
    Require(tx > 0);
    Require(sceSaveDataDeleteTransactionResource(tx) == 0);
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
    const int status = FailWrite(path, 16, [&] { return sceSaveDataSetSaveDataMemory2(&set); });
    Require(Read(path) == original);
    Require(status == static_cast<int>(0x809F000Bu));
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
    const int writeStatus = FailWrite(memoryPath, 16, [&] { return sceSaveDataSetSaveDataMemory2(&set); });
    Require(writeStatus == static_cast<int>(0x809F000Bu));
    Require(Read(memoryPath) == memoryOriginal);
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
    std::array<char, 4> third{'p', 'q', 'r', 's'};
    std::array<char, 4> fourth{'t', 'u', 'v', 'w'};
    constexpr std::size_t GuestMemoryDataSize = 64;
    alignas(SaveDataMemoryData) std::array<unsigned char, 2 * GuestMemoryDataSize> guestData{};
    const auto putGuestData = [&guestData](std::size_t index, const char* buffer, std::uint64_t size, std::uint64_t offset) {
        unsigned char* entry = guestData.data() + index * GuestMemoryDataSize;
        std::memcpy(entry, &buffer, sizeof(buffer));
        std::memcpy(entry + 8, &size, sizeof(size));
        std::memcpy(entry + 16, &offset, sizeof(offset));
    };
    putGuestData(0, third.data(), third.size(), 0);
    putGuestData(1, fourth.data(), fourth.size(), 16);
    set.data = reinterpret_cast<const SaveDataMemoryData*>(guestData.data());
    set.data_num = 2;
    Require(sceSaveDataSetSaveDataMemory2(&set) == 0);
    std::copy(third.begin(), third.end(), memoryExpected.begin());
    std::copy(fourth.begin(), fourth.end(), memoryExpected.begin() + 16);
    Require(Read(memoryPath) == memoryExpected);
    const std::uint32_t syncWritten[2] = {7531u, 0u};
    Require(sceSaveDataSyncSaveDataMemory(syncWritten) == 0);
    Require(Read(memoryPath) == memoryExpected);
    const auto setupParamPath = std::filesystem::path("_sd_mem/u42/slot0.param");
    const auto setupBinPath = std::filesystem::path("_sd_mem/u42/slot0.bin");
    Require(std::filesystem::create_directories(setupParamPath.parent_path()));
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
    Require(BlockDestination(setupParamPath, [&] { return sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult); }) == static_cast<int>(0x809F000Bu));
    Require(setupResult.existed_memory_size == 555);
    Require(!std::filesystem::exists(setupBinPath));
    Require(!std::filesystem::exists(setupParamPath));
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 0);
    Require(Read(setupBinPath) == std::vector<char>(128, 0));
    const std::uint32_t syncSetup[2] = {42u, 0u};
    Require(sceSaveDataSyncSaveDataMemory(syncSetup) == 0);
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
    SaveDataParam previousParam = setupParam;
    setupParam.user_param = 200;
    setup2.memory_size = 256;
    setupResult.existed_memory_size = 999;
    Require(FailWrite(setupParamPath, 16, [&] { return sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult); }) == static_cast<int>(0x809F000Bu));
    Require(setupResult.existed_memory_size == 999);
    Require(Read(setupBinPath) == seededBin);
    const auto* previousBytes = reinterpret_cast<const char*>(&previousParam);
    Require(Read(setupParamPath) == std::vector<char>(previousBytes, previousBytes + sizeof(previousParam)));
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 128);
    std::vector<char> expectedGrownBin = seededBin;
    expectedGrownBin.resize(256, 0);
    Require(Read(setupBinPath) == expectedGrownBin);
    const auto* updatedBytes = reinterpret_cast<const char*>(&setupParam);
    Require(Read(setupParamPath) == std::vector<char>(updatedBytes, updatedBytes + sizeof(setupParam)));
    setupParam.user_param = 300;
    constexpr std::size_t GrownMemorySize = 2 * sizeof(SaveDataParam);
    setup2.memory_size = GrownMemorySize;
    setupResult.existed_memory_size = 888;
    Require(FailWrite(setupBinPath, sizeof(SaveDataParam), [&] { return sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult); }) == static_cast<int>(0x809F000Bu));
    Require(setupResult.existed_memory_size == 888);
    Require(Read(setupBinPath) == expectedGrownBin);
    const auto* param300Bytes = reinterpret_cast<const char*>(&setupParam);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == 256);
    std::vector<char> expectedFinalBlob = expectedGrownBin;
    expectedFinalBlob.resize(GrownMemorySize, 0);
    Require(Read(setupBinPath) == expectedFinalBlob);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    const auto freshFailBinPath = std::filesystem::path("_sd_mem/u55/slot0.bin");
    const auto freshFailParamPath = std::filesystem::path("_sd_mem/u55/slot0.param");
    Require(std::filesystem::create_directories(freshFailBinPath.parent_path()));
    SaveDataParam freshFailParam{};
    freshFailParam.user_param = 55;
    SaveDataMemorySetup2 freshFailSetup{};
    freshFailSetup.user_id = 55;
    freshFailSetup.slot_id = 0;
    freshFailSetup.memory_size = 64;
    freshFailSetup.option = 1;
    freshFailSetup.init_param = &freshFailParam;
    SaveDataMemorySetupResult freshFailResult{};
    freshFailResult.existed_memory_size = 777;
    Require(BlockDestination(freshFailBinPath, [&] { return sceSaveDataSetupSaveDataMemory2(&freshFailSetup, &freshFailResult); }) == static_cast<int>(0x809F000Bu));
    Require(freshFailResult.existed_memory_size == 777);
    Require(!std::filesystem::exists(freshFailBinPath));
    const auto* freshFailBytes = reinterpret_cast<const char*>(&freshFailParam);
    Require(Read(freshFailParamPath) == std::vector<char>(freshFailBytes, freshFailBytes + sizeof(freshFailParam)));
    Require(sceSaveDataSetupSaveDataMemory2(&freshFailSetup, &freshFailResult) == 0);
    Require(freshFailResult.existed_memory_size == 0);
    Require(Read(freshFailBinPath) == std::vector<char>(64, 0));
    Require(Read(freshFailParamPath) == std::vector<char>(freshFailBytes, freshFailBytes + sizeof(freshFailParam)));
    SaveDataParam noParam{};
    noParam.user_param = 5;
    SaveDataMemorySetup2 noParamSetup{};
    SaveDataMemorySetupResult noParamResult{};
    noParamSetup.user_id = 56;
    noParamSetup.slot_id = 0;
    noParamSetup.memory_size = 16;
    noParamSetup.option = 1;
    noParamSetup.init_param = nullptr;
    Require(sceSaveDataSetupSaveDataMemory2(&noParamSetup, &noParamResult) == 0);
    Require(Read("_sd_mem/u56/slot0.bin") == std::vector<char>(16, 0));
    Require(!std::filesystem::exists("_sd_mem/u56/slot0.param"));
    noParamSetup.user_id = 57;
    noParamSetup.option = 0;
    noParamSetup.init_param = &noParam;
    Require(sceSaveDataSetupSaveDataMemory2(&noParamSetup, &noParamResult) == 0);
    Require(Read("_sd_mem/u57/slot0.bin") == std::vector<char>(16, 0));
    Require(!std::filesystem::exists("_sd_mem/u57/slot0.param"));
    SaveDataParam userA{};
    userA.user_param = 1;
    SaveDataParam userB{};
    userB.user_param = 2;
    SaveDataMemorySetup2 isoSetup{};
    SaveDataMemorySetupResult isoResult{};
    isoSetup.user_id = 60;
    isoSetup.slot_id = 0;
    isoSetup.memory_size = 8;
    isoSetup.option = 1;
    isoSetup.init_param = &userA;
    Require(sceSaveDataSetupSaveDataMemory2(&isoSetup, &isoResult) == 0);
    isoSetup.slot_id = 1;
    isoSetup.init_param = &userB;
    Require(sceSaveDataSetupSaveDataMemory2(&isoSetup, &isoResult) == 0);
    const auto* userABytes = reinterpret_cast<const char*>(&userA);
    const auto* userBBytes = reinterpret_cast<const char*>(&userB);
    Require(Read("_sd_mem/u60/slot0.param") == std::vector<char>(userABytes, userABytes + sizeof(userA)));
    Require(Read("_sd_mem/u60/slot1.param") == std::vector<char>(userBBytes, userBBytes + sizeof(userB)));
    isoSetup.user_id = 61;
    isoSetup.slot_id = 0;
    Require(sceSaveDataSetupSaveDataMemory2(&isoSetup, &isoResult) == 0);
    Require(Read("_sd_mem/u60/slot0.param") == std::vector<char>(userABytes, userABytes + sizeof(userA)));
    Require(Read("_sd_mem/u61/slot0.param") == std::vector<char>(userBBytes, userBBytes + sizeof(userB)));
    const auto orphanParamPath = std::filesystem::path("_sd_mem/u99/slot0.param");
    const auto orphanBinPath = std::filesystem::path("_sd_mem/u99/slot0.bin");
    Require(std::filesystem::create_directories(orphanBinPath.parent_path()));
    const std::vector<char> orphanBin(64, 'k');
    {
        std::ofstream orphanFile(orphanBinPath, std::ios::binary);
        orphanFile.write(orphanBin.data(), static_cast<std::streamsize>(orphanBin.size()));
        Require(static_cast<bool>(orphanFile));
    }
    SaveDataParam orphanParam{};
    orphanParam.user_param = 42;
    SaveDataMemorySetup2 orphanSetup{};
    orphanSetup.user_id = 99;
    orphanSetup.slot_id = 0;
    orphanSetup.memory_size = 64;
    orphanSetup.option = 1;
    orphanSetup.init_param = &orphanParam;
    SaveDataMemorySetupResult orphanResult{};
    orphanResult.existed_memory_size = 333;
    Require(sceSaveDataSetupSaveDataMemory2(&orphanSetup, &orphanResult) == 0);
    Require(orphanResult.existed_memory_size == 64);
    Require(Read(orphanBinPath) == orphanBin);
    const auto* orphanBytes = reinterpret_cast<const char*>(&orphanParam);
    Require(Read(orphanParamPath) == std::vector<char>(orphanBytes, orphanBytes + sizeof(orphanParam)));
    Require(sceSaveDataTerminate() == 0);
    Require(Read(setupBinPath) == expectedFinalBlob);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    Require(sceSaveDataInitialize3(nullptr) == 0);
    setupResult.existed_memory_size = 111;
    Require(sceSaveDataSetupSaveDataMemory2(&setup2, &setupResult) == 0);
    Require(setupResult.existed_memory_size == GrownMemorySize);
    Require(Read(setupBinPath) == expectedFinalBlob);
    Require(Read(setupParamPath) == std::vector<char>(param300Bytes, param300Bytes + sizeof(setupParam)));
    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataCreateTransactionResource(1024) == kSaveDataNotInitialized);
    Require(sceSaveDataDeleteTransactionResource(tx) == kSaveDataNotInitialized);
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
