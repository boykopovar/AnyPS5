#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceSaveDataCancel();

int main() {
    try {
        sceSaveDataCancel();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == "sceSaveDataCancel not implemented") return 0;
        std::fprintf(stderr, "sceSaveDataCancel threw: %s\n", error.what());
        return 1;
    }
    std::fprintf(stderr, "sceSaveDataCancel returned instead of reporting that it is not implemented\n");
    return 1;
}
