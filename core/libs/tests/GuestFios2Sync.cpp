#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileSync(int handle);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidHandle = sceFiosFileSync(-1);
    Require(retInvalidHandle != 0, "sceFiosFileSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileSync(0);
    Require(retInvalidHandleZero != 0, "sceFiosFileSync accepted zero handle");

    int retInvalidHandleLarge = sceFiosFileSync(-100);
    Require(retInvalidHandleLarge != 0, "sceFiosFileSync accepted large negative handle");

    std::fprintf(stderr, "GuestFios2Sync tests passed\n");
    return 0;
}
