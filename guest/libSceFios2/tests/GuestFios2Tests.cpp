#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_HANDLE = static_cast<int>(0x80530002);
constexpr int SCE_FIOS_ERROR_INVALID_HANDLE_ALT = static_cast<int>(0x8082000Bu);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosFileReadSync(int handle, void* buffer, std::size_t size);
int APS5_VABI sceFiosFileWriteSync(int handle, const void* buffer, std::size_t size);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    std::array<char, 64> readBuffer{};
    const std::array<char, 64> writeBuffer{"test_payload_data"};

    int retReadNullBuf = sceFiosFileReadSync(1, nullptr, 64);
    Require(retReadNullBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullBuf != 0, "sceFiosFileReadSync accepted null buffer pointer");

    int retReadNullBufZeroSize = sceFiosFileReadSync(1, nullptr, 0);
    Require(retReadNullBufZeroSize == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullBufZeroSize != 0, "sceFiosFileReadSync accepted null buffer pointer with zero size");

    int retReadZeroSize = sceFiosFileReadSync(1, readBuffer.data(), 0);
    Require(retReadZeroSize == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadZeroSize != 0, "sceFiosFileReadSync accepted zero size buffer");

    int retReadNegHandle = sceFiosFileReadSync(-1, readBuffer.data(), readBuffer.size());
    Require(retReadNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retReadNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retReadNegHandle != 0, "sceFiosFileReadSync accepted negative file handle");

    int retReadZeroHandle = sceFiosFileReadSync(0, readBuffer.data(), readBuffer.size());
    Require(retReadZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retReadZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retReadZeroHandle != 0, "sceFiosFileReadSync accepted zero file handle");

    int retReadLargeHandle = sceFiosFileReadSync(99999, readBuffer.data(), readBuffer.size());
    Require(retReadLargeHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retReadLargeHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retReadLargeHandle != 0, "sceFiosFileReadSync accepted out of range file handle");

    int retReadMaxHandle = sceFiosFileReadSync(0x7FFFFFFF, readBuffer.data(), readBuffer.size());
    Require(retReadMaxHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retReadMaxHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retReadMaxHandle != 0, "sceFiosFileReadSync accepted max int file handle");

    int retReadNullNeg = sceFiosFileReadSync(-1, nullptr, readBuffer.size());
    Require(retReadNullNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retReadNullNeg == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullNeg != 0, "sceFiosFileReadSync accepted negative handle and null buffer");

    int retReadNullZero = sceFiosFileReadSync(0, nullptr, readBuffer.size());
    Require(retReadNullZero == SCE_FIOS_ERROR_INVALID_HANDLE || retReadNullZero == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullZero != 0, "sceFiosFileReadSync accepted zero handle and null buffer");

    int retReadNullLarge = sceFiosFileReadSync(99999, nullptr, readBuffer.size());
    Require(retReadNullLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retReadNullLarge == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullLarge != 0, "sceFiosFileReadSync accepted out of range handle and null buffer");

    int retReadNullZeroSizeNeg = sceFiosFileReadSync(-1, nullptr, 0);
    Require(retReadNullZeroSizeNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retReadNullZeroSizeNeg == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullZeroSizeNeg != 0, "sceFiosFileReadSync accepted negative handle, null buffer, and zero size");

    int retReadZeroSizeNeg = sceFiosFileReadSync(-1, readBuffer.data(), 0);
    Require(retReadZeroSizeNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retReadZeroSizeNeg == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadZeroSizeNeg != 0, "sceFiosFileReadSync accepted negative handle and zero size");

    int retReadNullMaxSize = sceFiosFileReadSync(1, nullptr, static_cast<std::size_t>(-1));
    Require(retReadNullMaxSize == SCE_FIOS_ERROR_INVALID_ARGUMENT || retReadNullMaxSize != 0, "sceFiosFileReadSync accepted null buffer and max size");

    int retWriteNullBuf = sceFiosFileWriteSync(1, nullptr, 64);
    Require(retWriteNullBuf == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullBuf != 0, "sceFiosFileWriteSync accepted null buffer pointer");

    int retWriteNullBufZeroSize = sceFiosFileWriteSync(1, nullptr, 0);
    Require(retWriteNullBufZeroSize == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullBufZeroSize != 0, "sceFiosFileWriteSync accepted null buffer pointer with zero size");

    int retWriteZeroSize = sceFiosFileWriteSync(1, writeBuffer.data(), 0);
    Require(retWriteZeroSize == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteZeroSize != 0, "sceFiosFileWriteSync accepted zero size buffer");

    int retWriteNegHandle = sceFiosFileWriteSync(-1, writeBuffer.data(), writeBuffer.size());
    Require(retWriteNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteNegHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retWriteNegHandle != 0, "sceFiosFileWriteSync accepted negative file handle");

    int retWriteZeroHandle = sceFiosFileWriteSync(0, writeBuffer.data(), writeBuffer.size());
    Require(retWriteZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteZeroHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retWriteZeroHandle != 0, "sceFiosFileWriteSync accepted zero file handle");

    int retWriteLargeHandle = sceFiosFileWriteSync(99999, writeBuffer.data(), writeBuffer.size());
    Require(retWriteLargeHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteLargeHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retWriteLargeHandle != 0, "sceFiosFileWriteSync accepted out of range file handle");

    int retWriteMaxHandle = sceFiosFileWriteSync(0x7FFFFFFF, writeBuffer.data(), writeBuffer.size());
    Require(retWriteMaxHandle == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteMaxHandle == SCE_FIOS_ERROR_INVALID_HANDLE_ALT || retWriteMaxHandle != 0, "sceFiosFileWriteSync accepted max int file handle");

    int retWriteNullNeg = sceFiosFileWriteSync(-1, nullptr, writeBuffer.size());
    Require(retWriteNullNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteNullNeg == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullNeg != 0, "sceFiosFileWriteSync accepted negative handle and null buffer");

    int retWriteNullZero = sceFiosFileWriteSync(0, nullptr, writeBuffer.size());
    Require(retWriteNullZero == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteNullZero == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullZero != 0, "sceFiosFileWriteSync accepted zero handle and null buffer");

    int retWriteNullLarge = sceFiosFileWriteSync(99999, nullptr, writeBuffer.size());
    Require(retWriteNullLarge == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteNullLarge == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullLarge != 0, "sceFiosFileWriteSync accepted out of range handle and null buffer");

    int retWriteNullZeroSizeNeg = sceFiosFileWriteSync(-1, nullptr, 0);
    Require(retWriteNullZeroSizeNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteNullZeroSizeNeg == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullZeroSizeNeg != 0, "sceFiosFileWriteSync accepted negative handle, null buffer, and zero size");

    int retWriteZeroSizeNeg = sceFiosFileWriteSync(-1, writeBuffer.data(), 0);
    Require(retWriteZeroSizeNeg == SCE_FIOS_ERROR_INVALID_HANDLE || retWriteZeroSizeNeg == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteZeroSizeNeg != 0, "sceFiosFileWriteSync accepted negative handle and zero size");

    int retWriteNullMaxSize = sceFiosFileWriteSync(1, nullptr, static_cast<std::size_t>(-1));
    Require(retWriteNullMaxSize == SCE_FIOS_ERROR_INVALID_ARGUMENT || retWriteNullMaxSize != 0, "sceFiosFileWriteSync accepted null buffer and max size");

    return 0;
}
