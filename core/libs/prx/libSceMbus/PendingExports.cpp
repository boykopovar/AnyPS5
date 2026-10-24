#include "prx/libc/include/General.hpp"

#include <cstddef>

struct MbusDeviceFunctionState {
    std::byte data[180];
};

static_assert(sizeof(MbusDeviceFunctionState) == 180);
struct MbusDeviceDescription;
struct MbusDeviceInfo;
struct MbusSimulatedHandleStatus;
struct MbusSparkState;
struct MbusDriverCallbacks;
struct MbusUsbPowerEntry;
struct MbusEvent;
struct alignas(8) MbusDeviceCondition {
    std::byte data[256];
};

static_assert(sizeof(MbusDeviceCondition) == 256);
struct MbusDeviceServiceInfo;
struct MbusEventCallbacks {
    void (APS5_VABI *onOverflow)(std::int32_t eventHandle, void* userData);
    void (APS5_VABI *onEvent)(std::int32_t eventHandle, void* userData, const MbusEvent* event);
};

static_assert(sizeof(MbusEventCallbacks) == 16);
struct MbusProcessInfo;
struct MbusStartupInfo;
struct MbusStartupParam;
struct MbusControlStatus;
struct MbusDebugDeviceInfo;
struct MbusInternalInfo;
struct MbusPriorityInfo;

