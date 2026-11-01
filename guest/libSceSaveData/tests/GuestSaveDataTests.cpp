#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#ifndef APS5_VABI
#define APS5_VABI
#endif

struct SceSaveDataDirName {
    char data[32];
};

struct SceSaveDataTitleId {
    char data[10];
    char padding[6];
};

struct SaveDataDelete {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    std::uint32_t unused;
    std::uint8_t reserved[32];
    std::int32_t pad2;
};

struct SaveDataRestore {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint8_t reserved[32];
};

extern "C" {
int APS5_VABI sceSaveDataDelete(const SaveDataDelete* del);
int APS5_VABI sceSaveDataDeleteTransactionResource(std::int32_t resource);
int APS5_VABI sceSaveDataRestore(const SaveDataRestore* restore);
}

namespace {

constexpr int SaveDataErrorParameter = static_cast<int>(0x809f0000);
constexpr int SaveDataErrorNotInitialized = static_cast<int>(0x809f0001);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    Require(sceSaveDataDelete(nullptr) == SaveDataErrorParameter);

    SaveDataDelete nullDel{};
    Require(sceSaveDataDelete(&nullDel) == SaveDataErrorParameter);

    for (const std::int32_t userId : {-100, -1, 0, 1, 0x10000005, 0x20000000}) {
        SaveDataDelete invalidUserDel{};
        invalidUserDel.user_id = userId;
        invalidUserDel.dir_name = nullptr;
        Require(sceSaveDataDelete(&invalidUserDel) == SaveDataErrorParameter);
    }

    char unterminatedDelete[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminatedDelete, 'a', sizeof(unterminatedDelete));
    SaveDataDelete unterminatedDel{};
    unterminatedDel.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedDelete);
    Require(sceSaveDataDelete(&unterminatedDel) == SaveDataErrorParameter);

    for (const char fill : {'A', 'z', '0', '_', '%', 'x', '!'}) {
        SceSaveDataDirName patternDir{};
        std::memset(patternDir.data, fill, sizeof(patternDir.data));
        SaveDataDelete patternDel{};
        patternDel.dir_name = &patternDir;
        Require(sceSaveDataDelete(&patternDel) == SaveDataErrorParameter);
    }

    SceSaveDataDirName emptyDirDelete{};
    emptyDirDelete.data[0] = '\0';
    SaveDataDelete emptyDel{};
    emptyDel.dir_name = &emptyDirDelete;
    Require(sceSaveDataDelete(&emptyDel) == SaveDataErrorParameter);

    for (const char* invalidName : {"../victim", "", ".", "..", "../..", "kept/..", "a\\b", "c:d", "/root"}) {
        SceSaveDataDirName invalidDir{};
        std::strncpy(invalidDir.data, invalidName, sizeof(invalidDir.data));
        SaveDataDelete invalidDel{};
        invalidDel.dir_name = &invalidDir;
        Require(sceSaveDataDelete(&invalidDel) == SaveDataErrorParameter);
    }

    for (const std::int32_t resourceHandle : {-100, -1, 0, 1, 9999}) {
        const int resourceResult = sceSaveDataDeleteTransactionResource(resourceHandle);
        Require(resourceResult == SaveDataErrorParameter || resourceResult == SaveDataErrorNotInitialized);
    }

    Require(sceSaveDataRestore(nullptr) == SaveDataErrorParameter);

    SaveDataRestore nullRestore{};
    Require(sceSaveDataRestore(&nullRestore) == SaveDataErrorParameter);

    for (const std::int32_t userId : {-100, -1, 0, 1, 0x10000000, 0x10000005, 0x20000000}) {
        SaveDataRestore invalidUserRestore{};
        invalidUserRestore.user_id = userId;
        invalidUserRestore.dir_name = nullptr;
        Require(sceSaveDataRestore(&invalidUserRestore) == SaveDataErrorParameter);
    }

    char unterminatedRestore[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminatedRestore, 'b', sizeof(unterminatedRestore));
    SaveDataRestore unterminatedRes{};
    unterminatedRes.user_id = 0x10000000;
    unterminatedRes.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedRestore);
    Require(sceSaveDataRestore(&unterminatedRes) == SaveDataErrorParameter);

    for (const char fill : {'A', 'z', '0', '_', '%', 'x', '!'}) {
        SceSaveDataDirName patternDir{};
        std::memset(patternDir.data, fill, sizeof(patternDir.data));
        SaveDataRestore patternRes{};
        patternRes.user_id = 0x10000000;
        patternRes.dir_name = &patternDir;
        Require(sceSaveDataRestore(&patternRes) == SaveDataErrorParameter);
    }

    SceSaveDataDirName emptyDirRestore{};
    emptyDirRestore.data[0] = '\0';
    SaveDataRestore emptyRes{};
    emptyRes.user_id = 0x10000000;
    emptyRes.dir_name = &emptyDirRestore;
    Require(sceSaveDataRestore(&emptyRes) == SaveDataErrorParameter);

    for (const char* invalidName : {"../victim", "", ".", "..", "../..", "rest/..", "a\\b", "c:d", "/root"}) {
        SceSaveDataDirName invalidDir{};
        std::strncpy(invalidDir.data, invalidName, sizeof(invalidDir.data));
        SaveDataRestore invalidRes{};
        invalidRes.user_id = 0x10000000;
        invalidRes.dir_name = &invalidDir;
        Require(sceSaveDataRestore(&invalidRes) == SaveDataErrorParameter);
    }

    return 0;
}
