#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int SaveDataErrorNotInitialized = -2137063423;

bool ReportsNotImplemented(std::uint32_t argument) {
    try {
        sceSaveDataCancel(argument);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == "sceSaveDataCancel not implemented") return true;
        std::fprintf(stderr, "sceSaveDataCancel threw: %s\n", error.what());
        return false;
    }
    std::fprintf(stderr, "sceSaveDataCancel returned instead of reporting that it is not implemented\n");
    return false;
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
    const bool reported = ReportsNotImplemented(0);
    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData did not terminate\n");
        return 1;
    }
    if (!reported) return 1;
    if (sceSaveDataCancel(0) != SaveDataErrorNotInitialized) {
        std::fprintf(stderr, "sceSaveDataCancel after termination did not report not initialized\n");
        return 1;
    }
    return 0;
}
