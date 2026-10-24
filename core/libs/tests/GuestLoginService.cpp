#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceLoginServiceInitialize(void);
int APS5_VABI sceLoginServiceTerminate(void);
int APS5_VABI sceLoginServiceRequestDevices(void* param);
}

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceTerminate() == 0);
    unsigned char buffer[0x80]{};
    try {
        sceLoginServiceRequestDevices(buffer);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == "sceLoginServiceRequestDevices not implemented") return 0;
        std::fprintf(stderr, "sceLoginServiceRequestDevices threw: %s\n", error.what());
        return 1;
    }
    std::fprintf(stderr, "sceLoginServiceRequestDevices returned instead of reporting that it is not implemented\n");
    return 1;
}
