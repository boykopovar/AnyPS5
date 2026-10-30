#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

struct DeviceServiceThreadParam {
    std::uint64_t affinityMask;
    std::int32_t priority;
    std::uint32_t reserved;
};

struct DeviceServiceInitParam {
    std::uint32_t size;
    std::uint32_t reserved;
    const DeviceServiceThreadParam* threadParam;
};

struct DeviceServiceUsageFilter {
    std::uint16_t usagePage;
    std::uint16_t minimumUsage;
    std::uint16_t maximumUsage;
};

struct MbusDeviceFunctionState {
    std::byte data[180];
};

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

struct MbusDeviceServiceInfo;

struct MbusEventCallbacks {
    void (APS5_VABI *onOverflow)(std::int32_t eventHandle, void* userData);
    void (APS5_VABI *onEvent)(std::int32_t eventHandle, void* userData, const MbusEvent* event);
};

struct MbusProcessInfo;

struct MbusStartupInfo;

struct MbusStartupParam;

struct MbusControlStatus;

struct MbusDebugDeviceInfo;

struct MbusInternalInfo;

struct MbusPriorityInfo;

extern "C" {

std::int32_t APS5_VABI sceDeviceServiceInitialize(std::int32_t mode, const DeviceServiceInitParam* param);
std::int32_t APS5_VABI sceDeviceServiceTerminate();
std::int32_t APS5_VABI sceDeviceServiceGetEventState(std::int32_t clear);
std::int32_t APS5_VABI sceDeviceServiceQueryDeviceInfo_(std::int32_t deviceClass, const DeviceServiceUsageFilter* filter, std::int32_t arg2, void* infos, std::int32_t maxInfos, std::int32_t* count, std::int32_t* reserved, std::uint32_t infoSize);
std::int32_t APS5_VABI sceDeviceServiceGetGeneration(void);
int APS5_VABI sceMbusAcquireControl(std::uint64_t control, std::int32_t first, std::int32_t second);
int APS5_VABI sceMbusAcquireControlList();
int APS5_VABI sceMbusAcquireControlWithState(std::uint32_t deviceClass, std::int32_t state);
int APS5_VABI sceMbusAcquireControlWithState2(std::uint32_t deviceClass, std::int32_t moduleId, std::int32_t state);
int APS5_VABI sceMbusAcquireControlWithStateFlag(std::uint32_t deviceClass, std::uint32_t moduleId, std::uint32_t state, std::uint32_t flags);
int APS5_VABI sceMbusAddHandle(std::uint32_t deviceClass, std::int32_t handle, std::uint32_t flags);
int APS5_VABI sceMbusAddHandleByDeviceId(std::uint32_t deviceClass, std::int32_t handle, std::uint64_t deviceId, std::uint32_t first, std::uint32_t second, std::uint32_t third);
int APS5_VABI sceMbusAddHandleByUserId(std::uint32_t deviceClass, std::int32_t handle, std::int32_t userId, std::int32_t first, std::int32_t second, std::int32_t third);
int APS5_VABI sceMbusAddHandleByVidPid(std::uint32_t deviceClass, std::int32_t handle, std::int32_t first, std::uint16_t vendorId, std::uint16_t productId, std::int32_t second, std::int32_t third);
int APS5_VABI sceMbusAddHandleWithModuleId(std::uint32_t deviceClass, std::int32_t handle, std::uint32_t flags, std::uint32_t moduleId);
int APS5_VABI sceMbusAttachToApplicationByPid(std::int32_t processId, std::int32_t deviceClass, std::int32_t flags);
int APS5_VABI sceMbusBindDeviceWithUserId(std::uint64_t deviceId, std::int32_t userId);
int APS5_VABI sceMbusChangeAppModuleState(std::uint32_t deviceClass, std::int32_t moduleId, std::uint32_t state);
int APS5_VABI sceMbusCheckDriverCallback();
int APS5_VABI sceMbusConvertToLocalDeviceId(std::uint64_t deviceId, std::uint32_t* localId, std::uint32_t* deviceClass);
int APS5_VABI sceMbusConvertToLocalDeviceId2(std::uint64_t deviceId, std::uint32_t deviceClass, std::uint64_t* localId);
int APS5_VABI sceMbusConvertToMbusDeviceId(std::uint64_t localId, std::uint32_t deviceClass, std::uint64_t* deviceId);
int APS5_VABI sceMbusDebugAcquireControl(std::uint64_t control, std::int32_t first, std::int32_t second, std::int32_t processId);
int APS5_VABI sceMbusDebugAcquireControlWithState(std::uint32_t deviceClass, std::int32_t state, std::int32_t processId);
int APS5_VABI sceMbusDebugAcquireControlWithState2(std::uint32_t deviceClass, std::int32_t moduleId, std::int32_t state, std::int32_t processId);
int APS5_VABI sceMbusDebugAcquireControlWithStateFlag(std::uint32_t deviceClass, std::uint32_t moduleId, std::uint32_t state, std::uint32_t flags, std::int32_t processId);
int APS5_VABI sceMbusDebugAddProcess(std::int32_t processId, std::int32_t flags, const MbusProcessInfo* info);
int APS5_VABI sceMbusDebugCheckProcessResume(MbusProcessInfo* info, std::uint32_t flags);
int APS5_VABI sceMbusDebugDisableBgmForShellUi(std::int32_t processId);
int APS5_VABI sceMbusDebugGetApplicationStartupInfo(std::uint32_t appId, const MbusProcessInfo* info, MbusStartupInfo* startupInfo);
int APS5_VABI sceMbusDebugGetControlStatus(std::int32_t deviceClass, std::int32_t processId, MbusControlStatus* status);
int APS5_VABI sceMbusDebugGetDeviceInfo(std::uint64_t deviceClass, MbusDebugDeviceInfo* info);
int APS5_VABI sceMbusDebugGetInternalInfo(MbusInternalInfo* info);
int APS5_VABI sceMbusDebugGetPriorityInfo(std::uint32_t deviceClass, MbusPriorityInfo* info);
int APS5_VABI sceMbusDebugReenableBgmForShellUi(std::int32_t processId);
int APS5_VABI sceMbusDebugReleaseControl(std::uint64_t control, std::int32_t processId);
int APS5_VABI sceMbusDebugRemoveCameraAppModuleFocus(std::int32_t processId);
int APS5_VABI sceMbusDebugResumeApplication(std::int32_t appId, std::uint8_t firstFlag, std::uint8_t secondFlag, MbusProcessInfo* info);
int APS5_VABI sceMbusDebugSetApplicationFocusByAppId(std::uint32_t appId);
int APS5_VABI sceMbusDebugSetCameraAppModuleFocus(std::int32_t processId, std::int32_t moduleId);
int APS5_VABI sceMbusDebugSetControllerFocusByAppId(std::uint32_t appId);
int APS5_VABI sceMbusDebugSetOtherProcessExcludedAction(std::uint64_t control, std::uint32_t action, std::int32_t processId);
int APS5_VABI sceMbusDebugSetPriority(std::uint64_t control, std::uint32_t priority, std::int32_t processId);
int APS5_VABI sceMbusDebugStartApplication(std::int32_t appId, std::int32_t first, std::int32_t second, std::uint8_t firstFlag, std::uint8_t secondFlag, const MbusStartupParam* param);
int APS5_VABI sceMbusDebugStartApplication2(std::int32_t appId, std::int32_t first, std::int32_t second, std::uint8_t firstFlag, std::uint8_t secondFlag, const MbusStartupParam* param);
int APS5_VABI sceMbusDebugStartApplicationNull(std::int32_t appId, std::int32_t first, std::int32_t second, std::uint8_t firstFlag, std::uint8_t secondFlag);
int APS5_VABI sceMbusDebugSuspendApplication(std::uint32_t appId);
int APS5_VABI sceMbusDebugTerminateApplication(std::uint32_t appId);
int APS5_VABI sceMbusDebugTerminateProcess(std::int32_t processId);
int APS5_VABI sceMbusDetachFromApplicationByPid(std::int32_t processId, std::int32_t deviceClass);
int APS5_VABI sceMbusDisableBgmForShellUi();
int APS5_VABI sceMbusDisableChat(std::uint32_t deviceClass);
int APS5_VABI sceMbusDisconnectDevice(std::uint64_t deviceId);
int APS5_VABI sceMbusDumpAppInfo();
int APS5_VABI sceMbusDumpDeviceInfo();
int APS5_VABI sceMbusDumpHandleStatusInfo(std::uint64_t control, std::int32_t processId);
int APS5_VABI sceMbusDumpHandleStatusInfoAll(std::uint32_t flags);
int APS5_VABI sceMbusDumpModuleStateInfo(std::uint64_t control);
int APS5_VABI sceMbusDumpModuleStateInfoAll();
int APS5_VABI sceMbusDumpPlayerInfo();
int APS5_VABI sceMbusDumpPortHandleInfo();
int APS5_VABI sceMbusDumpProcMediaInfo(std::uint64_t control);
int APS5_VABI sceMbusDumpProcMediaInfoAll();
int APS5_VABI sceMbusDumpUserMediaInfo();
int APS5_VABI sceMbusEnableChat(std::uint32_t deviceClass);
int APS5_VABI sceMbusEnterAudioOutAutoAttenuation(std::int32_t first, std::int32_t second);
int APS5_VABI sceMbusEventBusStatusChangeSubscribe(std::int32_t eventHandle, std::uint32_t first, std::uint32_t second, std::uint64_t firstValue, std::uint64_t secondValue);
int APS5_VABI sceMbusEventBusStatusChangeUnsubscribe(std::uint64_t unusedSlot, std::int32_t subscription);
void APS5_VABI sceMbusEventCallbackFuncsInit_(MbusEventCallbacks* callbacks, std::uint32_t size);
int APS5_VABI sceMbusEventCreate_(std::uint32_t flags, MbusEvent* buffer, std::uint32_t capacity, const MbusEventCallbacks* callbacks, void* userData);
int APS5_VABI sceMbusEventDestroy(std::int32_t eventHandle);
int APS5_VABI sceMbusEventReceive(std::int32_t eventHandle, MbusEvent* events, std::int32_t capacity, std::int32_t wait);
int APS5_VABI sceMbusGetControlStatus(std::uint64_t control, MbusControlStatus* status);
int APS5_VABI sceMbusGetDeviceDescription(std::uint64_t deviceId, std::uint32_t selector, MbusDeviceDescription* description);
int APS5_VABI sceMbusGetDeviceInfoByBusId_(std::int32_t busId, MbusDeviceInfo* info, std::uint32_t size);
int APS5_VABI sceMbusGetDeviceInfoByConditionForDeviceService(std::int32_t flags, MbusDeviceServiceInfo* infos, std::int32_t capacity, std::int32_t* first, std::int32_t* second, MbusDeviceCondition condition);
int APS5_VABI sceMbusGetDeviceInfoByCondition_(const MbusDeviceCondition* condition, std::int32_t flags, MbusDeviceInfo* infos, std::int32_t capacity, std::int32_t* first, std::int32_t* second, std::uint32_t infoSize);
int APS5_VABI sceMbusGetDeviceInfo_(std::uint64_t deviceId, MbusDeviceInfo* info, std::uint32_t size);
int APS5_VABI sceMbusGetSimulatedBusUsableStatusByBusType(std::uint32_t busType, std::int32_t first, std::uint64_t deviceId, std::uint32_t second, std::uint32_t third);
int APS5_VABI sceMbusGetSimulatedHandleStatusInfo(std::int32_t deviceClass, std::int32_t first, std::int32_t second, MbusSimulatedHandleStatus* status);
int APS5_VABI sceMbusGetSimulatedHandleStatusInfo2(std::uint32_t deviceClass, std::uint32_t first, std::uint32_t second, std::uint32_t third, MbusSimulatedHandleStatus* status);
int APS5_VABI sceMbusGetSparkState(std::uint64_t deviceId, MbusSparkState* state);
int APS5_VABI sceMbusInit();
int APS5_VABI sceMbusIsBgmProhibited();
int APS5_VABI sceMbusIsUsingDevice(std::int32_t deviceClass, std::int32_t handle);
int APS5_VABI sceMbusLeaveAudioOutAutoAttenuation(std::int32_t first, std::int32_t second);
int APS5_VABI sceMbusNotifyBtDisconnection(std::uint64_t deviceId);
int APS5_VABI sceMbusNotifyDriverEvent(std::int32_t driverId, std::int32_t eventId, std::uint64_t firstValue, std::int32_t flags, std::uint64_t secondValue);
int APS5_VABI sceMbusReenableBgmForShellUi();
int APS5_VABI sceMbusRegisterDriver(std::int32_t driverId, const MbusDriverCallbacks* callbacks, void* userData);
int APS5_VABI sceMbusReleaseControl(std::uint64_t control);
int APS5_VABI sceMbusRemoveCameraAppModuleFocus();
int APS5_VABI sceMbusRemoveHandle(std::int32_t deviceClass, std::int32_t handle);
int APS5_VABI sceMbusRemoveProxy(std::int32_t processId, std::uint32_t deviceClass, std::uint64_t deviceId);
int APS5_VABI sceMbusResolveByDeviceId(std::int32_t deviceClass, std::uint64_t deviceId, std::uint32_t first, std::uint32_t second);
int APS5_VABI sceMbusResolveByHandle(std::int32_t deviceClass, std::int32_t handle, std::int32_t flags);
int APS5_VABI sceMbusResolveByPlayerId(std::uint32_t deviceClass, std::uint32_t playerId, std::uint32_t first, std::uint32_t second);
int APS5_VABI sceMbusResolveByUserId(std::uint32_t deviceClass, std::int32_t userId, std::uint32_t first, std::uint32_t second);
int APS5_VABI sceMbusSetAutoLoginMode(std::uint32_t mode);
int APS5_VABI sceMbusSetCameraAppModuleFocus(std::int32_t moduleId);
int APS5_VABI sceMbusSetDeviceFunctionState(std::uint64_t deviceId, MbusDeviceFunctionState state);
int APS5_VABI sceMbusSetDuckingSetting(std::int32_t setting);
int APS5_VABI sceMbusSetProxy(std::int32_t processId, std::int32_t first, std::int32_t second, std::int32_t third, std::uint64_t deviceId);
int APS5_VABI sceMbusSetRoutingMode(std::int32_t mode);
int APS5_VABI sceMbusSetScratchData(std::uint64_t deviceId, std::int32_t first, std::int32_t second);
int APS5_VABI sceMbusSetScratchDataUpdatedEventMask(std::uint32_t mask);
int APS5_VABI sceMbusSetSocialScreenAudioMode(std::int32_t first, std::int32_t second);
int APS5_VABI sceMbusSetTestFlag(std::uint32_t flags);
int APS5_VABI sceMbusSetThreadParam(std::int32_t priority, std::uint64_t affinityMask, std::uint64_t reserved);
int APS5_VABI sceMbusSetUsbPowerBlackList(std::int32_t first, std::uint8_t count, const MbusUsbPowerEntry* entries);
int APS5_VABI sceMbusSetUserMuteStatus(std::int32_t userId, std::int32_t status);
int APS5_VABI sceMbusSetVolumeDown(std::uint32_t deviceClass, std::uint8_t volume);
int APS5_VABI sceMbusStartAudioOutAuxBroadcast();
int APS5_VABI sceMbusStartAudioOutSharePlay(std::int32_t mode);
int APS5_VABI sceMbusStopAudioOutAuxBroadcast();
int APS5_VABI sceMbusStopAudioOutSharePlay();
int APS5_VABI sceMbusTerm();
int APS5_VABI sceMbusUnregisterDriver(std::int32_t driverId);
int APS5_VABI sceMbusUserLogin(std::int32_t userId);
int APS5_VABI sceMbusUserLogout(std::int32_t userId);

}
