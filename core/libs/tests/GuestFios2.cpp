#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x8082000Bu);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

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
    int retNegHandle = sceFiosFileSeekSync(-1, 0, 0);
    Require(retNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retNegHandle != 0, "sceFiosFileSeekSync accepted negative file handle");

    int retZeroHandle = sceFiosFileSeekSync(0, 0, 0);
    Require(retZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retZeroHandle != 0, "sceFiosFileSeekSync accepted zero file handle");

    int retNegOffset = sceFiosFileSeekSync(1, -1, 0);
    Require(retNegOffset == SCE_FIOS_ERROR_INVALID_ARGUMENT || retNegOffset != 0, "sceFiosFileSeekSync accepted negative offset");

    int retBothInvalid = sceFiosFileSeekSync(-1, -1, 0);
    Require(retBothInvalid == SCE_FIOS_ERROR_INVALID_HANDLE || retBothInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retBothInvalid != 0, "sceFiosFileSeekSync accepted invalid file handle and negative offset");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
