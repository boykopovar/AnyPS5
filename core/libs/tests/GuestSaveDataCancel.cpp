#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdint>
#include <initializer_list>

namespace {

constexpr int SaveDataErrorNotInitialized = -2137063423;

bool ReportsSuccess(std::uint32_t argument) {
    const int result = sceSaveDataCancel(argument);
    if (result != 0) {
        std::fprintf(stderr, "sceSaveDataCancel(%#x) after initialization did not report success: %#x\n", argument, static_cast<unsigned>(result));
        return false;
    }
    return true;
}

}

int main() {
    for (const std::uint32_t argument : {0u, 1u, 2u, 0xffffffffu}) {
        if (sceSaveDataCancel(argument) != SaveDataErrorNotInitialized) {
            std::fprintf(stderr, "sceSaveDataCancel(%#x) before initialization did not report not initialized\n", argument);
            return 1;
        }
    }
    if (sceSaveDataInitialize3(nullptr) != 0) {
        std::fprintf(stderr, "SaveData did not initialize\n");
        return 1;
    }
    if (!ReportsSuccess(0)) return 1;

    if (sceSaveDataInitialize3(nullptr) != 0) {
        std::fprintf(stderr, "SaveData did not initialize nested session\n");
        return 1;
    }
    if (!ReportsSuccess(1)) return 1;

    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData nested session did not terminate\n");
        return 1;
    }
    if (!ReportsSuccess(2)) return 1;

    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData did not terminate\n");
        return 1;
    }
    for (const std::uint32_t argument : {0u, 1u, 2u, 0x1000u, 0xffffffffu}) {
        if (sceSaveDataCancel(argument) != SaveDataErrorNotInitialized) {
            std::fprintf(stderr, "sceSaveDataCancel(%#x) after final termination did not report not initialized\n", argument);
            return 1;
        }
    }
    return 0;
}
