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
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorMemoryNotReady = -2137063406;

constexpr std::size_t MaxMemorySize = 32u * 1024u * 1024u;

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

void TestPreInitializationBounds() {
    Require(sceSaveDataSetupSaveDataMemory2(nullptr, nullptr) == SaveDataErrorParameter);

    SaveDataMemorySetup2 setup{};
    setup.user_id = 1001;
    setup.memory_size = 1024;
    SaveDataMemorySetupResult result{};
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == SaveDataErrorNotInitialized);

    Require(sceSaveDataGetSaveDataMemory2(nullptr) == SaveDataErrorParameter);

    SaveDataMemoryGet2 getParam{};
    getParam.user_id = 1001;
    Require(sceSaveDataGetSaveDataMemory2(&getParam) == SaveDataErrorNotInitialized);

    Require(sceSaveDataSetSaveDataMemory2(nullptr) == SaveDataErrorParameter);

    SaveDataMemorySet2 setParam{};
    setParam.user_id = 1001;
    Require(sceSaveDataSetSaveDataMemory2(&setParam) == SaveDataErrorNotInitialized);

    Require(sceSaveDataSyncSaveDataMemory(nullptr) == SaveDataErrorParameter);
    int syncDummy = 0;
    Require(sceSaveDataSyncSaveDataMemory(&syncDummy) == SaveDataErrorNotInitialized);

    char buffer[64]{};
    Require(sceSaveDataSetupSaveDataMemory(1001, 1024, nullptr) == SaveDataErrorNotInitialized);
    Require(sceSaveDataGetSaveDataMemory(1001, buffer, sizeof(buffer), 0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataSetSaveDataMemory(1001, buffer, sizeof(buffer), 0) == SaveDataErrorNotInitialized);
}

void TestSetupMemoryBounds() {
    SaveDataMemorySetupResult result{};
    Require(sceSaveDataSetupSaveDataMemory2(nullptr, &result) == SaveDataErrorParameter);

    SaveDataMemorySetup2 setupZero{};
    setupZero.user_id = 1001;
    setupZero.memory_size = 0;
    Require(sceSaveDataSetupSaveDataMemory2(&setupZero, &result) == SaveDataErrorParameter);

    SaveDataMemorySetup2 setupOversized{};
    setupOversized.user_id = 1001;
    setupOversized.memory_size = MaxMemorySize + 1;
    Require(sceSaveDataSetupSaveDataMemory2(&setupOversized, &result) == SaveDataErrorParameter);

    SaveDataMemorySetup2 setupMaxInt{};
    setupMaxInt.user_id = 1001;
    setupMaxInt.memory_size = static_cast<std::size_t>(-1);
    Require(sceSaveDataSetupSaveDataMemory2(&setupMaxInt, &result) == SaveDataErrorParameter);

    SaveDataMemorySetup2 validSetup{};
    validSetup.user_id = 1001;
    validSetup.slot_id = 0;
    validSetup.memory_size = 512;
    result.existed_memory_size = 12345;
    Require(sceSaveDataSetupSaveDataMemory2(&validSetup, &result) == SaveDataOk);
    Require(result.existed_memory_size == 0);

    Require(sceSaveDataSetupSaveDataMemory2(&validSetup, nullptr) == SaveDataOk);

    validSetup.memory_size = 1024;
    Require(sceSaveDataSetupSaveDataMemory2(&validSetup, &result) == SaveDataOk);
    Require(result.existed_memory_size == 512);

    SaveDataParam initParam{};
    std::memcpy(initParam.title, "InitTitle", sizeof("InitTitle"));
    SaveDataMemorySetup2 paramSetup{};
    paramSetup.user_id = 1001;
    paramSetup.slot_id = 1;
    paramSetup.memory_size = 256;
    paramSetup.option = 1;
    paramSetup.init_param = &initParam;
    Require(sceSaveDataSetupSaveDataMemory2(&paramSetup, &result) == SaveDataOk);
    Require(result.existed_memory_size == 0);
}

