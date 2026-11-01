#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

using SaveDataSearchResult = SaveDataDirNameSearchResult;

extern "C" {
int APS5_VABI sceSaveDataSearchSaveData(const SceSaveDataDirName* dirName, SaveDataSearchResult* result);
}

namespace {

constexpr int SCE_SAVE_DATA_ERROR_PARAMETER = static_cast<int>(0x809f0000);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    SaveDataSearchResult result{};
    SceSaveDataDirName validDir{};
    std::strncpy(validDir.data, "valid_save_dir", sizeof(validDir.data));

    Require(sceSaveDataSearchSaveData(nullptr, nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);
    Require(sceSaveDataSearchSaveData(nullptr, &result) == SCE_SAVE_DATA_ERROR_PARAMETER);
    Require(sceSaveDataSearchSaveData(&validDir, nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);

    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    const auto* unterminatedDir = reinterpret_cast<const SceSaveDataDirName*>(unterminated);
    Require(sceSaveDataSearchSaveData(unterminatedDir, &result) == SCE_SAVE_DATA_ERROR_PARAMETER);
    Require(sceSaveDataSearchSaveData(unterminatedDir, nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    Require(sceSaveDataSearchSaveData(&emptyDir, &result) == SCE_SAVE_DATA_ERROR_PARAMETER);
    Require(sceSaveDataSearchSaveData(&emptyDir, nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);

    for (const char* invalidName : {".", "..", "invalid/dir", "invalid\\dir", "invalid:dir"}) {
        SceSaveDataDirName invalidDir{};
        std::strncpy(invalidDir.data, invalidName, sizeof(invalidDir.data));
        Require(sceSaveDataSearchSaveData(&invalidDir, &result) == SCE_SAVE_DATA_ERROR_PARAMETER);
        Require(sceSaveDataSearchSaveData(&invalidDir, nullptr) == SCE_SAVE_DATA_ERROR_PARAMETER);
    }

    return 0;
}