extern "C" {

int APS5_VABI sceMbusAcquireControl(std::uint64_t control, std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAcquireControlList() {
    return static_cast<int>(0x809B00FF);
}

int APS5_VABI sceMbusAcquireControlWithState(std::uint32_t deviceClass, std::int32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAcquireControlWithState2(std::uint32_t deviceClass, std::int32_t moduleId, std::int32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAcquireControlWithStateFlag(std::uint32_t deviceClass, std::uint32_t moduleId, std::uint32_t state, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAddHandle(std::uint32_t deviceClass, std::int32_t handle, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAddHandleByDeviceId(std::uint32_t deviceClass, std::int32_t handle, std::uint64_t deviceId, std::uint32_t first, std::uint32_t second, std::uint32_t third) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAddHandleByUserId(std::uint32_t deviceClass, std::int32_t handle, std::int32_t userId, std::int32_t first, std::int32_t second, std::int32_t third) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAddHandleByVidPid(std::uint32_t deviceClass, std::int32_t handle, std::int32_t first, std::uint16_t vendorId, std::uint16_t productId, std::int32_t second, std::int32_t third) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAddHandleWithModuleId(std::uint32_t deviceClass, std::int32_t handle, std::uint32_t flags, std::uint32_t moduleId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusAttachToApplicationByPid(std::int32_t processId, std::int32_t deviceClass, std::int32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusBindDeviceWithUserId(std::uint64_t deviceId, std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusChangeAppModuleState(std::uint32_t deviceClass, std::int32_t moduleId, std::uint32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusCheckDriverCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusConvertToLocalDeviceId(std::uint64_t deviceId, std::uint32_t* localId, std::uint32_t* deviceClass) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusConvertToLocalDeviceId2(std::uint64_t deviceId, std::uint32_t deviceClass, std::uint64_t* localId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusConvertToMbusDeviceId(std::uint64_t localId, std::uint32_t deviceClass, std::uint64_t* deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugAcquireControl(std::uint64_t control, std::int32_t first, std::int32_t second, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugAcquireControlWithState(std::uint32_t deviceClass, std::int32_t state, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugAcquireControlWithState2(std::uint32_t deviceClass, std::int32_t moduleId, std::int32_t state, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugAcquireControlWithStateFlag(std::uint32_t deviceClass, std::uint32_t moduleId, std::uint32_t state, std::uint32_t flags, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugAddProcess(std::int32_t processId, std::int32_t flags, const MbusProcessInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugCheckProcessResume(MbusProcessInfo* info, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugDisableBgmForShellUi(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugGetApplicationStartupInfo(std::uint32_t appId, const MbusProcessInfo* info, MbusStartupInfo* startupInfo) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugGetControlStatus(std::int32_t deviceClass, std::int32_t processId, MbusControlStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugGetDeviceInfo(std::uint64_t deviceClass, MbusDebugDeviceInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugGetInternalInfo(MbusInternalInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugGetPriorityInfo(std::uint32_t deviceClass, MbusPriorityInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugReenableBgmForShellUi(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugReleaseControl(std::uint64_t control, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugRemoveCameraAppModuleFocus(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugResumeApplication(std::int32_t appId, std::uint8_t firstFlag, std::uint8_t secondFlag, MbusProcessInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugSetApplicationFocusByAppId(std::uint32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugSetCameraAppModuleFocus(std::int32_t processId, std::int32_t moduleId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugSetControllerFocusByAppId(std::uint32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugSetOtherProcessExcludedAction(std::uint64_t control, std::uint32_t action, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugSetPriority(std::uint64_t control, std::uint32_t priority, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugStartApplication(std::int32_t appId, std::int32_t first, std::int32_t second, std::uint8_t firstFlag, std::uint8_t secondFlag, const MbusStartupParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugStartApplication2(std::int32_t appId, std::int32_t first, std::int32_t second, std::uint8_t firstFlag, std::uint8_t secondFlag, const MbusStartupParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugStartApplicationNull(std::int32_t appId, std::int32_t first, std::int32_t second, std::uint8_t firstFlag, std::uint8_t secondFlag) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugSuspendApplication(std::uint32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugTerminateApplication(std::uint32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDebugTerminateProcess(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDetachFromApplicationByPid(std::int32_t processId, std::int32_t deviceClass) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDisableBgmForShellUi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDisableChat(std::uint32_t deviceClass) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDisconnectDevice(std::uint64_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpAppInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpHandleStatusInfo(std::uint64_t control, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpHandleStatusInfoAll(std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpModuleStateInfo(std::uint64_t control) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpModuleStateInfoAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpPlayerInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpPortHandleInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpProcMediaInfo(std::uint64_t control) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpProcMediaInfoAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusDumpUserMediaInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEnableChat(std::uint32_t deviceClass) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEnterAudioOutAutoAttenuation(std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEventBusStatusChangeSubscribe(std::int32_t eventHandle, std::uint32_t first, std::uint32_t second, std::uint64_t firstValue, std::uint64_t secondValue) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEventBusStatusChangeUnsubscribe(std::uint64_t unusedSlot, std::int32_t subscription) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEventCallbackFuncsInit_(MbusEventCallbacks* callbacks, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEventCreate_(std::uint32_t flags, MbusEvent* buffer, std::uint32_t capacity, const MbusEventCallbacks* callbacks, void* userData) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEventDestroy(std::int32_t eventHandle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusEventReceive(std::int32_t eventHandle, MbusEvent* events, std::int32_t capacity, std::int32_t wait) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetControlStatus(std::uint64_t control, MbusControlStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetDeviceDescription(std::uint64_t deviceId, std::uint32_t selector, MbusDeviceDescription* description) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetDeviceInfoByBusId_(std::int32_t busId, MbusDeviceInfo* info, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetDeviceInfoByConditionForDeviceService(std::int32_t flags, MbusDeviceServiceInfo* infos, std::int32_t capacity, std::int32_t* first, std::int32_t* second, MbusDeviceCondition condition) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetDeviceInfoByCondition_(const MbusDeviceCondition* condition, std::int32_t flags, MbusDeviceInfo* infos, std::int32_t capacity, std::int32_t* first, std::int32_t* second, std::uint32_t infoSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetDeviceInfo_(std::uint64_t deviceId, MbusDeviceInfo* info, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetSimulatedBusUsableStatusByBusType(std::uint32_t busType, std::int32_t first, std::uint64_t deviceId, std::uint32_t second, std::uint32_t third) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetSimulatedHandleStatusInfo(std::int32_t deviceClass, std::int32_t first, std::int32_t second, MbusSimulatedHandleStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetSimulatedHandleStatusInfo2(std::uint32_t deviceClass, std::uint32_t first, std::uint32_t second, std::uint32_t third, MbusSimulatedHandleStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusGetSparkState(std::uint64_t deviceId, MbusSparkState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusIsBgmProhibited() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusIsUsingDevice(std::int32_t deviceClass, std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusLeaveAudioOutAutoAttenuation(std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusNotifyBtDisconnection(std::uint64_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusNotifyDriverEvent(std::int32_t driverId, std::int32_t eventId, std::uint64_t firstValue, std::int32_t flags, std::uint64_t secondValue) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusReenableBgmForShellUi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusRegisterDriver(std::int32_t driverId, const MbusDriverCallbacks* callbacks, void* userData) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusReleaseControl(std::uint64_t control) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusRemoveCameraAppModuleFocus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusRemoveHandle(std::int32_t deviceClass, std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusRemoveProxy(std::int32_t processId, std::uint32_t deviceClass, std::uint64_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusResolveByDeviceId(std::int32_t deviceClass, std::uint64_t deviceId, std::uint32_t first, std::uint32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusResolveByHandle(std::int32_t deviceClass, std::int32_t handle, std::int32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusResolveByPlayerId(std::uint32_t deviceClass, std::uint32_t playerId, std::uint32_t first, std::uint32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusResolveByUserId(std::uint32_t deviceClass, std::int32_t userId, std::uint32_t first, std::uint32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetAutoLoginMode(std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetCameraAppModuleFocus(std::int32_t moduleId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetDeviceFunctionState(std::uint64_t deviceId, MbusDeviceFunctionState state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetDuckingSetting(std::int32_t setting) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetProxy(std::int32_t processId, std::int32_t first, std::int32_t second, std::int32_t third, std::uint64_t deviceId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetRoutingMode(std::int32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetScratchData(std::uint64_t deviceId, std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetScratchDataUpdatedEventMask(std::uint32_t mask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetSocialScreenAudioMode(std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetTestFlag(std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetThreadParam(std::int32_t priority, std::uint64_t affinityMask, std::uint64_t reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetUsbPowerBlackList(std::int32_t first, std::uint8_t count, const MbusUsbPowerEntry* entries) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetUserMuteStatus(std::int32_t userId, std::int32_t status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusSetVolumeDown(std::uint32_t deviceClass, std::uint8_t volume) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusStartAudioOutAuxBroadcast() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusStartAudioOutSharePlay(std::int32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusStopAudioOutAuxBroadcast() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusStopAudioOutSharePlay() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusTerm() {
    return 0;
}

int APS5_VABI sceMbusUnregisterDriver(std::int32_t driverId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusUserLogin(std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMbusUserLogout(std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
