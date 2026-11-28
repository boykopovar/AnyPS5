#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

PadOutputState FetchOutput(int port, std::uint32_t& seen) {
    PadOutputState out{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &out));
    return out;
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestPreInitializationBounds() {
    Require(scePadEnableExtensionPort(false) == PAD_OK);
    Require(scePadEnableExtensionPort(true) == PAD_OK);

    for (int handle : {-1000, -1, 0, 1, 2, 16, 32, 100, 9999}) {
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 100) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 0xffffffffu) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, true) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_NOT_INITIALIZED);
    }

    Require(scePadEnableSpecificDeviceClass(0) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetExtensionUnitInfo(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(Throws([] { scePadSetExtensionReport(1, nullptr); }));
}

void TestPostInitializationUnopenedHandleBounds() {
    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    for (int handle : {-500, -1, 0, 1, 2, 3, 4, 16, 32, 47, 500}) {
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 50) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 0xffffffffu) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableTriggerEffect(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }
}

void TestStandardHandleBrightnessExtensionAndTriggerDisable() {
    const int handle0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0);

    Require(scePadIsLightBarBaseBrightnessControllable(handle0) == 1);

    for (std::uint32_t brightness : {0u, 1u, 2u, 10u, 50u, 100u, 128u, 200u, 255u, 500u, 1000u, 65535u, 0x80000000u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(handle0, brightness) == PAD_OK);
    }

    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};
    PadFetchOutputForPort_nid_postfix(0, &seen0, &out0);
    Require(!out0.lightBarValid);

    const PadLightBarParam color{64, 128, 192};
    Require(scePadSetLightBar(handle0, &color) == PAD_OK);
    out0 = FetchOutput(0, seen0);
    Require(out0.lightBarValid && out0.lightBar[0] == 64 && out0.lightBar[1] == 128 && out0.lightBar[2] == 192);

    for (std::uint32_t brightness : {0u, 100u, 255u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(handle0, brightness) == PAD_OK);
    }
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));

    Require(scePadResetLightBar(handle0) == PAD_OK);
    out0 = FetchOutput(0, seen0);
    Require(!out0.lightBarValid);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);
    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));

    Require(!out0.triggerDisabled);

    Require(scePadDisableTriggerEffect(handle0, true) == PAD_OK);
    out0 = FetchOutput(0, seen0);
    Require(out0.triggerDisabled);

    Require(scePadDisableTriggerEffect(handle0, true) == PAD_OK);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));

    Require(scePadDisableTriggerEffect(handle0, false) == PAD_OK);
    out0 = FetchOutput(0, seen0);
    Require(!out0.triggerDisabled);
}

void TestMultiControllerIsolation() {
    const int handle1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0);

    std::uint32_t seen1 = 0xffffffffu;
    PadOutputState out1{};
    PadFetchOutputForPort_nid_postfix(1, &seen1, &out1);
    Require(!out1.triggerDisabled);

    Require(scePadIsLightBarBaseBrightnessControllable(handle1) == 1);
    Require(scePadSetLightBarBaseBrightness(handle1, 200) == PAD_OK);

    Require(scePadDisableTriggerEffect(handle1, true) == PAD_OK);
    out1 = FetchOutput(1, seen1);
    Require(out1.triggerDisabled);

    std::uint32_t seen0 = 0xffffffffu;
    PadOutputState out0{};
    PadFetchOutputForPort_nid_postfix(0, &seen0, &out0);
    Require(!out0.triggerDisabled);

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
}

void TestSpecialAndRemoteHandleBehavior() {
    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle >= PAD_SPECIAL_HANDLE);

    Require(scePadIsLightBarBaseBrightnessControllable(specialHandle) == 0);

    for (std::uint32_t brightness : {0u, 10u, 100u, 255u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(specialHandle, brightness) == PAD_OK);
    }

    Require(scePadDisableTriggerEffect(specialHandle, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(specialHandle, false) == PAD_OK);

    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(specialHandle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(specialHandle, 100) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(specialHandle, true) == PAD_ERROR_INVALID_HANDLE);

    const int remoteHandle = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remoteHandle == PAD_REMOTE_HANDLE);

    Require(scePadIsLightBarBaseBrightnessControllable(remoteHandle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(remoteHandle, 100) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(remoteHandle, true) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(remoteHandle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(remoteHandle, 100) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(remoteHandle, true) == PAD_ERROR_INVALID_HANDLE);
}

void TestClosedStandardHandleBehavior() {
    const int handle0 = 1;
    Require(scePadClose_nid_postfix(handle0) == PAD_OK);

    Require(scePadIsLightBarBaseBrightnessControllable(handle0) == 1);

    for (std::uint32_t brightness : {0u, 50u, 100u, 255u, 0xffffffffu}) {
        Require(scePadSetLightBarBaseBrightness(handle0, brightness) == PAD_OK);
    }

    Require(scePadDisableTriggerEffect(handle0, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(handle0, false) == PAD_OK);

    const int unopenedHandle = 4;
    Require(scePadIsLightBarBaseBrightnessControllable(unopenedHandle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(unopenedHandle, 50) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(unopenedHandle, true) == PAD_ERROR_INVALID_HANDLE);
}

void TestExtensionPortStubsThrow() {
    Require(scePadEnableSpecificDeviceClass(0) == PAD_ERROR_INVALID_ARG);
    Require(Throws([] { scePadEnableSpecificDeviceClass(9); }));
    Require(scePadGetExtensionUnitInfo(1, nullptr) == PAD_ERROR_INVALID_ARG);
    alignas(alignof(std::max_align_t)) std::array<std::uint8_t, 64> unitBuf{};
    Require(Throws([&] { scePadGetExtensionUnitInfo(1, reinterpret_cast<PadExtensionUnitInfo*>(unitBuf.data())); }));
    Require(Throws([] { scePadSetExtensionReport(1, nullptr); }));
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestPostInitializationUnopenedHandleBounds();
    TestStandardHandleBrightnessExtensionAndTriggerDisable();
    TestMultiControllerIsolation();
    TestSpecialAndRemoteHandleBehavior();
    TestClosedStandardHandleBehavior();
    TestExtensionPortStubsThrow();
    return 0;
}
