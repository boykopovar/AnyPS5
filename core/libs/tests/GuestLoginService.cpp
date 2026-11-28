#include "prx/libSceLoginService/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = EACCES;
}

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrNotInitialized = static_cast<int>(0x813a0001u);
constexpr int kErrInvalidArg = static_cast<int>(0x813a0003u);

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);

    unsigned char buffer[0x80]{};
    const auto* request = reinterpret_cast<const LoginServiceDeviceRequest*>(buffer);
    const auto* unmappedLow = reinterpret_cast<const LoginServiceDeviceRequest*>(0x10);
    const auto* unmappedHigh = reinterpret_cast<const LoginServiceDeviceRequest*>(~std::uintptr_t{0});

    errno = EAGAIN;
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(unmappedLow) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(unmappedHigh) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);
    Require(errno == EAGAIN);

    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(unmappedLow) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(unmappedHigh) == kErrInvalidArg);
    Require(errno == EAGAIN);

    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(unmappedLow) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(unmappedHigh) == kErrInvalidArg);
    Require(errno == EAGAIN);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);
    Require(errno == EAGAIN);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    return 0;
}
