#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileAllocateSync(int handle, std::int64_t size);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retInvalidHandle = sceFiosFileAllocateSync(-1, 1024);
    Require(retInvalidHandle != 0, "sceFiosFileAllocateSync accepted negative handle");

    int retInvalidHandleZero = sceFiosFileAllocateSync(0, 1024);
    Require(retInvalidHandleZero != 0, "sceFiosFileAllocateSync accepted zero handle");

    int retNegativeSize = sceFiosFileAllocateSync(1, -1);
    Require(retNegativeSize != 0, "sceFiosFileAllocateSync accepted negative size");

    int retNegativeSizeLarge = sceFiosFileAllocateSync(1, -4096);
    Require(retNegativeSizeLarge != 0, "sceFiosFileAllocateSync accepted negative size large");

    std::fprintf(stderr, "GuestFios2Allocate tests passed\n");
    return 0;
}
