#include "prx/libSceLoginService/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

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
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);

    Require(sceLoginServiceInitialize() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);

    bool threw = false;
    try {
        sceLoginServiceRequestDevices(request);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == "sceLoginServiceRequestDevices not implemented") {
            threw = true;
        } else {
            std::fprintf(stderr, "sceLoginServiceRequestDevices threw: %s\n", error.what());
            return 1;
        }
    }
    Require(threw);

    Require(sceLoginServiceTerminate() == 0);
    Require(sceLoginServiceRequestDevices(nullptr) == kErrInvalidArg);
    Require(sceLoginServiceRequestDevices(request) == kErrNotInitialized);
    return 0;
}
