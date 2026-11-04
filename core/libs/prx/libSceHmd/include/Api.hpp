#pragma once

#include "SceTypes.hpp"
#include "prx/libSceHmd/Hmd.hpp"

struct HmdCaptureInfo;

struct HmdCaptureInitializeParameter;

struct HmdCrashDumpInfo;

struct HmdDeviceInformation;

struct HmdDfuData {
    const void* data;
    std::uint32_t size;
    std::uint32_t mode;
};

struct HmdDistortionParameters;

using HmdEventCallback = void (APS5_VABI *)(std::uint32_t events);

struct HmdEyeDistances {
    std::uint32_t left;
    std::uint32_t right;
};



struct HmdLedData {
    std::uint8_t values[9];
};

struct HmdLiveCaptureParameter;

struct HmdMirroringAspect {
    std::uint16_t reserved0;
    std::int16_t aspect;
    std::uint16_t reserved1;
    std::uint16_t reserved2;
};

struct HmdMirroringAspectDebug {
    std::uint16_t values[5];
    std::uint8_t mode;
};

struct HmdOpticalParameter;

struct HmdPuPositionParameter;

struct HmdReprojection2dVrParameter;

struct HmdReprojectionCallbackParameter;

struct HmdReprojectionDebugInfo;

struct HmdReprojectionFrameParameter;

struct HmdReprojectionInitializeParameter;

struct HmdReprojectionMultilayerDebugInfo;

struct HmdReprojectionMultilayerParameter;

struct HmdReprojectionOverlayParameter;

struct HmdReprojectionPose;

struct HmdReprojectionSubmissionParameter;

struct HmdReprojectionWideNearParameter;

struct HmdScreenData {
    const void* data;
    std::int32_t size;
};

struct HmdScreenFadeParameter;

struct HmdScreenOutputParameter;

struct HmdScreenStatus;

struct HmdStatusReport;

struct HmdVr2dData;

