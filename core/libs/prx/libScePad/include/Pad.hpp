#ifndef CORE_LIBS_PRX_LIBSCEPAD_PAD_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PAD_HPP

#include <cstdint>
#include "SceTypes.hpp"

constexpr int PAD_OK = 0;
constexpr int PAD_ERROR_INVALID_ARG = -2137915391;
constexpr int PAD_ERROR_INVALID_HANDLE = -2137915389;
constexpr int PAD_ERROR_NOT_INITIALIZED = static_cast<int>(0x80920005u);

constexpr int PAD_PORT_TYPE_STANDARD = 0;
constexpr int PAD_PORT_TYPE_SPECIAL = 2;
constexpr int PAD_PORT_TYPE_REMOTE = 16;
constexpr int PAD_USER_ID_SYSTEM = 0xff;
constexpr int PAD_MAX_CONTROLLERS = 4;
constexpr int PAD_HANDLE_BASE = 1;
constexpr int PAD_HANDLE = 1;
constexpr int PAD_REMOTE_HANDLE = 0x10;
constexpr int PAD_SPECIAL_HANDLE = 0x20;
constexpr int PAD_SPECIAL_HANDLE_COUNT = 16;

constexpr int PAD_CONNECTION_TYPE_LOCAL = 0;
constexpr int PAD_DEVICE_CLASS_STANDARD = 0;

constexpr int PAD_DEVICE_TYPE_NONE = 0;
constexpr int PAD_DEVICE_TYPE_NAVIGATOR = 1;
constexpr int PAD_DEVICE_TYPE_DUAL_SHOCK_4 = 2;
constexpr int PAD_DEVICE_TYPE_DUAL_SENSE = 3;
constexpr int PAD_DEVICE_TYPE_DUAL_SENSE_EDGE = 4;

constexpr int PAD_CONTROLLER_TYPE_UNKNOWN = 0;
constexpr int PAD_CONTROLLER_TYPE_DUAL_SENSE = 2;

constexpr int PAD_CONNECT_TYPE_UNREGISTERED = 0;
constexpr int PAD_CONNECT_TYPE_USB = 1;
constexpr int PAD_CONNECT_TYPE_BLUETOOTH = 2;

inline constexpr std::uint8_t PAD_BD_ADDRESS[6] = {0xDC, 0x8C, 0x37, 0x51, 0x01, 0xF4};

struct PadInfo {
    std::uint8_t maxConnectCount;
    std::uint8_t connectedCount[2];
    std::uint8_t reserved0[2];
    struct PadTypeInfo {
        std::uint8_t connectPort;
        std::uint8_t status;
        std::uint8_t deviceType;
        std::uint8_t connectType;
        std::uint8_t bluetoothMacAddress[6];
    } padTypeInfo[8];
    std::uint8_t reserved1[4];
};
static_assert(sizeof(PadInfo) == 0x59);

struct PadStickTuningParameter;
struct PadTriggerTuningParameter;
struct PadHuntProfile;
struct PadLeonardoProfile;
struct PadProfileInfo;
struct PadActiveProfileInfo;
struct PadSavedSettingInfo;
struct PadUsbDetectionState;
struct PadVrUsbConnectionStatus;
struct PadDeviceInfo;
struct PadLicenseControllerInformation;
struct PadRemoteSetting;
struct PadVirtualTriggerSetting;
struct PadProcessFocusParameter;
struct PadDataExtended;

struct PadVrControllerData {
    std::uint32_t buttons;
    std::uint8_t stick[2];
    std::uint8_t unused6[2];
    std::uint8_t triggers[2];
    std::uint8_t unused10[2];
    std::uint32_t touchButtons;
    std::uint8_t touchValues[4];
    float acceleration[3];
    float angularVelocity[3];
    float orientation[4];
    std::uint8_t unused60[4];
    std::uint8_t connected;
    std::uint8_t connectedCount;
    std::uint8_t unused66[6];
    std::uint64_t timestamp;
    std::uint8_t unused80[24];
};

