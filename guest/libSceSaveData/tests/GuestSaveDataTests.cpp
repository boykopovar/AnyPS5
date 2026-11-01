#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorMemoryNotReady = -2137063406;

struct SceSaveDataTitleId {
    char data[10];
    char pad[6];
};

struct SceSaveDataDirName {
    char data[32];
};

struct SaveDataBackup {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint8_t reserved[32];
};

struct SaveDataMountPoint {
    char data[16];
};

struct SaveDataIcon {
    void* buf;
    std::size_t buf_size;
    std::size_t data_size;
    std::uint8_t reserved[32];
};

struct SaveDataParam {
    char title[128];
    char sub_title[128];
    char detail[1024];
    std::uint32_t user_param;
    std::int32_t pad;
    std::int64_t mtime;
    std::uint8_t reserved[32];
};

struct SaveDataMemoryData {
    void* buf;
    std::size_t buf_size;
    std::size_t offset;
    std::uint8_t reserved[32];
};

struct SaveDataMemorySetup2 {
    std::uint32_t option;
    std::int32_t user_id;
    std::size_t memory_size;
    std::size_t icon_memory_size;
    const SaveDataParam* init_param;
    const SaveDataIcon* init_icon;
    std::uint32_t slot_id;
    std::uint8_t reserved[20];
};

struct SaveDataMemorySetupResult {
    std::size_t existed_memory_size;
    std::uint8_t reserved[16];
};

struct SaveDataMemoryGet2 {
    std::int32_t user_id;
    std::uint8_t padding[4];
    SaveDataMemoryData* data;
    SaveDataParam* param;
    SaveDataIcon* icon;
    std::uint32_t slot_id;
    std::uint8_t reserved[28];
};

struct SaveDataMemorySet2 {
    std::int32_t user_id;
    std::uint8_t padding[4];
    const SaveDataMemoryData* data;
    const SaveDataParam* param;
    const SaveDataIcon* icon;
    std::uint32_t data_num;
    std::uint32_t slot_id;
    std::uint8_t reserved[24];
};

extern "C" {
int sceSaveDataInitialize3(const void* init);
int sceSaveDataBackup(const SaveDataBackup* backup);
int sceSaveDataLoadIcon(const SaveDataMountPoint* mountPoint, SaveDataIcon* icon);
int sceSaveDataSaveIcon(const SaveDataMountPoint* mountPoint, const SaveDataIcon* icon);
int sceSaveDataSaveIconByPath(const SaveDataMountPoint* mountPoint, const char* path);
int sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setupParam, SaveDataMemorySetupResult* result);
int sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* getParam);
int sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* setParam);
int sceSaveDataSetupSaveDataMemory(std::int32_t userId, std::size_t memorySize, const SaveDataParam* param);
int sceSaveDataGetSaveDataMemory(std::int32_t userId, void* buf, std::size_t bufSize, std::int64_t offset);
int sceSaveDataSetSaveDataMemory(std::int32_t userId, void* buf, std::size_t bufSize, std::int64_t offset);
}

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what.c_str());
    ++failures;
}

void CheckSaveDataBackupBounds() {
    Check(sceSaveDataBackup(nullptr) == SaveDataErrorParameter, "backup null param returns PARAMETER");

    SaveDataBackup nullDirBackup{};
    Check(sceSaveDataBackup(&nullDirBackup) == SaveDataErrorParameter, "backup null dir_name returns PARAMETER");

    for (const std::int32_t userId : {-100, -1, 0, 1, 0x10000000, 0x20000000}) {
        SaveDataBackup invalidUserBackup{};
        invalidUserBackup.user_id = userId;
        invalidUserBackup.dir_name = nullptr;
        Check(sceSaveDataBackup(&invalidUserBackup) == SaveDataErrorParameter, "backup invalid user null dir returns PARAMETER");
    }

    char unterminatedBuffer[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminatedBuffer, 'a', sizeof(unterminatedBuffer));
    SaveDataBackup unterminatedBackup{};
    unterminatedBackup.user_id = 0x10000000;
    unterminatedBackup.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedBuffer);
    Check(sceSaveDataBackup(&unterminatedBackup) == SaveDataErrorParameter, "backup unterminated dir_name returns PARAMETER");

    for (const char fill : {'B', 'Z', '9', '_', 'x', '!'}) {
        SceSaveDataDirName patternDir{};
        std::memset(patternDir.data, fill, sizeof(patternDir.data));
        SaveDataBackup patternBackup{};
        patternBackup.user_id = 0x10000000;
        patternBackup.dir_name = &patternDir;
        Check(sceSaveDataBackup(&patternBackup) == SaveDataErrorParameter, "backup filled unterminated dir_name returns PARAMETER");
    }
}

