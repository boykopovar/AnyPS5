#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileTellSync(int handle);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidHandle = sceFiosFileTellSync(-1);
    Require(retInvalidHandle != 0, "sceFiosFileTellSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileTellSync(0);
    Require(retInvalidHandleZero != 0, "sceFiosFileTellSync accepted zero handle");

    int retInvalidHandleLarge = sceFiosFileTellSync(-100);
    Require(retInvalidHandleLarge != 0, "sceFiosFileTellSync accepted large negative handle");

    std::fprintf(stderr, "GuestFios2Tell tests passed\n");
    return 0;
}
