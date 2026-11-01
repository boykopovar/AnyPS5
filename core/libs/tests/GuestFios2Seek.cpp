#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileSeekSync(int handle, std::int64_t offset, int whence);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidWhenceNegative = sceFiosFileSeekSync(1, 0, -1);
    Require(retInvalidWhenceNegative != 0, "sceFiosFileSeekSync accepted negative whence");

    int retInvalidWhenceOob = sceFiosFileSeekSync(1, 0, 3);
    Require(retInvalidWhenceOob != 0, "sceFiosFileSeekSync accepted out-of-bounds whence");

    int retInvalidWhenceLarge = sceFiosFileSeekSync(1, 0, 999);
    Require(retInvalidWhenceLarge != 0, "sceFiosFileSeekSync accepted large whence");

    int retInvalidHandle = sceFiosFileSeekSync(-1, 0, 0);
    Require(retInvalidHandle != 0, "sceFiosFileSeekSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileSeekSync(0, 0, 0);
    Require(retInvalidHandleZero != 0, "sceFiosFileSeekSync accepted zero handle");

    std::fprintf(stderr, "GuestFios2Seek tests passed\n");
    return 0;
}
