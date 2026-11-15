#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorMemoryNotReady = -2137063413;

struct MemorySyncParam {
    std::int32_t user_id;
    std::uint32_t slot_id;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedBounds() {
    char buf[64]{};
    Require(sceSaveDataSetupSaveDataMemory(1, 1024, nullptr) == SaveDataErrorNotInitialized);
    Require(sceSaveDataGetSaveDataMemory(1, buf, sizeof(buf), 0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataSetSaveDataMemory(1, buf, sizeof(buf), 0) == SaveDataErrorNotInitialized);

    MemorySyncParam sync{1, 0};
    Require(sceSaveDataSyncSaveDataMemory(&sync) == SaveDataErrorNotInitialized);
}

void TestUnreadyMemoryBounds() {
    Require(sceSaveDataInitialize3(nullptr) == 0);

    char buf[64]{};
    Require(sceSaveDataGetSaveDataMemory(1, buf, sizeof(buf), 0) == SaveDataErrorMemoryNotReady);
    Require(sceSaveDataSetSaveDataMemory(1, buf, sizeof(buf), 0) == SaveDataErrorMemoryNotReady);
}

void TestQuotaAllocationLimits() {
    Require(sceSaveDataSetupSaveDataMemory(1, 0, nullptr) == SaveDataErrorParameter);

    Require(sceSaveDataSetupSaveDataMemory(1, (32u * 1024u * 1024u) + 1u, nullptr) == SaveDataErrorParameter);

    Require(sceSaveDataSetupSaveDataMemory(1, 4096, nullptr) == 0);
}

void TestReadWriteAndOffsetBounds() {
    std::array<char, 64> readBuf{};
    readBuf.fill(0x55);
    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), readBuf.size(), 0) == 0);
    for (char byte : readBuf) {
        Require(byte == 0);
    }

    std::array<char, 64> writeBuf{};
    writeBuf.fill(0x7A);
    Require(sceSaveDataSetSaveDataMemory(1, writeBuf.data(), writeBuf.size(), 128) == 0);

    readBuf.fill(0);
    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), readBuf.size(), 128) == 0);
    for (char byte : readBuf) {
        Require(byte == 0x7A);
    }

    Require(sceSaveDataGetSaveDataMemory(1, nullptr, 64, 0) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(1, nullptr, 64, 0) == SaveDataErrorParameter);

    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), 1, 4096) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(1, writeBuf.data(), 1, 4096) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), 1, 5000) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(1, writeBuf.data(), 1, 5000) == SaveDataErrorParameter);

    Require(sceSaveDataGetSaveDataMemory(1, readBuf.data(), 64, 4050) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(1, writeBuf.data(), 64, 4050) == SaveDataErrorParameter);

    Require(sceSaveDataGetSaveDataMemory(1, nullptr, 0, 0) == 0);
    Require(sceSaveDataSetSaveDataMemory(1, nullptr, 0, 0) == 0);
}

void TestMemorySync() {
    Require(sceSaveDataSyncSaveDataMemory(nullptr) == SaveDataErrorParameter);

    MemorySyncParam sync{1, 0};
    Require(sceSaveDataSyncSaveDataMemory(&sync) == 0);

    MemorySyncParam unreadySync{2, 0};
    Require(sceSaveDataSyncSaveDataMemory(&unreadySync) == SaveDataErrorMemoryNotReady);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-memory-quota-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestUninitializedBounds();
    TestUnreadyMemoryBounds();
    TestQuotaAllocationLimits();
    TestReadWriteAndOffsetBounds();
    TestMemorySync();

    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
