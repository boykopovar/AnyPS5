#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosFileSeekSync(int handle, std::int64_t offset, int whence);
int APS5_VABI sceFiosFileTellSync(int handle);
int APS5_VABI sceFiosFileCloseSync(int handle);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retSeekNeg = sceFiosFileSeekSync(-1, 0, 0);
    Require(retSeekNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retSeekNeg != 0, "sceFiosFileSeekSync accepted negative file handle");

    int retSeekZero = sceFiosFileSeekSync(0, 0, 0);
    Require(retSeekZero == SCE_FIOS_ERROR_INVALID_HANDLE || retSeekZero != 0, "sceFiosFileSeekSync accepted zero file handle");

    int retSeekLarge = sceFiosFileSeekSync(99999, 0, 0);
    Require(retSeekLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retSeekLarge != 0, "sceFiosFileSeekSync accepted out of range file handle");

    int retTellNeg = sceFiosFileTellSync(-1);
    Require(retTellNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retTellNeg != 0, "sceFiosFileTellSync accepted negative handle");

    int retTellZero = sceFiosFileTellSync(0);
    Require(retTellZero == SCE_FIOS_ERROR_INVALID_HANDLE || retTellZero != 0, "sceFiosFileTellSync accepted zero handle");

    int retTellLarge = sceFiosFileTellSync(99999);
    Require(retTellLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retTellLarge != 0, "sceFiosFileTellSync accepted out of range handle");

    int retCloseNeg = sceFiosFileCloseSync(-1);
    Require(retCloseNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retCloseNeg != 0, "sceFiosFileCloseSync accepted negative handle");

    int retCloseZero = sceFiosFileCloseSync(0);
    Require(retCloseZero == SCE_FIOS_ERROR_INVALID_HANDLE || retCloseZero != 0, "sceFiosFileCloseSync accepted zero handle");

    int retCloseLarge = sceFiosFileCloseSync(99999);
    Require(retCloseLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retCloseLarge != 0, "sceFiosFileCloseSync accepted out of range handle");

    return 0;
}
