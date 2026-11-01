#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileTruncateSync(int handle, std::int64_t length);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidHandle = sceFiosFileTruncateSync(-1, 1024);
    Require(retInvalidHandle != 0, "sceFiosFileTruncateSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileTruncateSync(0, 1024);
    Require(retInvalidHandleZero != 0, "sceFiosFileTruncateSync accepted zero handle");

    int retNegativeLength = sceFiosFileTruncateSync(1, -1);
    Require(retNegativeLength != 0, "sceFiosFileTruncateSync accepted negative length");

    int retNegativeLengthLarge = sceFiosFileTruncateSync(1, -4096);
    Require(retNegativeLengthLarge != 0, "sceFiosFileTruncateSync accepted negative length large");

    std::fprintf(stderr, "GuestFios2Truncate tests passed\n");
    return 0;
}
