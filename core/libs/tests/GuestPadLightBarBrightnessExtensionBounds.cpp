#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int PAD_ERROR_INVALID_HANDLE = static_cast<int>(0x80920003u);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

PadOutputState OutputOf(int port) {
    std::uint32_t seen = 0xffffffffu;
    PadOutputState output{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &output));
    return output;
}

bool LightBarIs(int port, bool valid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const PadOutputState output = OutputOf(port);
    return output.lightBarValid == valid &&
           output.lightBar[0] == r &&
           output.lightBar[1] == g &&
           output.lightBar[2] == b;
}

void TestPreInitializationBounds() {
    Require(scePadEnableExtensionPort(false) == PAD_OK);
    Require(scePadEnableExtensionPort(true) == PAD_OK);

    for (int handle : {-1000, -100, -2, -1, 0, 1, 2, 3, 4, 10, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 100) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 0xffffffffu) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    bool caught = false;
    try {
        scePadEnableSpecificDeviceClass(0);
    } catch (...) {
        caught = true;
    }
    Require(caught);

    caught = false;
    try {
        scePadGetExtensionUnitInfo(1, nullptr);
    } catch (...) {
        caught = true;
    }
    Require(caught);

    caught = false;
    try {
        scePadSetExtensionReport(1, nullptr);
    } catch (...) {
        caught = true;
    }
    Require(caught);
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    for (int handle : {-1000, -100, -2, -1, 0, 1, 2, 3, 4, 10, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 50) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 0xffffffffu) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestStandardHandleBrightnessAndControllable() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    Require(scePadIsLightBarBaseBrightnessControllable(handle0) == 1);

    for (std::uint32_t brightness : {0u, 1u, 2u, 10u, 50u, 100u, 128u, 200u, 255u, 500u, 1000u, 65535u, 0x80000000u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(handle0, brightness) == PAD_OK);
    }

    Require(LightBarIs(0, false, 0, 0, 0));

    const PadLightBarParam color{64, 128, 192};
    Require(scePadSetLightBar(handle0, &color) == PAD_OK);
    Require(LightBarIs(0, true, 64, 128, 192));

    Require(scePadSetLightBarBaseBrightness(handle0, 255) == PAD_OK);
    Require(LightBarIs(0, true, 64, 128, 192));

    Require(scePadResetLightBar(handle0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);
    Require(scePadEnableExtensionPort(true) == PAD_OK);

    for (int badHandle : {-100, -1, 0, 2, 3, 4, 5, 32, 33, 100, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetLightBarBaseBrightness(badHandle, 100) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(badHandle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestSpecialAndRemotePortBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);

    Require(scePadIsLightBarBaseBrightnessControllable(special) == 0);
    for (std::uint32_t brightness : {0u, 10u, 100u, 255u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(special, brightness) == PAD_OK);
    }

    Require(scePadClose_nid_postfix(special) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(special, 100) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);

    Require(scePadIsLightBarBaseBrightnessControllable(remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(remote, 100) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(remote) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(remote, 100) == PAD_ERROR_INVALID_HANDLE);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    Require(scePadIsLightBarBaseBrightnessControllable(handle1) == 1);
    Require(scePadSetLightBarBaseBrightness(handle1, 20) == PAD_OK);

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(handle1) == 1);
    Require(scePadSetLightBarBaseBrightness(handle1, 30) == PAD_OK);

    for (std::uint32_t brightness : {0u, 1u, 50u, 100u, 255u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(handle1, brightness) == PAD_OK);
    }

    Require(scePadIsLightBarBaseBrightnessControllable(3) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(3, 30) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle0) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(handle0) == 1);
    Require(scePadSetLightBarBaseBrightness(handle0, 40) == PAD_OK);
}

void TestExtensionPortStubsThrow() {
    bool caught = false;
    try {
        scePadEnableSpecificDeviceClass(1);
    } catch (...) {
        caught = true;
    }
    Require(caught);

    caught = false;
    try {
        scePadGetExtensionUnitInfo(1, nullptr);
    } catch (...) {
        caught = true;
    }
    Require(caught);

    caught = false;
    try {
        scePadSetExtensionReport(1, nullptr);
    } catch (...) {
        caught = true;
    }
    Require(caught);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleBrightnessAndControllable();
    TestSpecialAndRemotePortBehavior();
    TestClosedStandardHandleBehavior();
    TestExtensionPortStubsThrow();
    return 0;
}