extern "C" {

int APS5_VABI sceHmdInternalGetPUVersion(std::uint32_t* version);
int APS5_VABI sceHmdInternalEventInitialize(void);
int APS5_VABI sceHmdInternalSetVirtualDisplaySize(std::uint8_t size);
int APS5_VABI sceHmdInternalSocialScreenSetOutput(std::uint32_t output, const HmdScreenOutputParameter* parameter);
int APS5_VABI sceHmdInternalGetVirtualDisplayDepth(std::uint8_t* depth);
int APS5_VABI sceHmdInternalGetEyeStatus(std::int32_t handle, HmdEyeOffset* left, HmdEyeOffset* right);
int APS5_VABI sceHmdInternalAnotherScreenOpen(void);
int APS5_VABI sceHmdReprojectionUnsetCallback(void);
int APS5_VABI sceHmdInternalSetCrashReportCommand(std::uint8_t command);
int APS5_VABI sceHmdInternalGetDebugTextMode(std::uint8_t* mode);
int APS5_VABI sceHmdInternalRegisterEventCallback(HmdEventCallback callback);
int APS5_VABI sceHmdReprojectionStartWideNear(const HmdReprojectionWideNearParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const void* reserved);
int APS5_VABI sceHmdInternalGetPUSerialNumber(char* serialNumber);
int APS5_VABI sceHmdInternalGetDebugSocialScreenMode(std::uint8_t* mode);
int APS5_VABI sceHmdInternalSetM2LedBrightness(std::int32_t handle, std::uint32_t ledMask, const std::uint8_t* brightness);
int APS5_VABI sceHmdInternalSetIPD(std::int32_t mode, HmdEyeDistances distances);
int APS5_VABI sceHmdInternalMirroringModeSetAspect(const HmdMirroringAspect* aspect);
int APS5_VABI sceHmdInternalDfuGetStatus(std::uint64_t* status);
int APS5_VABI sceHmdInternalAnotherScreenSendVideo(const HmdScreenData* data);
int APS5_VABI sceHmdInternalGetRequiredPUPVersion(std::uint32_t* firstVersion, std::uint32_t* secondVersion);
int APS5_VABI sceHmdReprojectionSetCallback(const HmdReprojectionCallbackParameter* parameter);
int APS5_VABI sceHmdInternalDfuCheckAfterPvt(std::uint8_t* afterPvt);
int APS5_VABI sceHmdInternalResetInertialSensor(void);
int APS5_VABI sceHmdReprojectionSetUserEventStart(std::int64_t eventQueue, std::int32_t eventId);
int APS5_VABI sceHmdDistortionSetOutputMinColor(float red, float green, float blue);
int APS5_VABI sceHmdInternalSetVirtualDisplayDepth(std::uint8_t depth);
int APS5_VABI sceHmdGetDistortionWorkMemoryAlign(void);
int APS5_VABI sceHmdReprojectionStartMultilayer(const HmdReprojectionMultilayerParameter* layers, std::uint32_t layerCount, const HmdReprojectionSubmissionParameter* submission, const HmdReprojectionPose* pose, std::uint64_t flipArg, const void* reserved);
int APS5_VABI sceHmdInternalPushVr2dData(const HmdVr2dData* data);
int APS5_VABI sceHmdReprojectionClearUserEventEnd(void);
int APS5_VABI sceHmdInternalGetIPD(HmdEyeDistances* distances);
int APS5_VABI sceHmdInternalSetVRMode(std::int32_t enabled);
int APS5_VABI sceHmdInternalCrashReportReadDataSize(std::uint32_t* dataSize);
int APS5_VABI sceHmdInternalGetVr2dData(HmdVr2dData* data);
int APS5_VABI sceHmdInternalAnotherScreenSendAudio(const HmdScreenData* data);
int APS5_VABI sceHmdInternalGetPuPositionParam(HmdPuPositionParameter* parameter);
int APS5_VABI sceHmdReprojectionInitializeCapture(const HmdCaptureInitializeParameter* parameter);
int APS5_VABI sceHmdInternalSetDebugSocialScreenMode(std::uint8_t mode);
int APS5_VABI sceHmdInternalGetHmuPowerStatusForDebug(std::uint8_t* status);
int APS5_VABI sceHmdGetDistortionWorkMemorySize(void);
int APS5_VABI sceHmdReprojectionSetDisplayBuffers(std::int32_t videoOutHandle, std::int32_t firstBufferIndex, std::int32_t secondBufferIndex, const void* reserved);
int APS5_VABI sceHmdInternalAnotherScreenSetFadeAndSwitch(const HmdScreenFadeParameter* parameter);
int APS5_VABI sceHmdInternalCreateSharedMemory(void);
int APS5_VABI sceHmdInternalMirroringModeSetAspectDebug(const HmdMirroringAspectDebug* aspect);
int APS5_VABI sceHmdInternalGetPuBuildNumber(std::uint32_t* buildNumber);
int APS5_VABI sceHmdDistortionGetCompoundEyeCorrectionCommand(void);
int APS5_VABI sceHmdInternalSetS3dPassMode(std::uint8_t mode);
int APS5_VABI sceHmdReprojectionStopCapture(HmdCaptureInfo* info);
int APS5_VABI sceHmdInternalMmapGetSensorCalibrationData(void* buffer, std::uint32_t size);
int APS5_VABI sceHmdInternalGetDebugMode(std::uint8_t* mode);
int APS5_VABI sceHmdReprojectionSetOutputMinColor(float red, float green, float blue);
int APS5_VABI sceHmdInternalSetPortConnection(std::uint64_t connection);
int APS5_VABI sceHmdReprojectionDebugGetLastInfo(HmdReprojectionDebugInfo* info);
int APS5_VABI sceHmdInternalSetM2LedOn(std::int32_t handle, std::uint32_t ledMask, std::uint32_t brightness);
int APS5_VABI sceHmdReprojectionAddDisplayBuffer(std::int32_t bufferIndex, const void* reserved);
int APS5_VABI sceHmdInternalSetIpdSettingEnableForSystemService(std::uint8_t enabled);
int APS5_VABI sceHmdInternalSetLedOn(std::int32_t handle, std::uint32_t ledMask);
int APS5_VABI sceHmdInternal3dAudioSendData(std::int32_t port, const void* data, std::int32_t size);
int APS5_VABI sceHmdReprojectionInitialize(const HmdReprojectionInitializeParameter* parameter, std::uint32_t mode, const void* reserved);
int APS5_VABI sceHmdInternalGetDeviceInformationByHandle(std::int32_t handle, HmdDeviceInformation* information);
int APS5_VABI sceHmdReprojectionStopLiveCapture(void);
int APS5_VABI sceHmdInternalAnotherScreenClose(void);
int APS5_VABI sceHmdInternalCrashReportReadData(void* buffer, std::uint32_t* bytesRead, std::uint32_t bufferSize);
int APS5_VABI sceHmdInternalGetDeviceInformation(HmdDeviceInformation* information);
int APS5_VABI sceHmdReprojectionStartCapture(void);
int APS5_VABI sceHmdInternalGetDefaultLedData(std::int32_t handle, std::uint32_t* mask, HmdLedData* data);
int APS5_VABI sceHmdInternalDfuClose(void);
int APS5_VABI sceHmdInternalSetHmuPowerControl(std::uint8_t control);
int APS5_VABI sceHmdInternalBindDeviceWithUserId(std::int32_t handle, std::int32_t userId);
int APS5_VABI sceHmdInternalGetHmuSerialNumber(std::uint8_t* serialNumber);
int APS5_VABI sceHmdInternalCheckS3dPassModeAvailable(bool* available);
int APS5_VABI sceHmdInternalAnotherScreenGetAudioStatus(HmdScreenStatus* status);
int APS5_VABI sceHmdInternalSeparateModeGetAudioStatus(HmdScreenStatus* status);
int APS5_VABI sceHmdReprojectionStartLiveCapture(const HmdLiveCaptureParameter* parameter);
int APS5_VABI sceHmdReprojectionFinalize(void);
int APS5_VABI sceHmdInternalSocialScreenSetFadeAndSwitch(const HmdScreenFadeParameter* parameter);
int APS5_VABI sceHmdInternalDfuStart(void);
int APS5_VABI sceHmdInternalSetVirtualDisplayHeight(std::uint8_t height);
int APS5_VABI sceHmdDistortionInitialize(void* workMemory);
int APS5_VABI sceHmdInternalDfuSetMode(std::uint32_t mode);
int APS5_VABI sceHmdInternalSetForcedCrash(std::uint8_t mode);
int APS5_VABI sceHmdInternalDfuCheckPartialUpdateAvailable(bool* available);
int APS5_VABI sceHmdInternalSeparateModeClose(void);
int APS5_VABI sceHmdInternalSetDebugTextMode(std::uint8_t mode);
int APS5_VABI sceHmdInternalCrashReportClose(void);
int APS5_VABI sceHmdInternalSetHmuPowerControlForDebug(std::uint8_t control);
int APS5_VABI sceHmdInternalGetVirtualDisplayHeight(std::uint8_t* height);
int APS5_VABI sceHmdReprojectionStart(const HmdReprojectionFrameParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const void* reserved);
int APS5_VABI sceHmdInternalResetLedForVsh(void);
int APS5_VABI sceHmdInternalDfuOpen(void);
int APS5_VABI sceHmdInternalSetDebugMode(std::uint8_t mode);
int APS5_VABI sceHmdInternalCrashReportCancel(void);
int APS5_VABI sceHmdInternalSeparateModeGetVideoStatus(HmdScreenStatus* status);
int APS5_VABI sceHmdInternalCheckDeviceModelMk3(bool* isMk3);
int APS5_VABI sceHmdSetVr2dEnable(std::int32_t enabled);
int APS5_VABI sceHmdInternalSetSidetone(std::uint8_t level);
int APS5_VABI sceHmdInternalSeparateModeOpen(void);
int APS5_VABI sceHmdInternalMmapGetModeId(std::uint32_t* modeId);
int APS5_VABI sceHmdInternalDfuSendSize(std::int32_t size);
int APS5_VABI sceHmdReprojectionStartMultilayer2(const HmdReprojectionMultilayerParameter* layers, std::uint32_t layerCount, const HmdReprojectionSubmissionParameter* submission, std::uint64_t flipArg, const void* reserved);
int APS5_VABI sceHmdInternalSetDeviceConnection(std::uint64_t connection);
int APS5_VABI sceHmdInternalGetVirtualDisplaySize(std::uint8_t* size);
int APS5_VABI sceHmdReprojectionUnsetDisplayBuffers(void);
int APS5_VABI sceHmdInternalGetHmuOpticalParam(HmdOpticalParameter* parameter);
int APS5_VABI sceHmdInternalSeparateModeSendVideo(const HmdScreenData* data);
int APS5_VABI sceHmdInternalResetLedForVrTracker(void);
int APS5_VABI sceHmdReprojectionStartWithOverlay(const HmdReprojectionFrameParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const HmdReprojectionOverlayParameter* overlay, const void* reserved);
int APS5_VABI sceHmdReprojectionSetUserEventEnd(std::int64_t eventQueue, std::int32_t eventId);
int APS5_VABI sceHmdInternalCrashReportOpen(void);
int APS5_VABI sceHmdInternalSetDemoMode(std::uint8_t mode);
int APS5_VABI sceHmdReprojectionStartWideNearWithOverlay(const HmdReprojectionWideNearParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const HmdReprojectionOverlayParameter* overlay, const void* reserved);
int APS5_VABI sceHmdGetDistortionParams(HmdDistortionParameters* parameters);
int APS5_VABI sceHmdInternalDfuSend(const HmdDfuData* data);
int APS5_VABI sceHmdReprojectionClearUserEventStart(void);
int APS5_VABI sceHmdInternalSetUserType(std::uint8_t userType);
int APS5_VABI sceHmdInternalGetDemoMode(std::uint8_t* mode);
int APS5_VABI sceHmdInternalMapSharedMemory(void);
int APS5_VABI sceHmdInternalGetCrashDumpInfo(HmdCrashDumpInfo* info);
int APS5_VABI sceHmdInternalGetTv4kCapability(std::uint8_t* capability);
int APS5_VABI sceHmdInternalDfuReset(void);
int APS5_VABI sceHmdReprojectionStart2dVr(const HmdReprojection2dVrParameter* parameter, std::uint64_t flipArg, const void* reserved);
int APS5_VABI sceHmdInternalMmapGetCount(void);
int APS5_VABI sceHmdInternalSetDefaultLedData(std::int32_t handle, std::uint32_t mask, const HmdLedData* data);
int APS5_VABI sceHmdInternal3dAudioClose(std::int32_t port);
int APS5_VABI sceHmdInternalSetBrightness(std::uint8_t brightness);
int APS5_VABI sceHmdInternalSocialScreenGetFadeState(std::int32_t* state);
int APS5_VABI sceHmdInternalSeparateModeSendAudio(const HmdScreenData* data);
int APS5_VABI sceHmdInternalGetStatusReport(std::int32_t handle, HmdStatusReport* report);
int APS5_VABI sceHmdInternalGetBrightness(std::uint8_t* brightness);
int APS5_VABI sceHmdReprojectionFinalizeCapture(void);
int APS5_VABI sceHmdInternalSetPortStatus(std::uint64_t status);
int APS5_VABI sceHmdReprojectionStop(void);
int APS5_VABI sceHmdInternalAnotherScreenGetVideoStatus(HmdScreenStatus* status);
int APS5_VABI sceHmdInternal3dAudioOpen(std::int32_t port);
int APS5_VABI sceHmdInternalAnotherScreenGetFadeState(std::int32_t* state);
int APS5_VABI sceHmdInternalSetDebugGpo(std::uint32_t value);
int APS5_VABI sceHmdInternalGetIpdSettingEnableForSystemService(std::uint8_t* enabled);
int APS5_VABI sceHmdReprojectionDebugGetLastInfoMultilayer(HmdReprojectionMultilayerDebugInfo* info);
int APS5_VABI sceHmdInternalGetPuRevision(std::uint32_t* revision);

}
