#include "prx/libSceCamera/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr int ErrorParam = static_cast<int>(0x802E0000u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Camera: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == name + " not implemented", "a declared camera function threw another error");
        return;
    }
    Require(false, "a declared camera function returned");
}

}

int main() {
    Require(sceCameraIsAttached(0) == 0, "camera 0 must be reported as not attached");
    Require(sceCameraIsAttached(1) == ErrorParam, "camera index 1 must be rejected");
    Require(sceCameraIsAttached(-1) == ErrorParam, "a negative camera index must be rejected");
    RequireNotImplemented([] { sceCameraOpen(); }, "sceCameraOpen");
    RequireNotImplemented([] { sceCameraClose(); }, "sceCameraClose");
    RequireNotImplemented([] { sceCameraStart(); }, "sceCameraStart");
    RequireNotImplemented([] { sceCameraStop(); }, "sceCameraStop");
    RequireNotImplemented([] { sceCameraGetConfig(); }, "sceCameraGetConfig");
    RequireNotImplemented([] { sceCameraSetConfig(); }, "sceCameraSetConfig");
    RequireNotImplemented([] { sceCameraSetAttribute(); }, "sceCameraSetAttribute");
    RequireNotImplemented([] { sceCameraGetDeviceInfo(); }, "sceCameraGetDeviceInfo");
    RequireNotImplemented([] { sceCameraGetFrameData(); }, "sceCameraGetFrameData");
    RequireNotImplemented([] { sceCameraIsValidFrameData(); }, "sceCameraIsValidFrameData");
    return 0;
}
