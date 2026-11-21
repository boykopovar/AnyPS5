#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool condition, const char* message, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "guest_pad_light_bar_rgb_user_color_bounds failed at line %u: %s\n", static_cast<unsigned>(location.line()), message);
    std::fflush(stderr);
    std::abort();
}

PadOutputState FetchOutput(int port, std::uint32_t& seen) {
    PadOutputState out{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &out), "expected output state update");
    return out;
}

void CheckColor(const PadOutputState& out, bool expectedValid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Require(out.lightBarValid == expectedValid, "lightBarValid state mismatch");
    Require(out.lightBar[0] == r, "red component mismatch");
    Require(out.lightBar[1] == g, "green component mismatch");
    Require(out.lightBar[2] == b, "blue component mismatch");
}

void TestPreInitializationBounds() {
    const PadLightBarParam param{255, 128, 64};
    for (int handle : {-100, -1, 0, 1, 2, 16, 32, 100, 9999}) {
        Require(scePadSetLightBar(handle, &param) == PAD_ERROR_INVALID_HANDLE, "pre-init set light bar must return INVALID_HANDLE");
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE, "pre-init set light bar null must return INVALID_HANDLE");
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE, "pre-init reset light bar must return INVALID_HANDLE");
        Require(scePadSetLightBarForTracker(handle, &param) == PAD_ERROR_INVALID_HANDLE, "pre-init tracker set light bar must return INVALID_HANDLE");
        Require(scePadSetLightBarForTracker(handle, nullptr) == PAD_ERROR_INVALID_HANDLE, "pre-init tracker set light bar null must return INVALID_HANDLE");
    }

    Require(scePadResetLightBarAll() == PAD_OK, "pre-init reset light bar all must return PAD_OK");
    for (std::uint32_t portType : {static_cast<std::uint32_t>(PAD_PORT_TYPE_STANDARD), 1u, static_cast<std::uint32_t>(PAD_PORT_TYPE_SPECIAL)}) {
        Require(scePadResetLightBarAllByPortType(portType) == PAD_OK, "pre-init reset light bar by port type 0/1/2 must return PAD_OK");
    }
    for (std::uint32_t invalidType : {3u, 4u, 16u, 255u, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(invalidType) == PAD_ERROR_INVALID_ARG, "pre-init reset light bar with invalid port type must return INVALID_ARG");
    }
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK, "scePadInit must return PAD_OK");
    Require(scePadInit_nid_postfix() == PAD_OK, "repeated scePadInit must return PAD_OK");

    const PadLightBarParam param{100, 150, 200};
    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        Require(scePadSetLightBar(handle, &param) == PAD_ERROR_INVALID_HANDLE, "unopened handle set light bar must return INVALID_HANDLE");
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE, "unopened handle set light bar null must return INVALID_HANDLE");
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE, "unopened handle reset light bar must return INVALID_HANDLE");
        Require(scePadSetLightBarForTracker(handle, &param) == PAD_ERROR_INVALID_HANDLE, "unopened handle tracker set light bar must return INVALID_HANDLE");
        Require(scePadSetLightBarForTracker(handle, nullptr) == PAD_ERROR_INVALID_HANDLE, "unopened handle tracker set light bar null must return INVALID_HANDLE");
    }

    Require(scePadResetLightBarAll() == PAD_OK, "reset light bar all with no open handles must return PAD_OK");
    for (std::uint32_t portType : {0u, 1u, 2u}) {
        Require(scePadResetLightBarAllByPortType(portType) == PAD_OK, "reset light bar by port type 0/1/2 must return PAD_OK");
    }
    for (std::uint32_t invalidType : {3u, 4u, 16u, 100u, 0x80000000u, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(invalidType) == PAD_ERROR_INVALID_ARG, "reset light bar with invalid port type must return INVALID_ARG");
    }
}

