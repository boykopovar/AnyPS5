#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadDisableDataConversion(std::int32_t handle, bool disabled);
int APS5_VABI scePadDisableActiveProfileChange(std::int32_t handle, bool disabled);
int APS5_VABI scePadDisableActiveProfileChangeByDeviceId(std::int32_t deviceId, bool disabled);
int APS5_VABI scePadDisableTrackingLedBatterySaver(bool disabled);
int APS5_VABI scePadDisableTransportButtons(bool disabled);
int APS5_VABI scePadEnableAutoDetect(bool enabled);
int APS5_VABI scePadEnableControllerIndicator(std::int32_t handle);
int APS5_VABI scePadEnableProfileIndicator(std::int32_t handle);
int APS5_VABI scePadEnablePs4CompatibleMode(std::int32_t handle, bool enabled);
int APS5_VABI scePadEnableUsbConnection(bool enabled);
int APS5_VABI scePadEnablePlayPauseEmulation(bool enabled);
int APS5_VABI scePadEnableSpecificDeviceClass(std::int32_t deviceClass);
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename F>
static void RequireThrows(F&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    Require(threw);
}

int main() {
    Require(scePadDisableDataConversion(PAD_HANDLE, true) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadDisableTrackingLedBatterySaver(true) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadEnableAutoDetect(true) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadEnableSpecificDeviceClass(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    const int pad = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(pad > 0);
    for (int handle : {-1, 0, 2, 12345}) {
        Require(scePadDisableDataConversion(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableDataConversion(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadDisableActiveProfileChange(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadEnablePs4CompatibleMode(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadEnablePs4CompatibleMode(handle, false) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadEnableControllerIndicator(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadEnableProfileIndicator(handle) == PAD_ERROR_INVALID_HANDLE);
    }
    Require(scePadDisableDataConversion(pad, false) == PAD_OK);
    RequireThrows([] { (void)scePadDisableDataConversion(pad, true); });
    Require(scePadDisableActiveProfileChange(pad, false) == PAD_OK);
    Require(scePadDisableActiveProfileChange(pad, true) == PAD_OK);
    for (int deviceId : {-1, 0, 1, 123456})
        Require(scePadDisableActiveProfileChangeByDeviceId(deviceId, true) == PAD_OK);
    Require(scePadEnablePs4CompatibleMode(pad, false) == PAD_OK);
    RequireThrows([] { (void)scePadEnablePs4CompatibleMode(pad, true); });
    Require(scePadEnableControllerIndicator(pad) == PAD_OK);
    Require(scePadEnableProfileIndicator(pad) == PAD_OK);
    for (bool value : {false, true}) {
        Require(scePadDisableTrackingLedBatterySaver(value) == PAD_OK);
        Require(scePadDisableTransportButtons(value) == PAD_OK);
        Require(scePadEnableAutoDetect(value) == PAD_OK);
        Require(scePadEnableUsbConnection(value) == PAD_OK);
        Require(scePadEnablePlayPauseEmulation(value) == PAD_OK);
    }
    for (int deviceClass : {-1, 0, 1, 128})
        Require(scePadEnableSpecificDeviceClass(deviceClass) == PAD_OK);
    return 0;
}
