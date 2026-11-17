#include "prx/libSceAutoMounterClient/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Testing.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = EACCES;
}

constexpr int AUTOMOUNTER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80F40002u);

bool AllBytesEqual(const void* data, std::size_t size, unsigned char expected) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        if (bytes[index] != expected) return false;
    }
    return true;
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    AutoMounterUsbDeviceInfo info{};
    std::memset(&info, 0x5a, sizeof(info));
    char deviceNames[1][64]{};
    std::int32_t count = 0x12345678;

    for (const char* invalidName : {"usb0", "dev/usb0", "/usb0", " /dev/usb0", "/DEV/usb0", "a"}) {
        errno = EAGAIN;
        Require(sceAutoMounterClientGetUsbDeviceInfo(invalidName, &info) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
        Require(errno == EAGAIN);
    }

    std::array<char, 256> unterminated{};
    unterminated.fill('x');
    errno = EAGAIN;
    Require(sceAutoMounterClientGetUsbDeviceInfo(unterminated.data(), &info) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(errno == EAGAIN);
    errno = EAGAIN;
    Require(sceAutoMounterClientGetUsbDeviceInfo(reinterpret_cast<const char*>(0x10), &info) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(errno == EAGAIN);
    errno = EAGAIN;
    Require(sceAutoMounterClientGetUsbDeviceInfo(reinterpret_cast<const char*>(~std::uintptr_t{0}), &info) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(errno == EAGAIN);
    Require(AllBytesEqual(&info, sizeof(info), 0x5a));

    Require(sceAutoMounterClientGetUsbDeviceList(nullptr, &count) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(count == 0x12345678);
    Require(sceAutoMounterClientGetUsbDeviceList(deviceNames, nullptr) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);
    Require(sceAutoMounterClientGetUsbDeviceList(nullptr, nullptr) == AUTOMOUNTER_ERROR_INVALID_ARGUMENT);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    return 0;
}
