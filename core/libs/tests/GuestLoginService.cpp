#include "prx/libSceLoginService/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    unsigned char buffer[0x80]{};
    const auto* request = reinterpret_cast<const LoginServiceDeviceRequest*>(buffer);

    Require(sceLoginServiceRequestDevices(nullptr) == static_cast<int>(0x813a0003));
    Require(sceLoginServiceRequestDevices(request) == static_cast<int>(0x813a0001));
    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == static_cast<int>(0x813a0003));
    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceRequestDevices(request) == static_cast<int>(0x813a0001));
    return 0;
}
