#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_PATH = static_cast<int>(0x80820002u);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosDirectoryCreateSync(const void* attr, const char* path);
int APS5_VABI sceFiosDirectoryDeleteSync(const void* attr, const char* path);
int APS5_VABI sceFiosArchiveMountSync(const void* attr, void* fh, const char* archivePath, const char* mountPath, void* pBuffer, std::size_t bufferSize);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retCreateNull = sceFiosDirectoryCreateSync(nullptr, nullptr);
    Require(retCreateNull == SCE_FIOS_ERROR_INVALID_PATH || retCreateNull == SCE_FIOS_ERROR_INVALID_ARGUMENT || retCreateNull != 0, "sceFiosDirectoryCreateSync accepted null path");

    int retCreateEmpty = sceFiosDirectoryCreateSync(nullptr, "");
    Require(retCreateEmpty == SCE_FIOS_ERROR_INVALID_PATH || retCreateEmpty == SCE_FIOS_ERROR_INVALID_ARGUMENT || retCreateEmpty != 0, "sceFiosDirectoryCreateSync accepted empty path");

    int retCreateInvalid = sceFiosDirectoryCreateSync(nullptr, "invalid:////?");
    Require(retCreateInvalid == SCE_FIOS_ERROR_INVALID_PATH || retCreateInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retCreateInvalid != 0, "sceFiosDirectoryCreateSync accepted invalid path");

    int retDeleteNull = sceFiosDirectoryDeleteSync(nullptr, nullptr);
    Require(retDeleteNull == SCE_FIOS_ERROR_INVALID_PATH || retDeleteNull == SCE_FIOS_ERROR_INVALID_ARGUMENT || retDeleteNull != 0, "sceFiosDirectoryDeleteSync accepted null path");

    int retDeleteEmpty = sceFiosDirectoryDeleteSync(nullptr, "");
    Require(retDeleteEmpty == SCE_FIOS_ERROR_INVALID_PATH || retDeleteEmpty == SCE_FIOS_ERROR_INVALID_ARGUMENT || retDeleteEmpty != 0, "sceFiosDirectoryDeleteSync accepted empty path");

    int retDeleteInvalid = sceFiosDirectoryDeleteSync(nullptr, "invalid:////?");
    Require(retDeleteInvalid == SCE_FIOS_ERROR_INVALID_PATH || retDeleteInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retDeleteInvalid != 0, "sceFiosDirectoryDeleteSync accepted invalid path");

    int retMountNull = sceFiosArchiveMountSync(nullptr, nullptr, nullptr, nullptr, nullptr, 0);
    Require(retMountNull == SCE_FIOS_ERROR_INVALID_PATH || retMountNull == SCE_FIOS_ERROR_INVALID_ARGUMENT || retMountNull != 0, "sceFiosArchiveMountSync accepted null archive and mount paths");

    int retMountEmpty = sceFiosArchiveMountSync(nullptr, nullptr, "", "/app0", nullptr, 0);
    Require(retMountEmpty == SCE_FIOS_ERROR_INVALID_PATH || retMountEmpty == SCE_FIOS_ERROR_INVALID_ARGUMENT || retMountEmpty != 0, "sceFiosArchiveMountSync accepted empty archive path");

    int retMountEmptyDest = sceFiosArchiveMountSync(nullptr, nullptr, "/data/test.psarc", "", nullptr, 0);
    Require(retMountEmptyDest == SCE_FIOS_ERROR_INVALID_PATH || retMountEmptyDest == SCE_FIOS_ERROR_INVALID_ARGUMENT || retMountEmptyDest != 0, "sceFiosArchiveMountSync accepted empty mount path");

    int retMountInvalid = sceFiosArchiveMountSync(nullptr, nullptr, "invalid:////?", "invalid:////?", nullptr, 0);
    Require(retMountInvalid == SCE_FIOS_ERROR_INVALID_PATH || retMountInvalid == SCE_FIOS_ERROR_INVALID_ARGUMENT || retMountInvalid != 0, "sceFiosArchiveMountSync accepted invalid paths");

    return 0;
}
