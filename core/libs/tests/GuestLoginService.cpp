#include "prx/libSceLoginService/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrNotInitialized = static_cast<int>(0x813a0001u);
constexpr int kErrInvalidArg = static_cast<int>(0x813a0003u);

}

int main() {
    std::array<unsigned char, 0x80> buffer{};
    buffer.fill(0x5Au);
    const auto snapshot = buffer;
    const auto* request = reinterpret_cast<const LoginServiceDeviceRequest*>(buffer.data());

    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);
    Require(buffer == snapshot);

    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);

    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);
    Require(buffer == snapshot);
    return 0;
}