void TestStandardHandleLightBarRgbAndRestore() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0, "scePadOpen port 0 failed");

    Require(scePadSetLightBar(handle0, nullptr) == PAD_ERROR_INVALID_ARG, "null param on open handle must return INVALID_ARG");

    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};
    PadFetchOutputForPort_nid_postfix(0, &seen0, &out0);
    CheckColor(out0, false, 0, 0, 0);

    const PadLightBarParam testColors[] = {
        {0, 0, 0},
        {255, 0, 0},
        {0, 255, 0},
        {0, 0, 255},
        {255, 255, 255},
        {128, 64, 192},
        {1, 2, 3},
        {200, 150, 50}
    };

    for (const auto& color : testColors) {
        Require(scePadSetLightBar(handle0, &color) == PAD_OK, "scePadSetLightBar failed");
        out0 = FetchOutput(0, seen0);
        CheckColor(out0, true, color.r, color.g, color.b);
    }

    Require(scePadResetLightBar(handle0) == PAD_OK, "scePadResetLightBar failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, false, 0, 0, 0);

    Require(scePadResetLightBar(handle0) == PAD_OK, "repeated scePadResetLightBar failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, false, 0, 0, 0);

    const PadLightBarParam amber{255, 191, 0};
    Require(scePadSetLightBar(handle0, &amber) == PAD_OK, "restore light bar after reset failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 191, 0);

    Require(scePadResetLightBarAll() == PAD_OK, "scePadResetLightBarAll failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, false, 0, 0, 0);

    Require(scePadSetLightBar(handle0, &amber) == PAD_OK, "restore light bar before port type reset failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 191, 0);

    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK, "scePadResetLightBarAllByPortType failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, false, 0, 0, 0);

    Require(scePadSetLightBar(handle0, &amber) == PAD_OK, "restore light bar failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 191, 0);

    Require(scePadResetLightBarAllByPortType(1) == PAD_OK, "reset port type 1 failed");
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK, "reset port type 2 failed");
    std::uint32_t querySeen0 = 0xffffffffu;
    PadFetchOutputForPort_nid_postfix(0, &querySeen0, &out0);
    CheckColor(out0, true, 255, 191, 0);
}

void TestMultiControllerPortIsolation() {
    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};
    PadFetchOutputForPort_nid_postfix(0, &seen0, &out0);

    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0, "scePadOpen port 1 failed");
    std::uint32_t seen1 = 0xffffffffu;
    PadOutputState out1{};
    PadFetchOutputForPort_nid_postfix(1, &seen1, &out1);
    CheckColor(out1, false, 0, 0, 0);

    const int handle2 = scePadOpen_nid_postfix(0x10000002, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0, "scePadOpen port 2 failed");
    std::uint32_t seen2 = 0xffffffffu;
    PadOutputState out2{};
    PadFetchOutputForPort_nid_postfix(2, &seen2, &out2);
    CheckColor(out2, false, 0, 0, 0);

    const PadLightBarParam c0{255, 50, 0};
    const PadLightBarParam c1{0, 255, 50};
    const PadLightBarParam c2{50, 0, 255};

    Require(scePadSetLightBar(1, &c0) == PAD_OK, "set port 0 color failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 50, 0);
    out1 = FetchOutput(1, seen1);
    CheckColor(out1, false, 0, 0, 0);
    out2 = FetchOutput(2, seen2);
    CheckColor(out2, false, 0, 0, 0);

    Require(scePadSetLightBar(handle1, &c1) == PAD_OK, "set port 1 color failed");
    out1 = FetchOutput(1, seen1);
    CheckColor(out1, true, 0, 255, 50);
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 50, 0);
    out2 = FetchOutput(2, seen2);
    CheckColor(out2, false, 0, 0, 0);

    Require(scePadSetLightBar(handle2, &c2) == PAD_OK, "set port 2 color failed");
    out2 = FetchOutput(2, seen2);
    CheckColor(out2, true, 50, 0, 255);
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 50, 0);
    out1 = FetchOutput(1, seen1);
    CheckColor(out1, true, 0, 255, 50);

    Require(scePadResetLightBar(handle1) == PAD_OK, "reset port 1 failed");
    out1 = FetchOutput(1, seen1);
    CheckColor(out1, false, 0, 0, 0);
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 255, 50, 0);
    out2 = FetchOutput(2, seen2);
    CheckColor(out2, true, 50, 0, 255);

    Require(scePadResetLightBarAll() == PAD_OK, "reset all ports failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, false, 0, 0, 0);
    out1 = FetchOutput(1, seen1);
    CheckColor(out1, false, 0, 0, 0);
    out2 = FetchOutput(2, seen2);
    CheckColor(out2, false, 0, 0, 0);

    Require(scePadClose_nid_postfix(handle2) == PAD_OK, "close handle2 failed");
    Require(scePadClose_nid_postfix(handle1) == PAD_OK, "close handle1 failed");
}

void TestSpecialAndRemotePortBehavior() {
    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE, "open special port failed");

    Require(scePadSetLightBar(specialHandle, nullptr) == PAD_ERROR_INVALID_ARG, "null param on special handle must return INVALID_ARG");
    Require(scePadSetLightBarForTracker(specialHandle, nullptr) == PAD_ERROR_INVALID_ARG, "tracker null param on special handle must return INVALID_ARG");

    const PadLightBarParam dummyColor{100, 200, 150};
    Require(scePadSetLightBar(specialHandle, &dummyColor) == PAD_OK, "set light bar on special handle must return PAD_OK");
    Require(scePadSetLightBarForTracker(specialHandle, &dummyColor) == PAD_OK, "tracker set light bar on special handle must return PAD_OK");
    Require(scePadResetLightBar(specialHandle) == PAD_OK, "reset light bar on special handle must return PAD_OK");

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK, "close special handle failed");

    Require(scePadSetLightBar(specialHandle, &dummyColor) == PAD_ERROR_INVALID_HANDLE, "closed special handle must return INVALID_HANDLE");
    Require(scePadSetLightBarForTracker(specialHandle, &dummyColor) == PAD_ERROR_INVALID_HANDLE, "closed special handle tracker must return INVALID_HANDLE");
    Require(scePadResetLightBar(specialHandle) == PAD_ERROR_INVALID_HANDLE, "closed special handle reset must return INVALID_HANDLE");

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE, "open remote port failed");

    Require(scePadSetLightBar(remoteHandle, &dummyColor) == PAD_ERROR_INVALID_HANDLE, "remote handle set light bar must return INVALID_HANDLE");
    Require(scePadSetLightBar(remoteHandle, nullptr) == PAD_ERROR_INVALID_HANDLE, "remote handle null set light bar must return INVALID_HANDLE");
    Require(scePadSetLightBarForTracker(remoteHandle, &dummyColor) == PAD_ERROR_INVALID_HANDLE, "remote handle tracker set light bar must return INVALID_HANDLE");
    Require(scePadSetLightBarForTracker(remoteHandle, nullptr) == PAD_ERROR_INVALID_HANDLE, "remote handle tracker null set light bar must return INVALID_HANDLE");
    Require(scePadResetLightBar(remoteHandle) == PAD_ERROR_INVALID_HANDLE, "remote handle reset light bar must return INVALID_HANDLE");

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK, "close remote handle failed");
    Require(scePadSetLightBar(remoteHandle, &dummyColor) == PAD_ERROR_INVALID_HANDLE, "closed remote handle set light bar must return INVALID_HANDLE");
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    const PadLightBarParam c{77, 88, 99};
    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};

    Require(scePadSetLightBar(handle0, &c) == PAD_OK, "set port 0 before close failed");
    out0 = FetchOutput(0, seen0);
    CheckColor(out0, true, 77, 88, 99);

    Require(scePadClose_nid_postfix(handle0) == PAD_OK, "close handle0 failed");

    Require(scePadSetLightBar(handle0, nullptr) == PAD_ERROR_INVALID_HANDLE, "null param on closed standard handle must return INVALID_HANDLE");

    const PadLightBarParam cNew{11, 22, 33};
    Require(scePadSetLightBar(handle0, &cNew) == PAD_ERROR_INVALID_HANDLE, "set color on closed standard handle must return INVALID_HANDLE");
    Require(scePadResetLightBar(handle0) == PAD_ERROR_INVALID_HANDLE, "direct reset on closed standard handle must return INVALID_HANDLE");
    Require(scePadSetLightBarForTracker(handle0, &cNew) == PAD_ERROR_INVALID_HANDLE, "tracker set color on closed standard handle must return INVALID_HANDLE");
    Require(scePadSetLightBarForTracker(handle0, nullptr) == PAD_ERROR_INVALID_HANDLE, "tracker null on closed standard handle must return INVALID_HANDLE");

    const int unopenedHandle = 4;
    Require(scePadSetLightBar(unopenedHandle, &cNew) == PAD_ERROR_INVALID_HANDLE, "unopened handle 4 must return INVALID_HANDLE");
    Require(scePadResetLightBar(unopenedHandle) == PAD_ERROR_INVALID_HANDLE, "unopened handle 4 reset must return INVALID_HANDLE");
    Require(scePadSetLightBarForTracker(unopenedHandle, &cNew) == PAD_ERROR_INVALID_HANDLE, "unopened handle 4 tracker must return INVALID_HANDLE");
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleLightBarRgbAndRestore();
    TestMultiControllerPortIsolation();
    TestSpecialAndRemotePortBehavior();
    TestClosedStandardHandleBehavior();
    return 0;
}
