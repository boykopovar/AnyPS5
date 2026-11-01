#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdint>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorMemoryNotReady = -2137063415;

}

int main() {
    if (sceSaveDataSyncSaveDataMemory(nullptr) != SaveDataErrorNotInitialized) {
        std::fprintf(stderr, "sceSaveDataSyncSaveDataMemory(nullptr) before init did not return NOT_INITIALIZED\n");
        return 1;
    }
    if (sceSaveDataInitialize3(nullptr) != 0) {
        std::fprintf(stderr, "SaveData initialization failed\n");
        return 1;
    }
    if (sceSaveDataSyncSaveDataMemory(nullptr) != SaveDataErrorParameter) {
        std::fprintf(stderr, "sceSaveDataSyncSaveDataMemory(nullptr) did not return PARAMETER\n");
        sceSaveDataTerminate();
        return 1;
    }
    const std::uint32_t invalidSlotParam[2] = {0, 0xffffffffu};
    if (sceSaveDataSyncSaveDataMemory(invalidSlotParam) != SaveDataErrorMemoryNotReady) {
        std::fprintf(stderr, "sceSaveDataSyncSaveDataMemory with invalid/unmounted slot did not return MEMORY_NOT_READY\n");
        sceSaveDataTerminate();
        return 1;
    }
    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData termination failed\n");
        return 1;
    }
    return 0;
}