void TestGetMemoryBounds() {
    Require(sceSaveDataGetSaveDataMemory2(nullptr) == SaveDataErrorParameter);

    SaveDataMemoryGet2 unreadyGet{};
    unreadyGet.user_id = 9999;
    unreadyGet.slot_id = 0;
    Require(sceSaveDataGetSaveDataMemory2(&unreadyGet) == SaveDataErrorMemoryNotReady);

    unreadyGet.user_id = 1001;
    unreadyGet.slot_id = 7;
    Require(sceSaveDataGetSaveDataMemory2(&unreadyGet) == SaveDataErrorMemoryNotReady);

    SaveDataMemoryGet2 readyGet{};
    readyGet.user_id = 1001;
    readyGet.slot_id = 0;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataOk);

    SaveDataMemoryData zeroData{};
    zeroData.buf = nullptr;
    zeroData.buf_size = 0;
    zeroData.offset = 0;
    readyGet.data = &zeroData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataOk);

    SaveDataMemoryData nullBufData{};
    nullBufData.buf = nullptr;
    nullBufData.buf_size = 16;
    nullBufData.offset = 0;
    readyGet.data = &nullBufData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataErrorParameter);

    char readBuf[1024]{};
    SaveDataMemoryData offsetOverData{};
    offsetOverData.buf = readBuf;
    offsetOverData.buf_size = 1;
    offsetOverData.offset = 1025;
    readyGet.data = &offsetOverData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataErrorParameter);

    SaveDataMemoryData offsetAtLimitData{};
    offsetAtLimitData.buf = readBuf;
    offsetAtLimitData.buf_size = 1;
    offsetAtLimitData.offset = 1024;
    readyGet.data = &offsetAtLimitData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataErrorParameter);

    SaveDataMemoryData spanOverflowData{};
    spanOverflowData.buf = readBuf;
    spanOverflowData.buf_size = 30;
    spanOverflowData.offset = 1000;
    readyGet.data = &spanOverflowData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataErrorParameter);

    SaveDataMemoryData extremeData{};
    extremeData.buf = readBuf;
    extremeData.buf_size = 20;
    extremeData.offset = static_cast<std::size_t>(-1) - 10;
    readyGet.data = &extremeData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataErrorParameter);

    SaveDataIcon icon{};
    icon.data_size = 100;
    readyGet.icon = &icon;
    readyGet.data = nullptr;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataOk);
    Require(icon.data_size == 0);
    readyGet.icon = nullptr;

    SaveDataMemoryGet2 slot1Get{};
    slot1Get.user_id = 1001;
    slot1Get.slot_id = 1;
    SaveDataParam fetchedParam{};
    slot1Get.param = &fetchedParam;
    Require(sceSaveDataGetSaveDataMemory2(&slot1Get) == SaveDataOk);
    Require(std::strcmp(fetchedParam.title, "InitTitle") == 0);

    SaveDataMemoryData validBoundaryData{};
    validBoundaryData.buf = readBuf;
    validBoundaryData.buf_size = 1024;
    validBoundaryData.offset = 0;
    readyGet.data = &validBoundaryData;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataOk);

    validBoundaryData.buf_size = 1;
    validBoundaryData.offset = 1023;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataOk);

    validBoundaryData.buf_size = 0;
    validBoundaryData.offset = 1024;
    Require(sceSaveDataGetSaveDataMemory2(&readyGet) == SaveDataOk);
}

