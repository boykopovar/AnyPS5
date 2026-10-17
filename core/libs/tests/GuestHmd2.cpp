#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceHmd2Initialize(const void* param);
int APS5_VABI sceHmd2Open();
int APS5_VABI sceHmd2GazeActivateGazeTracking();
int APS5_VABI sceHmd2ReprojectionGetCurrentOutput();
int APS5_VABI sceHmd2SetFieldOfView();
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "HMD2: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const std::uint8_t param[16]{};
    Require(sceHmd2Initialize(param) == static_cast<std::int32_t>(0x81110016), "initialization did not report the unsupported feature");
    Require(sceHmd2Initialize(nullptr) == static_cast<std::int32_t>(0x81110016), "initialization without a param did not report the unsupported feature");
    bool threw = false;
    try {
        sceHmd2Open();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "open after failed initialization did not throw");
    threw = false;
    try {
        sceHmd2GazeActivateGazeTracking();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "gaze activation did not throw");
    threw = false;
    try {
        sceHmd2ReprojectionGetCurrentOutput();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "reprojection get current output did not throw");
    threw = false;
    try {
        sceHmd2SetFieldOfView();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "set field of view did not throw");
    std::puts("HMD2 tests passed");
    return 0;
}
