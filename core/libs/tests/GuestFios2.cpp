#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);

extern "C" {
int APS5_VABI sceFiosFileGetSizeSync(int handle);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidHandle = sceFiosFileGetSizeSync(-1);
    Require(retInvalidHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidHandle != 0, "sceFiosFileGetSizeSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileGetSizeSync(0);
    Require(retInvalidHandleZero == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidHandleZero != 0, "sceFiosFileGetSizeSync accepted zero handle");

    int retInvalidHandleLarge = sceFiosFileGetSizeSync(99999);
    Require(retInvalidHandleLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidHandleLarge != 0, "sceFiosFileGetSizeSync accepted large handle");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