void TestSetMemoryBounds() {
    Require(sceSaveDataSetSaveDataMemory2(nullptr) == SaveDataErrorParameter);

    SaveDataMemorySet2 unreadySet{};
    unreadySet.user_id = 9999;
    unreadySet.slot_id = 0;
    Require(sceSaveDataSetSaveDataMemory2(&unreadySet) == SaveDataErrorMemoryNotReady);

    unreadySet.user_id = 1001;
    unreadySet.slot_id = 7;
    Require(sceSaveDataSetSaveDataMemory2(&unreadySet) == SaveDataErrorMemoryNotReady);

    SaveDataMemorySet2 readySet{};
    readySet.user_id = 1001;
    readySet.slot_id = 0;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataOk);

    SaveDataMemoryData nullBufData{};
    nullBufData.buf = nullptr;
    nullBufData.buf_size = 16;
    nullBufData.offset = 0;
    readySet.data = &nullBufData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataErrorParameter);

    char payload[64]{};
    std::memset(payload, 0xAB, sizeof(payload));

    SaveDataMemoryData offsetOverData{};
    offsetOverData.buf = payload;
    offsetOverData.buf_size = 1;
    offsetOverData.offset = 1025;
    readySet.data = &offsetOverData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataErrorParameter);

    SaveDataMemoryData offsetAtLimitData{};
    offsetAtLimitData.buf = payload;
    offsetAtLimitData.buf_size = 1;
    offsetAtLimitData.offset = 1024;
    readySet.data = &offsetAtLimitData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataErrorParameter);

    SaveDataMemoryData spanOverflowData{};
    spanOverflowData.buf = payload;
    spanOverflowData.buf_size = 40;
    spanOverflowData.offset = 1000;
    readySet.data = &spanOverflowData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataErrorParameter);

    SaveDataMemoryData extremeData{};
    extremeData.buf = payload;
    extremeData.buf_size = 20;
    extremeData.offset = static_cast<std::size_t>(-1) - 10;
    readySet.data = &extremeData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataErrorParameter);

    SaveDataMemoryData zeroData{};
    zeroData.buf = nullptr;
    zeroData.buf_size = 0;
    zeroData.offset = 0;
    readySet.data = &zeroData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataOk);

    std::array<SaveDataMemoryData, 2> atomicBatch{};
    char validWriteData[16]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    atomicBatch[0].buf = validWriteData;
    atomicBatch[0].buf_size = sizeof(validWriteData);
    atomicBatch[0].offset = 100;
    atomicBatch[1].buf = validWriteData;
    atomicBatch[1].buf_size = sizeof(validWriteData);
    atomicBatch[1].offset = 1020;
    readySet.data = atomicBatch.data();
    readySet.data_num = 2;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataErrorParameter);

    char verifyBuf[16]{};
    SaveDataMemoryData checkData{};
    checkData.buf = verifyBuf;
    checkData.buf_size = sizeof(verifyBuf);
    checkData.offset = 100;
    SaveDataMemoryGet2 checkGet{};
    checkGet.user_id = 1001;
    checkGet.slot_id = 0;
    checkGet.data = &checkData;
    Require(sceSaveDataGetSaveDataMemory2(&checkGet) == SaveDataOk);
    for (char byte : verifyBuf) {
        Require(byte == 0);
    }

    readySet.data = &atomicBatch[0];
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataOk);

    Require(sceSaveDataGetSaveDataMemory2(&checkGet) == SaveDataOk);
    Require(std::memcmp(verifyBuf, validWriteData, sizeof(verifyBuf)) == 0);

    char edgeByte = '\x7E';
    SaveDataMemoryData edgeData{};
    edgeData.buf = &edgeByte;
    edgeData.buf_size = 1;
    edgeData.offset = 1023;
    readySet.data = &edgeData;
    readySet.data_num = 1;
    Require(sceSaveDataSetSaveDataMemory2(&readySet) == SaveDataOk);

    char readEdge = 0;
    edgeData.buf = &readEdge;
    checkGet.data = &edgeData;
    Require(sceSaveDataGetSaveDataMemory2(&checkGet) == SaveDataOk);
    Require(readEdge == '\x7E');
}

void TestSyncMemoryBounds() {
    Require(sceSaveDataSyncSaveDataMemory(nullptr) == SaveDataErrorParameter);

    int syncParam = 0;
    Require(sceSaveDataSyncSaveDataMemory(&syncParam) == SaveDataOk);
}

