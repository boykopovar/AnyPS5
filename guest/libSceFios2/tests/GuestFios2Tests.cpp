#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);
constexpr int SCE_FIOS_ERROR_INVALID_PATH = static_cast<int>(0x80820002u);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosFileCancelSync(int handle);
int APS5_VABI sceFiosStatSync(const char* path, void* buf);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retNegativeHandle = sceFiosFileCancelSync(-1);
    Require(retNegativeHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retNegativeHandle != 0, "sceFiosFileCancelSync accepted negative handle");

    int retZeroHandle = sceFiosFileCancelSync(0);
    Require(retZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retZeroHandle != 0, "sceFiosFileCancelSync accepted zero handle");

    int retLargeHandle = sceFiosFileCancelSync(99999);
    Require(retLargeHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retLargeHandle != 0, "sceFiosFileCancelSync accepted large invalid handle");

    int retMaxHandle = sceFiosFileCancelSync(0x7FFFFFFF);
    Require(retMaxHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retMaxHandle != 0, "sceFiosFileCancelSync accepted max invalid handle");

    std::array<char, 128> statBuffer{};

    int retNullPathNullBuf = sceFiosStatSync(nullptr, nullptr);
    Require(retNullPathNullBuf == SCE_FIOS_ERROR_INVALID_PATH || retNullPathNullBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retNullPathNullBuf != 0, "sceFiosStatSync accepted null path with null buffer");

    int retNullPathValidBuf = sceFiosStatSync(nullptr, statBuffer.data());
    Require(retNullPathValidBuf == SCE_FIOS_ERROR_INVALID_PATH || retNullPathValidBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retNullPathValidBuf != 0, "sceFiosStatSync accepted null path with valid buffer");

    int retEmptyPathNullBuf = sceFiosStatSync("", nullptr);
    Require(retEmptyPathNullBuf == SCE_FIOS_ERROR_INVALID_PATH || retEmptyPathNullBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retEmptyPathNullBuf != 0, "sceFiosStatSync accepted empty path with null buffer");

    int retEmptyPathValidBuf = sceFiosStatSync("", statBuffer.data());
    Require(retEmptyPathValidBuf == SCE_FIOS_ERROR_INVALID_PATH || retEmptyPathValidBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retEmptyPathValidBuf != 0, "sceFiosStatSync accepted empty path with valid buffer");

    std::fprintf(stderr, "GuestFios2 cancel and stat bounds tests passed\n");
    return 0;
}
