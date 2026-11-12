#include "prx/libSceLoginService/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrNotInitialized = static_cast<int>(0x813a0001u);
constexpr int kErrInvalidArg = static_cast<int>(0x813a0003u);

}

int main() {
    unsigned char buffer[0x80]{};
    const auto* request = reinterpret_cast<const LoginServiceDeviceRequest*>(buffer);

    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    errno = 71;
    Require(sceLoginServiceRequestDevices(reinterpret_cast<const LoginServiceDeviceRequest*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);

    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    errno = 71;
    Require(sceLoginServiceRequestDevices(reinterpret_cast<const LoginServiceDeviceRequest*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);

    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    errno = 71;
    Require(sceLoginServiceRequestDevices(reinterpret_cast<const LoginServiceDeviceRequest*>(0x10)) == kErrInvalidArg);
    Require(errno == 71);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);
    return 0;
}