struct PadVrControllerDeviceInformation;
struct PadVrTriggerEffect;
struct PadVrTriggerEffects;
struct PadTriggerEffectExtended;
struct PadExtensionReport;
struct PadExtensionUnitInfo;
struct PadTriggerInternalState;
struct PadTrackerData;
struct PadBlasterTrackerData;
struct PadAstonTrackerData;
struct PadUiData;
struct PadUiTuningData;
struct PadLotusButtonData;
struct PadVersionInfo;
struct PadTelemetryInfo;
struct PadPortInfo;
struct PadVrControllerCombinedState;
struct PadLeonardoData;
struct PadLeonardoUniqueData;
struct PadHistoryData;
struct PadUpdateParameter;
struct PadVirtualDeviceParameter;
struct PadVirtualDeviceId;
struct PadVirtualDeviceComponentInformation;

struct PadVirtualTriggerInternalState {
    std::uint32_t states[2];
    std::uint8_t mode;
    std::uint8_t flags[2];
};

struct PadLightBarBlinkingParameter {
    std::uint8_t values[3];
};

struct PadButtonRemappingInfo {
    std::uint8_t mapping[14];
    std::uint8_t reserved;
    std::uint8_t mode;
};

struct PadUpdateData {
    std::size_t size;
    std::uint8_t data[32768];
};

struct PadMotionSensorPosition {
    float x;
    float y;
    float z;
};

