#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TFunc>
bool ReportsNotImplemented(TFunc&& func) {
    try {
        func();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestUninitializedState() {
    const PadLightBarParam color{255, 128, 64};
    const PadLightBarBlinkingParameter blink{};

    for (int handle : {-1000, -1, 0, 1, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarForTracker(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarForTracker(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBlinking(handle, &blink) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 255) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadDisableTrackingLedBatterySaver(false) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadDisableTrackingLedBatterySaver(true) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadResetLightBarAll() == PAD_OK);
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK);
    Require(scePadResetLightBarAllByPortType(1) == PAD_OK);
    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK);
    for (std::uint32_t invalidType : {3u, 4u, 10u, 100u, 0x80000000u, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(invalidType) == PAD_ERROR_INVALID_ARG);
    }
}

void TestInitializedInvalidHandles() {
    const PadLightBarParam color{255, 128, 64};
    const PadLightBarBlinkingParameter blink{};

    for (int handle : {-1000, -100, -1, 0, 1, 2, 3, 16, 32, 100, 12345}) {
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarForTracker(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarForTracker(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBlinking(handle, &blink) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 255) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestStandardHandleColorLed() {
    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBarForTracker(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    Require(scePadIsLightBarBaseBrightnessControllable(handle) == 1);
    for (std::uint32_t brightness : {0u, 1u, 2u, 128u, 255u}) {
        Require(scePadSetLightBarBaseBrightness(handle, brightness) == PAD_OK);
    }

    std::uint32_t seen = 0xffffffffu;
    PadOutputState output{};
    Require(PadFetchOutputForPort_nid_postfix(0, &seen, &output));
    Require(!output.lightBarValid);
    Require(output.lightBar[0] == 0 && output.lightBar[1] == 0 && output.lightBar[2] == 0);

    const PadLightBarParam colors[] = {
        {0, 0, 0},
        {255, 0, 0},
        {0, 255, 0},
        {0, 0, 255},
        {255, 255, 0},
        {0, 255, 255},
        {255, 0, 255},
        {255, 255, 255},
        {128, 64, 32},
        {1, 2, 3},
        {199, 88, 44}
    };

    for (const auto& color : colors) {
        Require(scePadSetLightBar(handle, &color) == PAD_OK);
        Require(PadFetchOutputForPort_nid_postfix(0, &seen, &output));
        Require(output.lightBarValid);
        Require(output.lightBar[0] == color.r);
        Require(output.lightBar[1] == color.g);
        Require(output.lightBar[2] == color.b);
    }

    const PadLightBarParam trackerColor{80, 160, 240};
    Require(scePadSetLightBarForTracker(handle, &trackerColor) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen, &output));
    Require(output.lightBarValid);
    Require(output.lightBar[0] == trackerColor.r);
    Require(output.lightBar[1] == trackerColor.g);
    Require(output.lightBar[2] == trackerColor.b);

    Require(scePadResetLightBar(handle) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen, &output));
    Require(!output.lightBarValid);
    Require(output.lightBar[0] == 0 && output.lightBar[1] == 0 && output.lightBar[2] == 0);

    const PadLightBarBlinkingParameter blink{};
    Require(ReportsNotImplemented([&] {
        scePadSetLightBarBlinking(handle, &blink);
    }));

    Require(ReportsNotImplemented([&] {
        scePadSetUserColor(handle, 0xff00ff00u);
    }));

    Require(ReportsNotImplemented([&] {
        scePadSetMuteLed(handle, 1);
    }));

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadSetLightBar(handle, &trackerColor) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarForTracker(handle, &trackerColor) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarForTracker(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBlinking(handle, &blink) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
}

void TestSpecialHandleColorLed() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBarForTracker(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBarBlinking(special, nullptr) == PAD_ERROR_INVALID_ARG);

    Require(scePadIsLightBarBaseBrightnessControllable(special) == 0);
    for (std::uint32_t brightness : {0u, 1u, 2u, 255u}) {
        Require(scePadSetLightBarBaseBrightness(special, brightness) == PAD_OK);
    }

    const PadLightBarParam color{200, 100, 50};
    Require(scePadSetLightBar(special, &color) == PAD_OK);
    Require(scePadSetLightBarForTracker(special, &color) == PAD_OK);
    Require(scePadResetLightBar(special) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetLightBar(special, &color) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarForTracker(special, &color) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarForTracker(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsLightBarBaseBrightnessControllable(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(special, 1) == PAD_ERROR_INVALID_HANDLE);
}

void TestMultiPortAndResetAll() {
    const int h0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    const int h1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 1, nullptr);
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(h0 > 0 && h1 > 0 && special > 0);

    const PadLightBarParam color0{11, 22, 33};
    const PadLightBarParam color1{44, 55, 66};
    Require(scePadSetLightBar(h0, &color0) == PAD_OK);
    Require(scePadSetLightBar(h1, &color1) == PAD_OK);

    std::uint32_t seen0 = 0xffffffffu;
    std::uint32_t seen1 = 0xffffffffu;
    PadOutputState out0{};
    PadOutputState out1{};
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(out0.lightBarValid && out0.lightBar[0] == 11 && out0.lightBar[1] == 22 && out0.lightBar[2] == 33);
    Require(out1.lightBarValid && out1.lightBar[0] == 44 && out1.lightBar[1] == 55 && out1.lightBar[2] == 66);

    for (std::uint32_t invalidType : {3u, 4u, 8u, 16u, 0xffu, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(invalidType) == PAD_ERROR_INVALID_ARG);
    }

    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(out0.lightBarValid && out0.lightBar[0] == 11);
    Require(out1.lightBarValid && out1.lightBar[0] == 44);

    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(!out0.lightBarValid);
    Require(out1.lightBarValid && out1.lightBar[0] == 44);

    Require(scePadSetLightBar(h0, &color0) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.lightBarValid && out1.lightBarValid);

    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(!out0.lightBarValid);
    Require(!out1.lightBarValid);

    Require(scePadSetLightBar(h0, &color0) == PAD_OK);
    Require(scePadSetLightBar(h1, &color1) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(out0.lightBarValid && out1.lightBarValid);

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(!out0.lightBarValid);
    Require(!out1.lightBarValid);

    Require(scePadClose_nid_postfix(h0) == PAD_OK);
    Require(scePadClose_nid_postfix(h1) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
}

void TestTrackingLedBatterySaver() {
    Require(scePadDisableTrackingLedBatterySaver(false) == PAD_OK);
    Require(ReportsNotImplemented([] {
        scePadDisableTrackingLedBatterySaver(true);
    }));
}

} // namespace

int main() {
    TestUninitializedState();
    Require(scePadInit_nid_postfix() == PAD_OK);
    TestInitializedInvalidHandles();
    TestStandardHandleColorLed();
    TestSpecialHandleColorLed();
    TestMultiPortAndResetAll();
    TestTrackingLedBatterySaver();
    return 0;
}
