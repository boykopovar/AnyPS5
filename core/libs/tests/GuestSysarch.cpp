#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sysarch(int number, void* args);
}

int main() {
    try {
        sysarch(129, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == "sysarch not implemented") return 0;
        std::fprintf(stderr, "sysarch threw: %s\n", error.what());
        return 1;
    }
    std::fprintf(stderr, "sysarch returned instead of reporting that it is not implemented\n");
    return 1;
}