void TestLegacyPs4MemoryBounds() {
    const std::int32_t legacyUser = 2002;

    Require(sceSaveDataSetupSaveDataMemory(legacyUser, 0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSetupSaveDataMemory(legacyUser, MaxMemorySize + 1, nullptr) == SaveDataErrorParameter);

    char sampleBuf[64]{};
    Require(sceSaveDataGetSaveDataMemory(legacyUser, sampleBuf, sizeof(sampleBuf), 0) == SaveDataErrorMemoryNotReady);
    Require(sceSaveDataSetSaveDataMemory(legacyUser, sampleBuf, sizeof(sampleBuf), 0) == SaveDataErrorMemoryNotReady);

    Require(sceSaveDataSetupSaveDataMemory(legacyUser, 512, nullptr) == SaveDataOk);

    Require(sceSaveDataGetSaveDataMemory(legacyUser, nullptr, 16, 0) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataMemory(legacyUser, sampleBuf, 16, 500) == SaveDataErrorParameter);
    Require(sceSaveDataGetSaveDataMemory(legacyUser, sampleBuf, 1, 512) == SaveDataErrorParameter);

    Require(sceSaveDataSetSaveDataMemory(legacyUser, nullptr, 16, 0) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(legacyUser, sampleBuf, 16, 500) == SaveDataErrorParameter);
    Require(sceSaveDataSetSaveDataMemory(legacyUser, sampleBuf, 1, 512) == SaveDataErrorParameter);

    char writePattern[32]{};
    for (std::size_t i = 0; i < sizeof(writePattern); ++i) {
        writePattern[i] = static_cast<char>(i + 42);
    }
    Require(sceSaveDataSetSaveDataMemory(legacyUser, writePattern, sizeof(writePattern), 64) == SaveDataOk);

    char readPattern[32]{};
    Require(sceSaveDataGetSaveDataMemory(legacyUser, readPattern, sizeof(readPattern), 64) == SaveDataOk);
    Require(std::memcmp(readPattern, writePattern, sizeof(readPattern)) == 0);
}

void TestUnimplementedMemoryVariants() {
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataRestoreLoadSaveDataMemory()); }, "sceSaveDataRestoreLoadSaveDataMemory");
}

void TestPostTerminationBounds() {
    Require(sceSaveDataSetupSaveDataMemory2(nullptr, nullptr) == SaveDataErrorParameter);

    SaveDataMemorySetup2 setup{};
    setup.user_id = 1001;
    setup.memory_size = 1024;
    SaveDataMemorySetupResult result{};
    Require(sceSaveDataSetupSaveDataMemory2(&setup, &result) == SaveDataErrorNotInitialized);

    Require(sceSaveDataGetSaveDataMemory2(nullptr) == SaveDataErrorParameter);

    SaveDataMemoryGet2 getParam{};
    getParam.user_id = 1001;
    Require(sceSaveDataGetSaveDataMemory2(&getParam) == SaveDataErrorNotInitialized);

    Require(sceSaveDataSetSaveDataMemory2(nullptr) == SaveDataErrorParameter);

    SaveDataMemorySet2 setParam{};
    setParam.user_id = 1001;
    Require(sceSaveDataSetSaveDataMemory2(&setParam) == SaveDataErrorNotInitialized);

    Require(sceSaveDataSyncSaveDataMemory(nullptr) == SaveDataErrorParameter);
    int syncDummy = 0;
    Require(sceSaveDataSyncSaveDataMemory(&syncDummy) == SaveDataErrorNotInitialized);

    char buffer[64]{};
    Require(sceSaveDataSetupSaveDataMemory(1001, 1024, nullptr) == SaveDataErrorNotInitialized);
    Require(sceSaveDataGetSaveDataMemory(1001, buffer, sizeof(buffer), 0) == SaveDataErrorNotInitialized);
    Require(sceSaveDataSetSaveDataMemory(1001, buffer, sizeof(buffer), 0) == SaveDataErrorNotInitialized);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-mem-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestPreInitializationBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestSetupMemoryBounds();
    TestGetMemoryBounds();
    TestSetMemoryBounds();
    TestSyncMemoryBounds();
    TestLegacyPs4MemoryBounds();
    TestUnimplementedMemoryVariants();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    TestPostTerminationBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
