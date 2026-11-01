#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);
constexpr int SCE_FIOS_ERROR_INVALID_PATH = static_cast<int>(0x80820002u);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosStatSync(const void* attr, const char* path, void* stat);
int APS5_VABI sceFiosFHStatSync(const void* attr, int handle, void* stat);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    char statBuffer[128]{};

    int retStatNullPtr = sceFiosStatSync(nullptr, "/app0/test", nullptr);
    Require(retStatNullPtr == SCE_FIOS_ERROR_INVALID_ARGUMENT || retStatNullPtr != 0, "sceFiosStatSync accepted null stat pointer");

    int retStatNullPath = sceFiosStatSync(nullptr, nullptr, statBuffer);
    Require(retStatNullPath == SCE_FIOS_ERROR_INVALID_PATH || retStatNullPath == SCE_FIOS_ERROR_INVALID_ARGUMENT || retStatNullPath != 0, "sceFiosStatSync accepted null path pointer");

    int retStatBothNull = sceFiosStatSync(nullptr, nullptr, nullptr);
    Require(retStatBothNull == SCE_FIOS_ERROR_INVALID_PATH || retStatBothNull == SCE_FIOS_ERROR_INVALID_ARGUMENT || retStatBothNull != 0, "sceFiosStatSync accepted null path and null stat");

    int retStatEmpty = sceFiosStatSync(nullptr, "", statBuffer);
    Require(retStatEmpty == SCE_FIOS_ERROR_INVALID_PATH || retStatEmpty == SCE_FIOS_ERROR_INVALID_ARGUMENT || retStatEmpty != 0, "sceFiosStatSync accepted empty path");

    int retFHStatNeg = sceFiosFHStatSync(nullptr, -1, statBuffer);
    Require(retFHStatNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retFHStatNeg != 0, "sceFiosFHStatSync accepted negative file handle");

    int retFHStatZero = sceFiosFHStatSync(nullptr, 0, statBuffer);
    Require(retFHStatZero == SCE_FIOS_ERROR_INVALID_HANDLE || retFHStatZero != 0, "sceFiosFHStatSync accepted zero file handle");

    int retFHStatLarge = sceFiosFHStatSync(nullptr, 99999, statBuffer);
    Require(retFHStatLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retFHStatLarge != 0, "sceFiosFHStatSync accepted out of range file handle");

    int retFHStatNullPtr = sceFiosFHStatSync(nullptr, 1, nullptr);
    Require(retFHStatNullPtr == SCE_FIOS_ERROR_INVALID_ARGUMENT || retFHStatNullPtr != 0, "sceFiosFHStatSync accepted null stat pointer");

    int retFHStatBothInvalid = sceFiosFHStatSync(nullptr, -1, nullptr);
    Require(retFHStatBothInvalid == SCE_FIOS_ERROR_INVALID_HANDLE || retFHStatBothInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retFHStatBothInvalid != 0, "sceFiosFHStatSync accepted null stat pointer and negative handle");

    return 0;
}
