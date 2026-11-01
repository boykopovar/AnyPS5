#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);

extern "C" {
int APS5_VABI sceFiosFileCloseSync(int handle);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidNegative = sceFiosFileCloseSync(-1);
    Require(retInvalidNegative == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidNegative != 0, "sceFiosFileCloseSync accepted negative handle");

    int retInvalidZero = sceFiosFileCloseSync(0);
    Require(retInvalidZero == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidZero != 0, "sceFiosFileCloseSync accepted zero handle");

    int retInvalidLarge = sceFiosFileCloseSync(99999);
    Require(retInvalidLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retInvalidLarge != 0, "sceFiosFileCloseSync accepted out of range handle");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
