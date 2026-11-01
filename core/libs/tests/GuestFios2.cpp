#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x8082000Bu);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosFileWriteSync(int handle, const void* buffer, std::size_t size);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    const char validBuffer[64]{"data"};

    int retNegHandle = sceFiosFileWriteSync(-1, validBuffer, sizeof(validBuffer));
    Require(retNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retNegHandle != 0, "sceFiosFileWriteSync accepted negative file handle");

    int retZeroHandle = sceFiosFileWriteSync(0, validBuffer, sizeof(validBuffer));
    Require(retZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retZeroHandle != 0, "sceFiosFileWriteSync accepted zero file handle");

    int retNullBuf = sceFiosFileWriteSync(1, nullptr, sizeof(validBuffer));
    Require(retNullBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retNullBuf != 0, "sceFiosFileWriteSync accepted null buffer pointer");

    int retBothInvalid = sceFiosFileWriteSync(-1, nullptr, sizeof(validBuffer));
    Require(retBothInvalid == SCE_FIOS_ERROR_INVALID_HANDLE || retBothInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retBothInvalid != 0, "sceFiosFileWriteSync accepted invalid file handle and null buffer pointer");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}
