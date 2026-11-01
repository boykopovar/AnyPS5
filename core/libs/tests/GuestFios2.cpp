#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x8082000Bu);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

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
    int retNegHandle = sceFiosFileTruncateSync(-1, 0);
    Require(retNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retNegHandle != 0, "sceFiosFileTruncateSync accepted negative file handle");

    int retZeroHandle = sceFiosFileTruncateSync(0, 0);
    Require(retZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retZeroHandle != 0, "sceFiosFileTruncateSync accepted zero file handle");

    int retNegLength = sceFiosFileTruncateSync(1, -1);
    Require(retNegLength == SCE_FIOS_ERROR_INVALID_ARGUMENT || retNegLength != 0, "sceFiosFileTruncateSync accepted negative truncate length");

    int retBothInvalid = sceFiosFileTruncateSync(-1, -1);
    Require(retBothInvalid == SCE_FIOS_ERROR_INVALID_HANDLE || retBothInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retBothInvalid != 0, "sceFiosFileTruncateSync accepted invalid file handle and negative truncate length");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
