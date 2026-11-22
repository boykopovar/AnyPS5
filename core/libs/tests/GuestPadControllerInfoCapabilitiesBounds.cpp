#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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
static_assert(offsetof(PadControllerInformation, pad) == 13);
static_assert(offsetof(PadControllerInformation, deviceClass) == 16);
static_assert(offsetof(PadControllerInformation, reserve) == 20);

static_assert(sizeof(PadDeviceClassExtendedInformation) == 20);
static_assert(offsetof(PadDeviceClassExtendedInformation, deviceClass) == 0);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    constexpr int errorNotInitialized = static_cast<int>(0x80920005u);
    constexpr int errorInvalidArg = -2137915391;
    constexpr int errorInvalidHandle = -2137915389;
    constexpr int errorDeviceNotConnected = static_cast<int>(0x80920007u);
    constexpr int errorNotPermitted = static_cast<int>(0x80920101u);

    PadControllerInformation preInfo{};
    for (int handle : {-9999, -1, 0, 1, 2, 0x10, 0x20, 9999}) {
        Require(scePadGetControllerInformation(handle, &preInfo) == errorInvalidHandle);
        Require(scePadGetControllerInformation(handle, nullptr) == errorInvalidHandle);
    }

    int preType = -1;
    Require(scePadGetControllerType(1, &preType) == errorNotInitialized && preType == -1);
    Require(scePadGetControllerType(1, nullptr) == errorNotInitialized);
    Require(scePadGetControllerType(-1, &preType) == errorNotInitialized);

    Require(scePadIsSupportedAudioFunction(1) == errorNotInitialized);
    Require(scePadIsSupportedAudioFunction(-1) == errorNotInitialized);

    std::uint64_t preBaddr = 0;
    Require(scePadGetBluetoothAddress(1, &preBaddr) == errorNotInitialized);
    Require(scePadGetBluetoothAddress(1, nullptr) == errorNotInitialized);

    std::array<std::uint8_t, 64> unitBuf{};
    Require(scePadGetExtensionUnitInfo(1, reinterpret_cast<PadExtensionUnitInfo*>(unitBuf.data())) == errorNotInitialized);
    Require(scePadGetExtensionUnitInfo(1, nullptr) == errorNotInitialized);

    std::uint32_t preAcc = 999;
    Require(scePadGetAccessibilityControllerState(1, &preAcc) == errorNotInitialized && preAcc == 999);
    Require(scePadGetAccessibilityControllerState(1, nullptr) == errorNotInitialized);

    std::uint32_t extParam = 0;
    Require(scePadOpenExt(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == errorInvalidArg);
    Require(scePadOpenExt(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, &extParam) == errorDeviceNotConnected);

    std::array<std::uint8_t, 0x10> ext2Param{};
    Require(scePadOpenExt2(0x10000000, 0x10, ext2Param.data()) == errorNotInitialized);

    std::uint32_t preCap = 0;
    Require(scePadGetCapability(0, &preCap) == errorNotInitialized);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    for (int handle : {-9999, -1, 0, 1, 0x20, 9999}) {
        std::array<std::uint8_t, 64> extCheckBuf{};
        Require(scePadGetExtControllerInformation(handle, nullptr) == errorInvalidHandle);
        Require(scePadGetExtControllerInformation(handle, extCheckBuf.data()) == errorInvalidHandle);
    }

    Require(scePadGetExtensionUnitInfo(1, nullptr) == errorInvalidArg);
    bool threwExtensionUnit = false;
    try {
        scePadGetExtensionUnitInfo(1, reinterpret_cast<PadExtensionUnitInfo*>(unitBuf.data()));
    } catch (const std::runtime_error&) {
        threwExtensionUnit = true;
    }
    Require(threwExtensionUnit);

    bool threwExtensionReport = false;
    try {
        scePadSetExtensionReport(1, nullptr);
    } catch (const std::runtime_error&) {
        threwExtensionReport = true;
    }
    Require(threwExtensionReport);

    Require(scePadGetCapability(0, nullptr) == errorInvalidArg);
    bool threwCapability = false;
    try {
        std::uint32_t c = 0;
        scePadGetCapability(0, &c);
    } catch (const std::runtime_error&) {
        threwCapability = true;
    }
    Require(threwCapability);

    bool threwLicense = false;
    try {
        scePadGetLicenseControllerInformation(1, nullptr);
    } catch (const std::runtime_error&) {
        threwLicense = true;
    }
    Require(threwLicense);

    Require(scePadOpenExt(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr) == errorInvalidArg);
    Require(scePadOpenExt(0x10000000, PAD_PORT_TYPE_SPECIAL, 1, &extParam) == errorInvalidArg);
    Require(scePadOpenExt(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, &extParam) == errorDeviceNotConnected);
    bool threwOpenExt = false;
    try {
        scePadOpenExt(0x10000000, PAD_PORT_TYPE_STANDARD, 0, &extParam);
    } catch (const std::runtime_error&) {
        threwOpenExt = true;
    }
    Require(threwOpenExt);

    Require(scePadOpenExt2(0x10000000, 0x08, ext2Param.data()) == errorInvalidArg);
    Require(scePadOpenExt2(0x10000000, 0x10, nullptr) == errorInvalidArg);
    bool threwOpenExt2 = false;
    try {
        scePadOpenExt2(0x10000000, 0x10, ext2Param.data());
    } catch (const std::runtime_error&) {
        threwOpenExt2 = true;
    }
    Require(threwOpenExt2);

    const int standard = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(standard > 0);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadIsValidHandle(standard, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(standard, PAD_PORT_TYPE_SPECIAL));
    Require(scePadIsValidHandle(special, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(special, PAD_PORT_TYPE_STANDARD));

    Require(scePadGetControllerInformation(standard, nullptr) == errorInvalidArg);
    Require(scePadGetControllerInformation(special, nullptr) == errorInvalidArg);

    PadSetPortConnected_nid_postfix(0, true);
    PadControllerInformation standardInfo{};
    std::memset(&standardInfo, 0xaa, sizeof(standardInfo));
    Require(scePadGetControllerInformation(standard, &standardInfo) == PAD_OK);
    Require(standardInfo.touchPadInfo.pixelDensity == 44.86f);
    Require(standardInfo.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(standardInfo.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(standardInfo.stickInfo.deadZoneLeft == 13);
    Require(standardInfo.stickInfo.deadZoneRight == 13);
    Require(standardInfo.connectionType == PAD_CONNECTION_TYPE_LOCAL);
    Require(standardInfo.connectedCount == 1);
    Require(standardInfo.connected == true);
    Require(standardInfo.deviceClass == PAD_DEVICE_CLASS_STANDARD);
    Require(standardInfo.pad[0] == 0 && standardInfo.pad[1] == 0 && standardInfo.pad[2] == 0);
    for (std::size_t i = 0; i < sizeof(standardInfo.reserve); ++i) {
        Require(standardInfo.reserve[i] == 0);
    }

    PadSetPortConnected_nid_postfix(0, false);
    std::memset(&standardInfo, 0x55, sizeof(standardInfo));
    Require(scePadGetControllerInformation(standard, &standardInfo) == PAD_OK);
    Require(!standardInfo.connected);
    Require(standardInfo.connectedCount == 0);
    Require(standardInfo.deviceClass == PAD_DEVICE_CLASS_STANDARD);
    Require(standardInfo.touchPadInfo.pixelDensity == 44.86f);
    Require(standardInfo.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(standardInfo.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(standardInfo.stickInfo.deadZoneLeft == 13);
    Require(standardInfo.stickInfo.deadZoneRight == 13);
    PadSetPortConnected_nid_postfix(0, true);

    PadControllerInformation specialInfo{};
    std::memset(&specialInfo, 0xff, sizeof(specialInfo));
    Require(scePadGetControllerInformation(special, &specialInfo) == PAD_OK);
    const PadControllerInformation zeroExpected{};
    Require(std::memcmp(&specialInfo, &zeroExpected, sizeof(PadControllerInformation)) == 0);

    Require(scePadGetControllerType(standard, nullptr) == errorInvalidArg);
    Require(scePadGetControllerType(special, nullptr) == errorInvalidArg);
    Require(scePadGetControllerType(-1, nullptr) == errorInvalidArg);

    int ctrlType = -1;
    Require(scePadGetControllerType(-1, &ctrlType) == errorInvalidHandle);
    Require(scePadGetControllerType(standard, &ctrlType) == PAD_OK && ctrlType == PAD_CONTROLLER_TYPE_DUAL_SENSE);
    ctrlType = -1;
    Require(scePadGetControllerType(special, &ctrlType) == PAD_OK && ctrlType == PAD_CONTROLLER_TYPE_UNKNOWN);

    Require(!scePadIsDS4Connected(standard));
    Require(!scePadIsBlasterConnected(standard));
    Require(!scePadIsMoveConnected(standard));
    Require(!scePadIsDS4Connected(special));
    Require(!scePadIsBlasterConnected(special));
    Require(!scePadIsMoveConnected(special));
    Require(!scePadIsDS4Connected(-1));
    Require(!scePadIsBlasterConnected(0));
    Require(!scePadIsMoveConnected(9999));

    Require(scePadIsRemoteController(-1, nullptr) == errorInvalidHandle);
    Require(scePadIsRemoteController(standard, nullptr) == errorInvalidArg);
    bool remoteFlag = true;
    Require(scePadIsRemoteController(standard, &remoteFlag) == PAD_OK && !remoteFlag);
    remoteFlag = true;
    Require(scePadIsRemoteController(special, &remoteFlag) == PAD_OK && !remoteFlag);

    Require(scePadIsLightBarBaseBrightnessControllable(-1) == errorInvalidHandle);
    Require(scePadIsLightBarBaseBrightnessControllable(standard) == 1);
    Require(scePadIsLightBarBaseBrightnessControllable(special) == 0);

    Require(scePadIsSupportedAudioFunction(-1) == errorInvalidHandle);
    Require(scePadIsSupportedAudioFunction(special) == errorDeviceNotConnected);
    Require(scePadIsSupportedAudioFunction(standard) == 1);
    PadSetPortConnected_nid_postfix(0, false);
    Require(scePadIsSupportedAudioFunction(standard) == errorDeviceNotConnected);
    PadSetPortConnected_nid_postfix(0, true);
    Require(scePadIsSupportedAudioFunction(standard) == 1);

    PadDeviceClassExtendedInformation devClassInfo{};
    Require(scePadDeviceClassGetExtendedInformation(-1, &devClassInfo) == errorInvalidHandle);
    Require(scePadDeviceClassGetExtendedInformation(standard, nullptr) == errorInvalidArg);
    Require(scePadDeviceClassGetExtendedInformation(standard, &devClassInfo) == PAD_OK);
    Require(devClassInfo.deviceClass == PAD_DEVICE_CLASS_STANDARD);
    Require(scePadDeviceClassGetExtendedInformation(special, &devClassInfo) == PAD_OK);
    Require(devClassInfo.deviceClass == -1);

    PadData sampleData{};
    sampleData.connected = true;
    PadDeviceClassData classParsed{};
    Require(scePadDeviceClassParseData(-1, &sampleData, &classParsed) == errorInvalidHandle);
    Require(scePadDeviceClassParseData(standard, nullptr, &classParsed) == errorInvalidArg);
    Require(scePadDeviceClassParseData(standard, &sampleData, nullptr) == errorInvalidArg);
    Require(scePadDeviceClassParseData(standard, &sampleData, &classParsed) == PAD_OK);
    Require(classParsed.deviceClass == PAD_DEVICE_CLASS_STANDARD && classParsed.dataValid);
    sampleData.connected = false;
    Require(scePadDeviceClassParseData(standard, &sampleData, &classParsed) == PAD_OK);
    Require(!classParsed.dataValid);
    Require(scePadDeviceClassParseData(special, &sampleData, &classParsed) == PAD_OK);
    Require(classParsed.deviceClass == 0 && !classParsed.dataValid);

    Require(scePadGetExtControllerInformation(-1, nullptr) == errorInvalidHandle);
    Require(scePadGetExtControllerInformation(standard, nullptr) == errorInvalidArg);
    Require(scePadGetExtControllerInformation(special, nullptr) == errorInvalidArg);

    std::array<std::uint8_t, 64> extBuf{};
    extBuf.fill(0xee);
    Require(scePadGetExtControllerInformation(standard, extBuf.data()) == errorNotPermitted);
    for (std::uint8_t b : extBuf) {
        Require(b == 0xee);
    }

    extBuf.fill(0xee);
    Require(scePadGetExtControllerInformation(special, extBuf.data()) == PAD_OK);
    for (std::size_t i = 0; i < 0x2c; ++i) {
        Require(extBuf[i] == 0);
    }
    for (std::size_t i = 0x2c; i < extBuf.size(); ++i) {
        Require(extBuf[i] == 0xee);
    }

    std::uint32_t acc = 888;
    Require(scePadGetAccessibilityControllerState(standard, nullptr) == errorInvalidArg);
    Require(scePadGetAccessibilityControllerState(-1, &acc) == errorInvalidHandle);
    Require(scePadGetAccessibilityControllerState(standard, &acc) == PAD_OK && acc == 0);
    acc = 888;
    Require(scePadGetAccessibilityControllerState(special, &acc) == PAD_OK && acc == 0);

    std::uint64_t baddr = 0;
    Require(scePadGetBluetoothAddress(standard, nullptr) == errorInvalidArg);
    Require(scePadGetBluetoothAddress(-1, &baddr) == errorInvalidHandle);
    Require(scePadGetBluetoothAddress(standard, &baddr) == errorDeviceNotConnected);
    Require(scePadGetBluetoothAddress(special, &baddr) == errorDeviceNotConnected);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == errorInvalidHandle);
    Require(scePadGetControllerInformation(special, &specialInfo) == errorInvalidHandle);
    Require(scePadGetExtControllerInformation(special, extBuf.data()) == errorInvalidHandle);
    Require(scePadGetControllerType(special, &ctrlType) == errorInvalidHandle);
    Require(scePadIsLightBarBaseBrightnessControllable(special) == errorInvalidHandle);
    Require(scePadIsRemoteController(special, &remoteFlag) == errorInvalidHandle);
    Require(scePadGetAccessibilityControllerState(special, &acc) == errorInvalidHandle);

    Require(scePadClose_nid_postfix(standard) == PAD_OK);
    Require(scePadGetControllerInformation(standard, &standardInfo) == PAD_OK);
    Require(scePadGetControllerType(standard, &ctrlType) == PAD_OK && ctrlType == PAD_CONTROLLER_TYPE_DUAL_SENSE);
    Require(scePadIsLightBarBaseBrightnessControllable(standard) == 1);
    Require(scePadGetExtControllerInformation(standard, extBuf.data()) == errorNotPermitted);
    Require(scePadGetBluetoothAddress(standard, &baddr) == errorInvalidHandle);

    return 0;
}
