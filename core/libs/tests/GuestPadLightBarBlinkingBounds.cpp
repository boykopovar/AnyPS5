#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstdio>
#include <initializer_list>
#include <source_location>

namespace {

constexpr int PAD_ERROR_INVALID_ARG = static_cast<int>(0x80920001u);
constexpr int PAD_ERROR_INVALID_HANDLE = static_cast<int>(0x80920003u);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GuestPadLightBarBlinkingBounds check failed at line %u\n", location.line());
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
    const PadLightBarParam color{10, 20, 30};

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK);
    Require(scePadResetLightBarAllByPortType(1) == PAD_OK);
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK);
    for (std::uint32_t badType : {3u, 4u, 5u, 16u, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(badType) == PAD_ERROR_INVALID_ARG);
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    const PadLightBarParam color{255, 128, 64};

    for (int handle : {-100, -1, 0, 1, 2, 3, 4, 10, PAD_REMOTE_HANDLE, PAD_SPECIAL_HANDLE, 9999}) {
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK);
    Require(scePadResetLightBarAllByPortType(1) == PAD_OK);
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK);
    for (std::uint32_t badType : {3u, 4u, 5u, 16u, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(badType) == PAD_ERROR_INVALID_ARG);
    }
}

void TestStandardHandleLightBarColorRestore() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 == 1);

    Require(scePadIsLightBarBaseBrightnessControllable(handle0) == 1);
    Require(scePadSetLightBarBaseBrightness(handle0, 50) == PAD_OK);

    Require(scePadSetLightBar(handle0, nullptr) == PAD_ERROR_INVALID_ARG);

    const PadLightBarParam off{0, 0, 0};
    const PadLightBarParam red{255, 0, 0};
    const PadLightBarParam green{0, 255, 0};
    const PadLightBarParam blue{0, 0, 255};
    const PadLightBarParam white{255, 255, 255};
    const PadLightBarParam mixed{128, 64, 192};

    Require(scePadSetLightBar(handle0, &off) == PAD_OK && LightBarIs(0, true, 0, 0, 0));
    Require(scePadSetLightBar(handle0, &red) == PAD_OK && LightBarIs(0, true, 255, 0, 0));
    Require(scePadSetLightBar(handle0, &green) == PAD_OK && LightBarIs(0, true, 0, 255, 0));
    Require(scePadSetLightBar(handle0, &blue) == PAD_OK && LightBarIs(0, true, 0, 0, 255));
    Require(scePadSetLightBar(handle0, &white) == PAD_OK && LightBarIs(0, true, 255, 255, 255));
    Require(scePadSetLightBar(handle0, &mixed) == PAD_OK && LightBarIs(0, true, 128, 64, 192));

    Require(scePadResetLightBar(handle0) == PAD_OK && LightBarIs(0, false, 0, 0, 0));

    Require(scePadSetLightBar(handle0, &mixed) == PAD_OK && LightBarIs(0, true, 128, 64, 192));
    Require(scePadResetLightBarAll() == PAD_OK && LightBarIs(0, false, 0, 0, 0));

    Require(scePadSetLightBar(handle0, &red) == PAD_OK && LightBarIs(0, true, 255, 0, 0));
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK && LightBarIs(0, false, 0, 0, 0));

    Require(scePadSetLightBar(handle0, &green) == PAD_OK && LightBarIs(0, true, 0, 255, 0));
    Require(scePadResetLightBarAllByPortType(1) == PAD_OK && LightBarIs(0, true, 0, 255, 0));
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK && LightBarIs(0, true, 0, 255, 0));
    for (std::uint32_t badType : {3u, 4u, 5u, 16u, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(badType) == PAD_ERROR_INVALID_ARG);
    }
    Require(LightBarIs(0, true, 0, 255, 0));
    Require(scePadResetLightBar(handle0) == PAD_OK && LightBarIs(0, false, 0, 0, 0));
}

void TestMultiPortIsolation() {
    const int handle0 = 1;
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 == 2);

    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(1, false, 0, 0, 0));

    const PadLightBarParam color0{200, 10, 30};
    const PadLightBarParam color1{40, 150, 250};

    Require(scePadSetLightBar(handle0, &color0) == PAD_OK);
    Require(LightBarIs(0, true, 200, 10, 30));
    Require(LightBarIs(1, false, 0, 0, 0));

    Require(scePadSetLightBar(handle1, &color1) == PAD_OK);
    Require(LightBarIs(0, true, 200, 10, 30));
    Require(LightBarIs(1, true, 40, 150, 250));

    Require(scePadResetLightBar(handle0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(1, true, 40, 150, 250));

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(1, false, 0, 0, 0));
}

void TestSpecialAndRemotePortBehavior() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special == PAD_SPECIAL_HANDLE);

    Require(scePadIsLightBarBaseBrightnessControllable(special) == 0);
    Require(scePadSetLightBarBaseBrightness(special, 10) == PAD_OK);

    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_ARG);
    const PadLightBarParam specialColor{99, 88, 77};
    Require(scePadSetLightBar(special, &specialColor) == PAD_OK);
    Require(scePadResetLightBar(special) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);

    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(special, &specialColor) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsLightBarBaseBrightnessControllable(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(special, 10) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote == PAD_REMOTE_HANDLE);

    Require(scePadSetLightBar(remote, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(remote, &specialColor) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsLightBarBaseBrightnessControllable(remote) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(remote, 10) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(remote) == PAD_OK);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const int handle1 = 2;

    const PadLightBarParam color1{70, 80, 90};
    Require(scePadSetLightBar(handle1, &color1) == PAD_OK);
    Require(LightBarIs(1, true, 70, 80, 90));

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(handle1) == 1);
    Require(scePadSetLightBarBaseBrightness(handle1, 5) == PAD_OK);

    Require(scePadSetLightBar(handle1, nullptr) == PAD_ERROR_INVALID_ARG);

    const PadLightBarParam closedColor{111, 122, 133};
    Require(scePadSetLightBar(handle1, &closedColor) == PAD_OK);
    Require(LightBarIs(1, true, 111, 122, 133));

    Require(scePadResetLightBar(handle1) == PAD_OK);
    Require(LightBarIs(1, false, 0, 0, 0));

    Require(scePadSetLightBar(handle1, &closedColor) == PAD_OK);
    Require(LightBarIs(1, true, 111, 122, 133));

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(LightBarIs(1, true, 111, 122, 133));

    Require(scePadResetLightBar(handle1) == PAD_OK);
    Require(LightBarIs(1, false, 0, 0, 0));

    for (int invalid : {-1, 0, 3, 4, 10, 9999}) {
        Require(scePadSetLightBar(invalid, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(invalid, &closedColor) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(invalid) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(invalid) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle0) == PAD_OK);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleLightBarColorRestore();
    TestMultiPortIsolation();
    TestSpecialAndRemotePortBehavior();
    TestClosedStandardHandleBehavior();
    return 0;
}
