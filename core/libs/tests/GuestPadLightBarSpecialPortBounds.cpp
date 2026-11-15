#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    constexpr int kInvalidUserId = static_cast<int>(0x809b0001);
    constexpr int kInvalidPortIndex = static_cast<int>(0x809b0003);
    constexpr int kAlreadyOpened = static_cast<int>(0x80920004);
    constexpr int kDeviceNotConnected = static_cast<int>(0x80920007);
    constexpr int kDeviceNoHandle = static_cast<int>(0x80920008);
    constexpr int kUnknownUser = static_cast<int>(0x809b0081);
    constexpr int kNotPermitted = static_cast<int>(0x80920101);

    for (int handle : {-1, 0, PAD_HANDLE, PAD_HANDLE_BASE + 1, PAD_SPECIAL_HANDLE, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadSetLightBarBaseBrightness(handle, 128) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_REMOTE));
        Require(scePadGetExtControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    PadData preInitData{};
    Require(scePadRead_nid_postfix(PAD_SPECIAL_HANDLE, &preInitData, 1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadReadState(PAD_SPECIAL_HANDLE, &preInitData) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1, 0, PAD_HANDLE, PAD_SPECIAL_HANDLE, 12345}) {
        Require(scePadSetLightBarBaseBrightness(handle, 255) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
    }

    Require(scePadOpen_nid_postfix(PAD_USER_ID_INVALID, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == kInvalidUserId);
    Require(scePadOpen_nid_postfix(0x20000000, PAD_PORT_TYPE_SPECIAL, 1, nullptr) == kInvalidPortIndex);
    Require(scePadOpen_nid_postfix(0x20000000, PAD_PORT_TYPE_SPECIAL, -1, nullptr) == kInvalidPortIndex);
    for (int badType : {1, 3, 4, 5, 6, 255, -1}) {
        Require(scePadOpen_nid_postfix(0x20000000, badType, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    }

    Require(scePadGetHandle(0x20000000, PAD_PORT_TYPE_SPECIAL, 0) == kDeviceNoHandle);
    Require(scePadGetHandle(0x20000000, PAD_PORT_TYPE_SPECIAL, 1) == kDeviceNoHandle);
    Require(scePadGetHandle(PAD_USER_ID_INVALID, PAD_PORT_TYPE_SPECIAL, 0) == kDeviceNoHandle);

    std::array<int, PAD_SPECIAL_HANDLE_COUNT> specialHandles{};
    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
        const int user = 0x20000000 + slot;
        const int h = scePadOpen_nid_postfix(user, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
        Require(h == PAD_SPECIAL_HANDLE + slot);
        specialHandles[slot] = h;
        Require(scePadGetHandle(user, PAD_PORT_TYPE_SPECIAL, 0) == h);
        Require(scePadIsValidHandle(h, PAD_PORT_TYPE_SPECIAL));
        Require(!scePadIsValidHandle(h, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(h, PAD_PORT_TYPE_REMOTE));
    }

    Require(scePadOpen_nid_postfix(0x20000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == kAlreadyOpened);
    Require(scePadOpen_nid_postfix(0x2000000f, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == kAlreadyOpened);
    Require(scePadOpen_nid_postfix(0x20000010, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == kUnknownUser);

    int dummyParam = 0;
    Require(scePadOpenExt(0x20000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(0x20000000, PAD_PORT_TYPE_SPECIAL, 1, &dummyParam) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(PAD_USER_ID_INVALID, PAD_PORT_TYPE_SPECIAL, 0, &dummyParam) == PAD_ERROR_INVALID_ARG);
    Require(scePadOpenExt(0x20000000, PAD_PORT_TYPE_SPECIAL, 0, &dummyParam) == kDeviceNotConnected);

    const int stdHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(stdHandle == PAD_HANDLE_BASE);
    Require(scePadIsValidHandle(stdHandle, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(stdHandle, PAD_PORT_TYPE_SPECIAL));

    Require(scePadIsLightBarBaseBrightnessControllable(stdHandle) == 1);
    for (std::uint32_t brightness : {0u, 1u, 50u, 128u, 255u, 1000u}) {
        Require(scePadSetLightBarBaseBrightness(stdHandle, brightness) == PAD_OK);
    }
    Require(scePadIsLightBarBaseBrightnessControllable(stdHandle) == 1);

    for (int h : specialHandles) {
        Require(scePadIsLightBarBaseBrightnessControllable(h) == 0);
        for (std::uint32_t brightness : {0u, 1u, 128u, 255u}) {
            Require(scePadSetLightBarBaseBrightness(h, brightness) == PAD_OK);
        }
        Require(scePadIsLightBarBaseBrightnessControllable(h) == 0);
    }

    const int spHandle = specialHandles[0];
    PadData readData{};
    Require(scePadRead_nid_postfix(spHandle, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(spHandle, &readData, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(spHandle, &readData, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(spHandle, &readData, 1) == 1);
    Require(readData.left_stick_x == 0x80);
    Require(readData.left_stick_y == 0x80);
    Require(readData.right_stick_x == 0x80);
    Require(readData.right_stick_y == 0x80);
    Require(readData.orientation_w == 1.0f);
    Require(readData.acceleration_y == 1.0f);
    Require(readData.connected == 0);
    Require(readData.buttons == 0);

    PadData stateData{};
    Require(scePadReadState(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadReadState(spHandle, &stateData) == PAD_OK);
    Require(stateData.left_stick_x == 0x80);
    Require(stateData.left_stick_y == 0x80);
    Require(stateData.right_stick_x == 0x80);
    Require(stateData.right_stick_y == 0x80);
    Require(stateData.orientation_w == 1.0f);
    Require(stateData.acceleration_y == 1.0f);
    Require(stateData.connected == 0);
    Require(stateData.buttons == 0);

    PadControllerInformation ctrlInfo{};
    std::memset(&ctrlInfo, 0xab, sizeof(ctrlInfo));
    Require(scePadGetControllerInformation(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerInformation(spHandle, &ctrlInfo) == PAD_OK);
    const auto* infoBytes = reinterpret_cast<const std::uint8_t*>(&ctrlInfo);
    for (std::size_t i = 0; i < sizeof(ctrlInfo); ++i) {
        Require(infoBytes[i] == 0);
    }

    PadDeviceClassExtendedInformation devClassExt{};
    Require(scePadDeviceClassGetExtendedInformation(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassGetExtendedInformation(spHandle, &devClassExt) == PAD_OK);
    Require(devClassExt.deviceClass == -1);

    PadDeviceClassData devClassData{};
    Require(scePadDeviceClassParseData(spHandle, nullptr, &devClassData) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassParseData(spHandle, &readData, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadDeviceClassParseData(spHandle, &readData, &devClassData) == PAD_OK);
    Require(devClassData.deviceClass == 0);
    Require(devClassData.dataValid == 0);

    alignas(8) std::array<std::uint8_t, 0x40> extInfoBuf{};
    extInfoBuf.fill(0xee);
    Require(scePadGetExtControllerInformation(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetExtControllerInformation(stdHandle, extInfoBuf.data()) == kNotPermitted);
    Require(scePadGetExtControllerInformation(spHandle, extInfoBuf.data()) == PAD_OK);
    for (std::size_t i = 0; i < 0x2c; ++i) {
        Require(extInfoBuf[i] == 0);
    }
    for (std::size_t i = 0x2c; i < extInfoBuf.size(); ++i) {
        Require(extInfoBuf[i] == 0xee);
    }

    int ctrlType = -1;
    Require(scePadGetControllerType(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerType(spHandle, &ctrlType) == PAD_OK);
    Require(ctrlType == PAD_CONTROLLER_TYPE_UNKNOWN);
    Require(scePadGetControllerType(stdHandle, &ctrlType) == PAD_OK);
    Require(ctrlType == PAD_CONTROLLER_TYPE_DUAL_SENSE);

    PadTriggerEffectStateInformation triggerState{};
    Require(scePadGetTriggerEffectState(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetTriggerEffectState(spHandle, &triggerState) == PAD_OK);
    Require(triggerState.state[0] == 0 && triggerState.state[1] == 0);

    int busType = -1;
    Require(scePadGetControllerBusType(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerBusType(spHandle, &busType) == kDeviceNotConnected);
    int jack = -1;
    Require(scePadGetJackState(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetJackState(spHandle, &jack) == kDeviceNotConnected);
    Require(scePadIsControllerUpdateRequired(spHandle) == kDeviceNotConnected);
    PadContainerIdInformation container{};
    Require(scePadGetContainerIdInformation(spHandle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetContainerIdInformation(spHandle, &container) == kDeviceNotConnected);

    const PadLightBarParam lbParam{255, 128, 64};
    Require(scePadSetLightBar(spHandle, &lbParam) == PAD_OK);
    Require(scePadResetLightBar(spHandle) == PAD_OK);
    Require(scePadResetOrientation(spHandle) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(spHandle, true) == PAD_OK);
    Require(scePadSetAngularVelocityDeadbandState(spHandle, false) == PAD_OK);
    Require(scePadDisableTriggerEffect(spHandle, true) == PAD_OK);
    Require(scePadDisableTriggerEffect(spHandle, false) == PAD_OK);
    const PadVibrationParam vibParam{120, 240};
    Require(scePadSetVibration(spHandle, &vibParam) == PAD_OK);
    Require(scePadSetVibrationForce(spHandle, &vibParam) == PAD_OK);
    Require(scePadDisableVibration(spHandle, true) == PAD_OK);
    Require(scePadDisableVibration(spHandle, false) == PAD_OK);
    Require(scePadSetMotionSensorState(spHandle, true) == PAD_OK);
    Require(scePadSetMotionSensorState(spHandle, false) == PAD_OK);
    Require(scePadSetTiltCorrectionState(spHandle, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(spHandle, false) == PAD_OK);
    Require(scePadSetUserColor(spHandle, 0x00ff00) == PAD_OK);
    Require(scePadSetLightBarForTracker(spHandle, &lbParam) == PAD_OK);
    Require(scePadResetOrientationForTracker(spHandle) == PAD_OK);
    Require(scePadSetAudioOutPath(spHandle, 0) == PAD_OK);
    const PadVolumeGain gain{80, 80, 0, 80};
    Require(scePadSetVolumeGain(spHandle, &gain) == PAD_OK);

    const int closeTarget = specialHandles[3];
    const int closeUser = 0x20000000 + 3;
    Require(scePadClose_nid_postfix(closeTarget) == PAD_OK);
    Require(scePadClose_nid_postfix(closeTarget) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadSetLightBarBaseBrightness(closeTarget, 100) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadIsLightBarBaseBrightnessControllable(closeTarget) == PAD_ERROR_INVALID_HANDLE);
    Require(!scePadIsValidHandle(closeTarget, PAD_PORT_TYPE_SPECIAL));
    Require(scePadGetHandle(closeUser, PAD_PORT_TYPE_SPECIAL, 0) == kDeviceNoHandle);
    Require(scePadRead_nid_postfix(closeTarget, &readData, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadReadState(closeTarget, &stateData) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerInformation(closeTarget, &ctrlInfo) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetExtControllerInformation(closeTarget, extInfoBuf.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDeviceClassGetExtendedInformation(closeTarget, &devClassExt) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDeviceClassParseData(closeTarget, &readData, &devClassData) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetTriggerEffectState(closeTarget, &triggerState) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerType(closeTarget, &ctrlType) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(closeTarget, &lbParam) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(closeTarget) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientation(closeTarget) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAngularVelocityDeadbandState(closeTarget, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(closeTarget, &vibParam) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableVibration(closeTarget, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadDisableTriggerEffect(closeTarget, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetUserColor(closeTarget, 0) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetAudioOutPath(closeTarget, 0) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVolumeGain(closeTarget, &gain) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarForTracker(closeTarget, &lbParam) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetOrientationForTracker(closeTarget) == PAD_ERROR_INVALID_HANDLE);

    const int recycled = scePadOpen_nid_postfix(0x30000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(recycled == closeTarget);
    Require(scePadIsValidHandle(recycled, PAD_PORT_TYPE_SPECIAL));
    Require(scePadGetHandle(0x30000000, PAD_PORT_TYPE_SPECIAL, 0) == recycled);
    Require(scePadClose_nid_postfix(recycled) == PAD_OK);

    for (int slot = 0; slot < PAD_SPECIAL_HANDLE_COUNT; ++slot) {
        if (slot == 3) {
            continue;
        }
        Require(scePadClose_nid_postfix(specialHandles[slot]) == PAD_OK);
        Require(!scePadIsValidHandle(specialHandles[slot], PAD_PORT_TYPE_SPECIAL));
    }

    Require(scePadClose_nid_postfix(stdHandle) == PAD_OK);
    Require(!scePadIsValidHandle(stdHandle, PAD_PORT_TYPE_STANDARD));
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == kDeviceNoHandle);

    float timerUnit = 1.0f;
    Require(scePadGetMotionTimerUnit(stdHandle, &timerUnit) == PAD_ERROR_INVALID_HANDLE);
    PadMotionSensorPosition sensorPos{};
    Require(scePadGetMotionSensorPosition(stdHandle, &sensorPos) == PAD_ERROR_INVALID_HANDLE);
    std::uint64_t btAddr = 0;
    Require(scePadGetBluetoothAddress(stdHandle, &btAddr) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadSetLightBarBaseBrightness(stdHandle, 75) == PAD_OK);
    Require(scePadIsLightBarBaseBrightnessControllable(stdHandle) == 1);
    Require(scePadSetLightBar(stdHandle, &lbParam) == PAD_OK);
    Require(scePadResetLightBar(stdHandle) == PAD_OK);
    Require(scePadSetVibration(stdHandle, &vibParam) == PAD_OK);
    Require(scePadDisableVibration(stdHandle, false) == PAD_OK);
    int closedStdType = -1;
    Require(scePadGetControllerType(stdHandle, &closedStdType) == PAD_OK);
    Require(closedStdType == PAD_CONTROLLER_TYPE_DUAL_SENSE);

    for (int badHandle : {-100, -1, 0, PAD_HANDLE_BASE + 4, PAD_SPECIAL_HANDLE + 16, 99999}) {
        Require(scePadClose_nid_postfix(badHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(badHandle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(badHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(!scePadIsValidHandle(badHandle, PAD_PORT_TYPE_STANDARD));
        Require(!scePadIsValidHandle(badHandle, PAD_PORT_TYPE_SPECIAL));
        Require(!scePadIsValidHandle(badHandle, PAD_PORT_TYPE_REMOTE));
    }

    return 0;
}
