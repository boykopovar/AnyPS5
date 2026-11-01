#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);

extern "C" {
int APS5_VABI sceFiosFileFlushSync(int handle);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidHandle = sceFiosFileFlushSync(-1);
    Require(retInvalidHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidHandle != 0, "sceFiosFileFlushSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileFlushSync(0);
    Require(retInvalidHandleZero == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidHandleZero != 0, "sceFiosFileFlushSync accepted zero handle");

    int retInvalidHandleLarge = sceFiosFileFlushSync(-100);
    Require(retInvalidHandleLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidHandleLarge != 0, "sceFiosFileFlushSync accepted large negative handle");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