void CheckSaveDataIconBounds() {
    SaveDataIcon icon{};
    icon.buf = nullptr;
    icon.buf_size = 0;
    icon.data_size = 0;

    SaveDataMountPoint point{};
    std::memcpy(point.data, "test_mount", sizeof("test_mount"));

    Check(sceSaveDataLoadIcon(nullptr, &icon) == SaveDataErrorParameter, "loadIcon null point returns PARAMETER");
    Check(sceSaveDataLoadIcon(nullptr, nullptr) == SaveDataErrorParameter, "loadIcon all null returns PARAMETER");

    SaveDataMountPoint zeroPoint{};
    Check(sceSaveDataLoadIcon(&zeroPoint, &icon) == SaveDataErrorNotMounted, "loadIcon zeroed point returns NOT_MOUNTED");
    Check(sceSaveDataLoadIcon(&zeroPoint, nullptr) == SaveDataErrorNotMounted, "loadIcon zeroed point null icon returns NOT_MOUNTED");
    Check(sceSaveDataLoadIcon(&point, &icon) == SaveDataErrorNotMounted, "loadIcon unmounted point returns NOT_MOUNTED");

    SaveDataMountPoint invalidSlot{};
    std::memcpy(invalidSlot.data, "/savedata99", sizeof("/savedata99"));
    Check(sceSaveDataLoadIcon(&invalidSlot, &icon) == SaveDataErrorNotMounted, "loadIcon invalid slot returns NOT_MOUNTED");

    SaveDataMountPoint overflowSlot{};
    std::memcpy(overflowSlot.data, "overflow_mount", sizeof("overflow_mount"));
    Check(sceSaveDataLoadIcon(&overflowSlot, &icon) == SaveDataErrorNotMounted, "loadIcon overflow slot returns NOT_MOUNTED");

    Check(sceSaveDataSaveIcon(nullptr, &icon) == SaveDataErrorParameter, "saveIcon null point returns PARAMETER");
    Check(sceSaveDataSaveIcon(nullptr, nullptr) == SaveDataErrorParameter, "saveIcon all null returns PARAMETER");
    Check(sceSaveDataSaveIcon(&zeroPoint, &icon) == SaveDataErrorNotMounted, "saveIcon zeroed point returns NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(&zeroPoint, nullptr) == SaveDataErrorNotMounted, "saveIcon zeroed point null icon returns NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(&point, &icon) == SaveDataErrorNotMounted, "saveIcon unmounted point returns NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(&invalidSlot, &icon) == SaveDataErrorNotMounted, "saveIcon invalid slot returns NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(&overflowSlot, &icon) == SaveDataErrorNotMounted, "saveIcon overflow slot returns NOT_MOUNTED");
}

void CheckSaveDataIconMemoryBounds() {
    SaveDataMemorySetupResult setupResult{};
    SaveDataMemorySetup2 setupParam{};
    SaveDataMemoryGet2 getParam{};
    SaveDataMemorySet2 setParam{};

    Check(sceSaveDataSetupSaveDataMemory2(nullptr, &setupResult) == SaveDataErrorParameter, "setupMemory2 null param returns PARAMETER");
    Check(sceSaveDataSetupSaveDataMemory2(nullptr, nullptr) == SaveDataErrorParameter, "setupMemory2 all null returns PARAMETER");
    Check(sceSaveDataGetSaveDataMemory2(nullptr) == SaveDataErrorParameter, "getMemory2 null param returns PARAMETER");
    Check(sceSaveDataSetSaveDataMemory2(nullptr) == SaveDataErrorParameter, "setMemory2 null param returns PARAMETER");

    setupParam.user_id = 0x10000000;
    setupParam.memory_size = 1024;
    Check(sceSaveDataSetupSaveDataMemory2(&setupParam, &setupResult) == SaveDataErrorNotInitialized, "setupMemory2 uninitialized returns NOT_INITIALIZED");
    getParam.user_id = 0x10000000;
    Check(sceSaveDataGetSaveDataMemory2(&getParam) == SaveDataErrorNotInitialized, "getMemory2 uninitialized returns NOT_INITIALIZED");
    setParam.user_id = 0x10000000;
    Check(sceSaveDataSetSaveDataMemory2(&setParam) == SaveDataErrorNotInitialized, "setMemory2 uninitialized returns NOT_INITIALIZED");

    sceSaveDataInitialize3(nullptr);

    setupParam.memory_size = 0;
    Check(sceSaveDataSetupSaveDataMemory2(&setupParam, &setupResult) == SaveDataErrorParameter, "setupMemory2 zero size returns PARAMETER");

    constexpr std::size_t maxMemorySize = 32u * 1024u * 1024u;
    setupParam.memory_size = maxMemorySize + 1u;
    Check(sceSaveDataSetupSaveDataMemory2(&setupParam, &setupResult) == SaveDataErrorParameter, "setupMemory2 oversized returns PARAMETER");

    setupParam.memory_size = static_cast<std::size_t>(-1);
    Check(sceSaveDataSetupSaveDataMemory2(&setupParam, &setupResult) == SaveDataErrorParameter, "setupMemory2 max size_t returns PARAMETER");

    Check(sceSaveDataSetupSaveDataMemory(0x10000000, 0, nullptr) == SaveDataErrorParameter, "setupMemory zero size returns PARAMETER");
    Check(sceSaveDataSetupSaveDataMemory(0x10000000, maxMemorySize + 1u, nullptr) == SaveDataErrorParameter, "setupMemory oversized returns PARAMETER");

    const int getRc = sceSaveDataGetSaveDataMemory(0x10000000, nullptr, 1024, 0);
    Check(getRc == SaveDataErrorParameter || getRc == SaveDataErrorMemoryNotReady, "getMemory null buf returns error");

    const int setRc = sceSaveDataSetSaveDataMemory(0x10000000, nullptr, 1024, 0);
    Check(setRc == SaveDataErrorParameter || setRc == SaveDataErrorMemoryNotReady, "setMemory null buf returns error");
}

}

int main() {
    CheckSaveDataBackupBounds();
    CheckSaveDataIconBounds();
    CheckSaveDataIconMemoryBounds();

    if (failures != 0) {
        return 1;
    }
    std::printf("all savedata backup and icon memory negative bounds tests passed\n");
    return 0;
}
