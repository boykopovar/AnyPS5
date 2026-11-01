#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>

namespace {
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorParameter = -2137063424;
}

int main() {
    if (sceSaveDataInitialize3(nullptr) != 0) {
        std::fprintf(stderr, "SaveData did not initialize\n");
        return 1;
    }

    SaveDataMemoryGet2 get{};
    get.user_id = 1;
    for (const std::int32_t slotId : {-1, 16, 100, -100}) {
        get.slot_id = static_cast<std::uint32_t>(slotId);
        if (slotId < 0 || slotId >= 16) {
            if (sceSaveDataGetSaveDataMemory2(&get) != SaveDataErrorParameter) {
                std::fprintf(stderr, "sceSaveDataGetSaveDataMemory2 with invalid slot ID %d did not return parameter error\n", slotId);
                return 1;
            }
        }
    }

    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData did not terminate\n");
        return 1;
    }
    return 0;
}