extern "C" {

int APS5_VABI scePadClose_nid_postfix(int handle);
int APS5_VABI scePadVrControllerSetVibration(std::int32_t handle, std::uint8_t intensity);
int APS5_VABI scePadVrControllerSetVibrationMode(std::int32_t handle, std::uint32_t mode);
int APS5_VABI scePadVrControllerReadCombinedState(std::int32_t leftHandle, std::int32_t rightHandle, PadVrControllerCombinedState* state);
int APS5_VABI scePadDeviceClassGetExtendedInformation(int handle, PadDeviceClassExtendedInformation* info);
int APS5_VABI scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* classData);
int APS5_VABI scePadGetControllerInformation(int handle, PadControllerInformation* info);
int APS5_VABI scePadGetHandle(int userId, int type, int index);
int APS5_VABI scePadGetInfo_nid_postfix(PadInfo* info);
int APS5_VABI scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info);
int APS5_VABI scePadGetExtControllerInformation(int handle, void* info);
int APS5_VABI scePadSetProcessPrivilege(int privilege);
int APS5_VABI scePadSetParticularMode(bool mode);
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadOpen_nid_postfix(int userId, int type, int index, const void* param);
int APS5_VABI scePadOpenExt(int userId, int type, int index, const void* param);
int APS5_VABI scePadReadExt(std::int32_t handle, PadDataExtended* data, std::int32_t count);
int APS5_VABI scePadGetFeatureReport(std::int32_t handle, std::uint8_t reportId, void* data, std::size_t size);
int APS5_VABI scePadSetFeatureReport(std::int32_t handle, std::uint8_t reportId, const void* data, std::size_t size);
int APS5_VABI scePadOutputReport(std::int32_t handle, std::uint8_t reportId, const void* data, std::uint64_t size);
int APS5_VABI scePadRead_nid_postfix(int handle, PadData* data, int num);
int APS5_VABI scePadReadState(int handle, PadData* data);
int APS5_VABI scePadResetLightBar(int handle);
int APS5_VABI scePadResetOrientation(int handle);
int APS5_VABI scePadSetAngularVelocityDeadbandState(int handle, bool enable);
int APS5_VABI scePadSetLightBar(int handle, const PadLightBarParam* param);
int APS5_VABI scePadSetMotionSensorState(int handle, bool enable);
int APS5_VABI scePadSetTiltCorrectionState(int handle, bool enabled);
int APS5_VABI scePadSetTriggerEffect(int handle, const void* param);
int APS5_VABI scePadSetVibration(int handle, const PadVibrationParam* param);
int APS5_VABI scePadSetVibrationMode(int handle, int mode);
int APS5_VABI scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool enabled);
int APS5_VABI scePadVrControllerGetDeviceInformation(std::int32_t handle, PadVrControllerDeviceInformation* info);
int APS5_VABI scePadVrControllerRead(std::int32_t handle, PadVrControllerData* data, std::int32_t count);
int APS5_VABI scePadVrControllerGetTriggerEffectState(std::int32_t leftHandle, std::int32_t rightHandle, PadTriggerEffectStateInformation* states);
int APS5_VABI scePadVrControllerReadState(int handle, PadVrControllerData* state);
int APS5_VABI scePadVrControllerSetTriggerEffect(std::int32_t handle, const PadVrTriggerEffect* effect);
int APS5_VABI scePadVrControllerSetTriggerEffects(std::int32_t leftHandle, std::int32_t rightHandle, const PadVrTriggerEffects* effects);
int APS5_VABI scePadIsRemoteController(int handle, bool* remote);
int APS5_VABI scePadSetAngularVelocityBiasCorrectionState(int handle, bool enabled);
int APS5_VABI scePadUnknown_n3kSX62fgNo();
int APS5_VABI scePadVrControllerGetTriggerEffectStates(std::int32_t leftHandle, std::int32_t rightHandle, PadTriggerEffectStateInformation* states);
int APS5_VABI scePadGetAccessibilityControllerState(int handle, std::uint32_t* state);
int APS5_VABI scePadSetTriggerEffectExt(std::int32_t handle, const PadTriggerEffectExtended* effect);
int APS5_VABI scePadLastLotusButtonData(std::int32_t handle, std::uint32_t parameter, PadLotusButtonData* data);
int APS5_VABI scePadReadStateExt(std::int32_t handle, PadDataExtended* data);
int APS5_VABI scePadSetExtensionReport(std::int32_t handle, const PadExtensionReport* report);
int APS5_VABI scePadGetExtensionUnitInfo(std::int32_t handle, PadExtensionUnitInfo* info);
int APS5_VABI scePadEnableExtensionPort(bool enabled);
int APS5_VABI scePadEnableSpecificDeviceClass(std::int32_t deviceClass);
int APS5_VABI scePadOpenExt2(std::uint64_t deviceId, std::int32_t type, const void* reserved);
int APS5_VABI scePadDeviceOpen(std::uint64_t deviceId, std::int32_t type, std::int32_t index);
int APS5_VABI scePadGetDataInternal(std::uint8_t port, PadData* data);
int APS5_VABI scePadSetUserColor(std::int32_t handle, std::uint32_t color);
int APS5_VABI scePadDisableVibration(std::int32_t handle, bool disabled);
int APS5_VABI scePadSetVibrationStrength(std::int32_t handle, std::uint32_t strength);
int APS5_VABI scePadSetLightBarBaseBrightness(std::int32_t handle, std::uint32_t brightness);
int APS5_VABI scePadSetLightBarBlinking(std::int32_t handle, const PadLightBarBlinkingParameter* parameter);
std::uint64_t APS5_VABI scePadGetIdleCount(std::int32_t userId);
int APS5_VABI scePadGetControllerIdleCount(std::int32_t handle, std::uint64_t* count);
int APS5_VABI scePadSetPowerSavingState(std::int32_t handle, std::uint8_t state);
int APS5_VABI scePadSetAutoPowerOffCount(std::int32_t count);
int APS5_VABI scePadEnableUsbConnection(bool enabled);
int APS5_VABI scePadEnableAutoDetect(bool enabled);
int APS5_VABI scePadEnablePlayPauseEmulation(bool enabled);
int APS5_VABI scePadDisableTrackingLedBatterySaver(bool disabled);
int APS5_VABI scePadSetLoginUserNumber(std::int32_t number);
int APS5_VABI scePadGetCapability(std::int32_t deviceId, std::uint32_t* capability);
int APS5_VABI scePadShareOutputData(std::int32_t source, std::int32_t destination);
int APS5_VABI scePadShareOutputDataWithFunctionType(std::int32_t source, std::int32_t destination, std::uint32_t functionType);
int APS5_VABI scePadSetVibrationForce(std::int32_t handle, const PadVibrationParam* param);
int APS5_VABI scePadGetVersionInfo(std::int32_t handle, PadVersionInfo* info);
int APS5_VABI scePadGetControllerType(std::int32_t handle, std::int32_t* type);
int APS5_VABI scePadDisableDataConversion(std::int32_t handle, bool disabled);
int APS5_VABI scePadSetButtonRemappingInfo(std::int32_t handle, const PadButtonRemappingInfo* info);
int APS5_VABI scePadSetProcessPrivilegeOfButtonRemapping(bool enabled);
int APS5_VABI scePadSetForceIntercepted(bool enabled);
int APS5_VABI scePadIsLightBarBaseBrightnessControllable(std::int32_t handle);
int APS5_VABI scePadSwitchConnection(std::int32_t handle, std::uint32_t flags);
int APS5_VABI scePadDisableTriggerEffect(std::int32_t handle, bool disabled);
int APS5_VABI scePadSetTriggerEffectStrength(std::int32_t handle, std::uint32_t strength);
int APS5_VABI scePadGetTriggerEffectInternalState(std::int32_t handle, PadTriggerInternalState* state);
int APS5_VABI scePadSetUserNumber(std::int32_t handle, std::int32_t number);
int APS5_VABI scePadDisableTransportButtons(bool disabled);
int APS5_VABI scePadSetLotusIrCodeSetStart(std::int32_t handle, std::uint8_t codeSet, std::uint16_t code, std::size_t dataSize);
int APS5_VABI scePadSetLotusIrCodeSetData(std::int32_t handle, std::uint8_t packetIndex, const void* data, std::size_t dataSize);
int APS5_VABI scePadSetLotusIrCodeSetEnd(std::int32_t handle);
int APS5_VABI scePadGetLotusIrCodeSetResult(std::int32_t handle, std::int32_t* result, std::uint8_t* deviceResult);
int APS5_VABI scePadSetLotusEnableIrKey(std::int32_t handle);
int APS5_VABI scePadSetLotusDisableIrKey(std::int32_t handle);
int APS5_VABI scePadGetUsbDetectionState(std::int32_t deviceId, PadUsbDetectionState* state);
int APS5_VABI scePadUiReadState(std::int32_t handle, PadUiData* data);
int APS5_VABI scePadUiReadStateForTuning(std::int32_t handle, PadUiTuningData* data);
int APS5_VABI scePadSetStickTriggerTuningMode(std::int32_t handle, std::uint32_t mode);
int APS5_VABI scePadSetStickTuningParam(std::int32_t handle, std::uint32_t stick, const PadStickTuningParameter* parameter);
int APS5_VABI scePadSetTriggerTuningParam(std::int32_t handle, const PadTriggerTuningParameter* parameter);
int APS5_VABI scePadGetProfileHunt(std::int32_t handle, std::int32_t profileId, PadHuntProfile* profile);
int APS5_VABI scePadSetProfileHunt(std::int32_t handle, std::int32_t profileId, PadHuntProfile* profile, std::uint32_t* result);
int APS5_VABI scePadResetHunt(std::int32_t handle);
int APS5_VABI scePadGetProfileInfo(std::int32_t deviceId, std::int32_t profileId, PadProfileInfo* info);
int APS5_VABI scePadGetActiveProfileInfo(std::int32_t handle, PadActiveProfileInfo* info);
int APS5_VABI scePadDisableActiveProfileChange(std::int32_t handle, bool disabled);
int APS5_VABI scePadDisableActiveProfileChangeByDeviceId(std::int32_t deviceId, bool disabled);
int APS5_VABI scePadSetActiveProfileSlot(std::int32_t deviceId, std::uint32_t slot);
int APS5_VABI scePadGetSavedSettingInfo(std::int32_t handle, PadSavedSettingInfo* info);
int APS5_VABI scePadSetSavedSettingInfo(std::int32_t handle, const PadSavedSettingInfo* info);
int APS5_VABI scePadVrControllerGetPhysicalUsbConnectionStatus(PadVrUsbConnectionStatus* status);
int APS5_VABI scePadParseUniqueDataLeonardo(std::int32_t handle, const PadLeonardoData* data, PadLeonardoUniqueData* uniqueData);
int APS5_VABI scePadGetControllerTypeLeonardo(std::int32_t deviceId, std::int32_t* controllerType);
int APS5_VABI scePadSetControllerTypeLeonardo(std::int32_t deviceId, std::int32_t controllerType);
int APS5_VABI scePadGetProfileLeonardo(std::int32_t handle, std::int32_t profileId, PadLeonardoProfile* profile);
int APS5_VABI scePadSetProfileLeonardo(std::int32_t handle, std::int32_t profileId, PadLeonardoProfile* profile, std::uint32_t* result);
int APS5_VABI scePadResetLeonardo(std::int32_t handle);
int APS5_VABI scePadSetProfileEditMode(std::int32_t handle, bool enabled, std::uint32_t profile);
int APS5_VABI scePadGetProfileEditMode(std::int32_t handle, std::int32_t* mode);
int APS5_VABI scePadGetProfileEditModeLeonardo(std::int32_t handle);
int APS5_VABI scePadSetSelectButtonCustomizeMode(std::int32_t handle, bool enabled);
int APS5_VABI scePadGetSelectButtonCustomizeMode(std::int32_t handle);
int APS5_VABI scePadSetToggleAllReleaseLeonardo(std::int32_t handle);
int APS5_VABI scePadEnableControllerIndicator(std::int32_t handle);
int APS5_VABI scePadEnableProfileIndicator(std::int32_t handle);
int APS5_VABI scePadSetProfileIndicatorAnimation(std::int32_t handle, std::uint8_t animation);
int APS5_VABI scePadIsCoPilot(std::int32_t handle);
int APS5_VABI scePadSetIsCoPilot(std::int32_t handle, bool enabled);
int APS5_VABI scePadRegisterCollaboInputMgrProcess(void);
int APS5_VABI scePadGetTelemetryInfo(std::int32_t handle, PadTelemetryInfo* info);
int APS5_VABI scePadGetActiveTime(std::int32_t handle, std::int32_t type, std::uint64_t* activeTime);
int APS5_VABI scePadGetDeviceId(std::int32_t handle, std::int32_t* deviceId);
int APS5_VABI scePadGetInfoByPortType(std::int32_t type, PadPortInfo* info);
int APS5_VABI scePadResetLightBarAll(void);
int APS5_VABI scePadResetLightBarAllByPortType(std::uint32_t portType);
int APS5_VABI scePadSetLightBarForTracker(std::int32_t handle, const PadLightBarParam* color);
int APS5_VABI scePadReadForTracker(std::int32_t handle, PadTrackerData* data, std::int32_t count);
int APS5_VABI scePadReadBlasterForTracker(std::int32_t handle, PadBlasterTrackerData* data, std::int32_t count);
int APS5_VABI scePadResetOrientationForTracker(std::int32_t handle);
int APS5_VABI scePadGetMotionTimerUnit(std::int32_t handle, float* unit);
int APS5_VABI scePadSetVrTrackingMode(std::uint8_t mode);
int APS5_VABI scePadGetMotionSensorPosition(std::int32_t handle, PadMotionSensorPosition* position);
int APS5_VABI scePadGetSphereRadius(std::int32_t handle, float* radius);
bool APS5_VABI scePadIsValidHandle(std::int32_t handle, std::int32_t portType);
bool APS5_VABI scePadIsDS4Connected(std::int32_t handle);
bool APS5_VABI scePadIsBlasterConnected(std::int32_t handle);
bool APS5_VABI scePadIsMoveConnected(std::int32_t handle);
int APS5_VABI scePadGetBluetoothAddress(std::int32_t handle, std::uint64_t* address);
int APS5_VABI scePadIsMoveReproductionModel(std::int32_t handle);
int APS5_VABI scePadReadAstonForTracker(std::int32_t handle, PadAstonTrackerData* data, std::int32_t count);
int APS5_VABI scePadGetAstonModel(std::int32_t handle);
int APS5_VABI scePadMbusInit(void);
int APS5_VABI scePadMbusTerm(void);
int APS5_VABI scePadConnectPort(std::int32_t userId, std::int32_t portType);
int APS5_VABI scePadDisconnectPort(std::int32_t userId, std::int32_t portType);
int APS5_VABI scePadDisconnectDevice(std::int32_t deviceId);
int APS5_VABI scePadGetDeviceInfo(std::int32_t deviceId, PadDeviceInfo* info);
int APS5_VABI scePadSetProcessFocus(const PadProcessFocusParameter* parameter);
int APS5_VABI scePadVertualDeviceAddDevice(const PadVirtualDeviceParameter* parameter, PadVirtualDeviceId* deviceId);
int APS5_VABI scePadVirtualDeviceAddDevice(const PadVirtualDeviceParameter* parameter, PadVirtualDeviceId* deviceId);
int APS5_VABI scePadVirtualDeviceDeleteDevice(std::int32_t deviceId);
int APS5_VABI scePadVirtualDeviceInsertData(std::int32_t deviceId, const PadData* data);
int APS5_VABI scePadVirtualDeviceInsertTriggerInternalState(std::int32_t deviceId, const PadVirtualTriggerInternalState* state);
int APS5_VABI scePadVirtualDeviceGetRemoteSetting(std::int32_t deviceId, PadRemoteSetting* setting);
int APS5_VABI scePadVirtualDeviceSetTriggerEffectSetting(std::int32_t handle, const PadVirtualTriggerSetting* setting);
int APS5_VABI scePadVirtualDeviceDisableButtonRemapping(bool disabled);
int APS5_VABI scePadSetShareButtonMaskForRemotePlay(std::uint8_t mask);
int APS5_VABI scePadSetPS4BcVibrationMode(std::uint8_t mode);
int APS5_VABI scePadSetVirtualDeviceComponentInformation(std::int32_t deviceId, const PadVirtualDeviceComponentInformation* info);
int APS5_VABI scePadEnablePs4CompatibleMode(std::int32_t handle, bool enabled);
int APS5_VABI scePadGetLicenseControllerInformation(std::int32_t handle, PadLicenseControllerInformation* info);
int APS5_VABI scePadReadHistory(std::int32_t handle, PadHistoryData* data, std::int32_t count);
int APS5_VABI scePadStartRecording(void);
int APS5_VABI scePadStopRecording(void);
int APS5_VABI scePadRemoteControlInsertData(const std::uint8_t* data);
int APS5_VABI scePadRemoteControlInsertDataForAston(const std::uint8_t* data);
int APS5_VABI scePadGetFeatureReport2(std::int32_t handle, std::uint8_t reportId, void* data, std::size_t size);
int APS5_VABI scePadSetFeatureReport2(std::int32_t handle, std::uint8_t reportId, const void* data, std::size_t size);
int APS5_VABI scePadSetMuteLed(std::int32_t handle, std::uint32_t mode);
int APS5_VABI scePadBondUpdateInit(void);
int APS5_VABI scePadBondUpdateIsReady(std::int32_t deviceId, std::int32_t* ready);
int APS5_VABI scePadBondUpdateStart(std::int32_t handle, const PadUpdateParameter* parameter);
int APS5_VABI scePadBondUpdateGetStatus(std::int32_t deviceId, std::int32_t* status);
int APS5_VABI scePadBondUpdateSendData(std::int32_t deviceId, const PadUpdateData* data);
int APS5_VABI scePadBondUpdateResetBond(std::int32_t deviceId);
int APS5_VABI scePadBondUpdateGetReconnectStatus(std::int32_t* status);
int APS5_VABI scePadBondUpdateTerm(void);
int APS5_VABI scePadControllerUpdateInit(void);
int APS5_VABI scePadControllerUpdateIsUpdateNeeded2(std::int32_t deviceId, const std::uint32_t* version, std::uint16_t* status);
int APS5_VABI scePadControllerUpdateIsReady(std::int32_t deviceId, std::int32_t* ready);
int APS5_VABI scePadControllerUpdateStart(std::int32_t handle, const PadUpdateParameter* parameter);
int APS5_VABI scePadControllerUpdateGetStatus(std::int32_t deviceId, std::int32_t* status);
int APS5_VABI scePadControllerUpdateSendData(std::int32_t deviceId, const PadUpdateData* data);
int APS5_VABI scePadControllerUpdateResetController(std::int32_t deviceId);
int APS5_VABI scePadControllerUpdateGetReconnectStatus(std::int32_t* status);
int APS5_VABI scePadControllerUpdateTerm(void);

}

#endif
