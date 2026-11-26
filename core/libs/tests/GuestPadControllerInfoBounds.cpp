#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static_assert(sizeof(PadTouchPadInformation) == 8);
static_assert(offsetof(PadTouchPadInformation, pixelDensity) == 0);
static_assert(offsetof(PadTouchPadInformation, resolution) == 4);

static_assert(sizeof(PadStickInformation) == 2);
static_assert(offsetof(PadStickInformation, deadZoneLeft) == 0);
static_assert(offsetof(PadStickInformation, deadZoneRight) == 1);

static_assert(sizeof(PadControllerInformation) == 28);
static_assert(offsetof(PadControllerInformation, touchPadInfo) == 0);
static_assert(offsetof(PadControllerInformation, stickInfo) == 8);
static_assert(offsetof(PadControllerInformation, connectionType) == 10);
static_assert(offsetof(PadControllerInformation, connectedCount) == 11);
static_assert(offsetof(PadControllerInformation, connected) == 12);
static_assert(offsetof(PadControllerInformation, deviceClass) == 16);
static_assert(offsetof(PadControllerInformation, reserve) == 20);

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    for (int invalidHandle : {-10000, -100, -2, -1, 0, 1, 2, 3, 4, 5, 15, 16, 31, 32, 47, 48, 100, 99999, 0x7fffffff}) {
        PadControllerInformation canary{};
        std::memset(&canary, 0xaa, sizeof(canary));
        Require(scePadGetControllerInformation(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(invalidHandle, &canary) == PAD_ERROR_INVALID_HANDLE);
        const auto* raw = reinterpret_cast<const std::uint8_t*>(&canary);
        for (std::size_t i = 0; i < sizeof(PadControllerInformation); ++i) {
            Require(raw[i] == 0xaa);
        }
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int invalidHandle : {-1000, -50, -1, 0, 1, 2, 3, 4, 5, 15, 16, 31, 32, 47, 48, 100, 99999}) {
        PadControllerInformation canary{};
        std::memset(&canary, 0xaa, sizeof(canary));
        Require(scePadGetControllerInformation(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(invalidHandle, &canary) == PAD_ERROR_INVALID_HANDLE);
        const auto* raw = reinterpret_cast<const std::uint8_t*>(&canary);
        for (std::size_t i = 0; i < sizeof(PadControllerInformation); ++i) {
            Require(raw[i] == 0xaa);
        }
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    for (int unopened : {PAD_HANDLE_BASE + 1, PAD_HANDLE_BASE + 2, PAD_HANDLE_BASE + 3}) {
        PadControllerInformation canary{};
        std::memset(&canary, 0xaa, sizeof(canary));
        Require(scePadGetControllerInformation(unopened, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(unopened, &canary) == PAD_ERROR_INVALID_HANDLE);
        const auto* raw = reinterpret_cast<const std::uint8_t*>(&canary);
        for (std::size_t i = 0; i < sizeof(PadControllerInformation); ++i) {
            Require(raw[i] == 0xaa);
        }
    }

    Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    PadControllerInformation info{};
    std::memset(&info, 0xaa, sizeof(info));
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(info.stickInfo.deadZoneLeft == 13);
    Require(info.stickInfo.deadZoneRight == 13);
    Require(info.connectionType == PAD_CONNECTION_TYPE_LOCAL);
    Require(info.deviceClass == PAD_DEVICE_CLASS_STANDARD);
    Require(info.connected && info.connectedCount == 1);

    PadSetPortConnected_nid_postfix(0, false);
    std::memset(&info, 0xbb, sizeof(info));
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(!info.connected && info.connectedCount == 0);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(info.stickInfo.deadZoneLeft == 13);
    Require(info.stickInfo.deadZoneRight == 13);
    Require(info.connectionType == PAD_CONNECTION_TYPE_LOCAL);
    Require(info.deviceClass == PAD_DEVICE_CLASS_STANDARD);

    PadSetPortConnected_nid_postfix(0, true);
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.connected && info.connectedCount == 1);

    const int handle2 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0);
    Require(scePadGetControllerInformation(handle2, nullptr) == PAD_ERROR_INVALID_ARG);
    PadSetPortConnected_nid_postfix(1, false);
    Require(scePadGetControllerInformation(handle2, &info) == PAD_OK);
    Require(!info.connected && info.connectedCount == 0);
    PadSetPortConnected_nid_postfix(1, true);
    Require(scePadGetControllerInformation(handle2, &info) == PAD_OK);
    Require(info.connected && info.connectedCount == 1);
    Require(scePadClose_nid_postfix(handle2) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadGetControllerInformation(special, nullptr) == PAD_ERROR_INVALID_ARG);

    std::memset(&info, 0xcc, sizeof(info));
    Require(scePadGetControllerInformation(special, &info) == PAD_OK);
    Require(!info.connected && info.connectedCount == 0);
    Require(info.touchPadInfo.pixelDensity == 0.0f);
    Require(info.touchPadInfo.resolution.x == 0);
    Require(info.touchPadInfo.resolution.y == 0);
    Require(info.stickInfo.deadZoneLeft == 0);
    Require(info.stickInfo.deadZoneRight == 0);
    Require(info.connectionType == 0);
    Require(info.deviceClass == 0);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadGetControllerInformation(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerInformation(special, &info) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    return 0;
}
