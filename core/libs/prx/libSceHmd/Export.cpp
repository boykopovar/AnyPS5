#include "Hmd.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <string>

struct HmdDistortionCommandParameter;
struct HmdWideNearDistortionCommandParameter;
struct Hmd2dVrCommandParameter;

struct HmdCommandBuffer {
    const std::uint32_t* commands;
    std::uint32_t dwordCount;
};

static_assert(sizeof(HmdCommandBuffer) == 16);
static_assert(offsetof(HmdCommandBuffer, dwordCount) == 8);

struct HmdReprojectionFrameParameter;
struct HmdReprojectionWideNearParameter;
struct HmdReprojection2dVrParameter;
struct HmdReprojectionPose;
struct HmdReprojectionOverlayParameter;
struct HmdReprojectionSubmissionParameter;
struct HmdReprojectionInitializeParameter;
struct HmdReprojectionMultilayerParameter;
struct HmdDeviceInformation;
struct HmdDistortionParameters;
struct HmdInertialSensorData;
struct HmdStatusReport;

struct HmdFieldOfView {
    float angles[4];
};

struct HmdAssemblyError {
    float offsets[4];
};

static_assert(sizeof(HmdFieldOfView) == 16);
static_assert(sizeof(HmdAssemblyError) == 16);

struct HmdVr2dData;
struct HmdPuPositionParameter;
struct HmdOpticalParameter;
struct HmdCrashDumpInfo;
struct HmdScreenOutputParameter;
struct HmdScreenFadeParameter;
struct HmdScreenStatus;
struct HmdCaptureInitializeParameter;
struct HmdCaptureInfo;
struct HmdLiveCaptureParameter;
struct HmdReprojectionDebugInfo;
struct HmdReprojectionMultilayerDebugInfo;
struct HmdReprojectionCallbackParameter;

namespace {

std::atomic<bool> initialized{false};

std::int32_t Initialize(const Hmd::InitializeParam* param) {
    if (initialized.load()) return Hmd::AlreadyInitialized;
    if (!param) return Hmd::ParameterNull;
    if (param->reserved0) return Hmd::UnsupportedFeature;
    return initialized.exchange(true) ? Hmd::AlreadyInitialized : 0;
}

std::uint32_t CompiledSdkVersion() {
    static const auto* processParameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, processParameters + 0x10, sizeof(version));
    return version;
}

std::int32_t CommandQueryHandle(std::int32_t handle, const char* query) {
    if (handle != 0) return Hmd::InvalidHandle;
    if (initialized.load()) NotImplemented_nid_no_patch((std::string(query) + " with handle 0 after sceHmdInitialize").c_str());
    if (CompiledSdkVersion() >= Hmd::StartupStateSdkVersion) NotImplemented_nid_no_patch((std::string(query) + " with handle 0 before sceHmdInitialize from SDK 4.50").c_str());
    return Hmd::NotInitialized;
}

template <typename T>
T ParameterWord(const Hmd2dVrCommandParameter* parameter, std::size_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(parameter) + offset, sizeof(value));
    return value;
}

std::int32_t Vr2dCommandArguments(const Hmd2dVrCommandParameter* parameter, const HmdCommandBuffer* commandBuffer) {
    if (!parameter || !commandBuffer) return Hmd::ParameterNull;
    for (const std::size_t offset : {0x00u, 0x08u, 0x20u}) {
        if (ParameterWord<std::uint64_t>(parameter, offset) == 0) return Hmd::ParameterNull;
    }
    if (ParameterWord<std::uint32_t>(parameter, 0x34) != 0 || ParameterWord<std::uint64_t>(parameter, 0x38) != 0) return Hmd::ParameterInvalid;
    return 0;
}

}

struct HmdEyeDistances {
    std::uint32_t left;
    std::uint32_t right;
};

struct HmdEyeOffset {
    float x;
    float y;
    float z;
};

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

static_assert(sizeof(HmdMirroringAspectDebug) == 12);
static_assert(offsetof(HmdMirroringAspectDebug, mode) == 10);
static_assert(sizeof(HmdMirroringAspect) == 8);
static_assert(offsetof(HmdMirroringAspect, aspect) == 2);

using HmdEventCallback = void (APS5_VABI *)(std::uint32_t events);

struct HmdDfuData {
    const void* data;
    std::uint32_t size;
    std::uint32_t mode;
};

static_assert(sizeof(HmdDfuData) == 16);
static_assert(offsetof(HmdDfuData, size) == 8);
static_assert(offsetof(HmdDfuData, mode) == 12);

struct HmdScreenData {
    const void* data;
    std::int32_t size;
};

static_assert(sizeof(HmdScreenData) == 16);
static_assert(offsetof(HmdScreenData, size) == 8);
static_assert(sizeof(HmdEyeDistances) == 8);
static_assert(sizeof(HmdEyeOffset) == 12);

struct HmdLedData {
    std::uint8_t values[9];
};

static_assert(sizeof(HmdLedData) == 9);

extern "C" {

std::int32_t APS5_VABI sceHmdInitialize(const Hmd::InitializeParam* param) {
    return Initialize(param);
}

std::int32_t APS5_VABI sceHmdInitialize315(const Hmd::InitializeParam* param) {
    return Initialize(param);
}

std::int32_t APS5_VABI sceHmdTerminate() {
    return initialized.exchange(false) ? 0 : Hmd::NotInitialized;
}

std::int32_t APS5_VABI sceHmdOpen(std::int32_t userId, std::int32_t type, std::int32_t index,
                                Hmd::OpenParam* param) {
    if (!initialized.load()) return Hmd::NotInitialized;
    if (type != 0 || index != 0 || param || userId == -1 || userId == 0xff)
        return Hmd::ParameterInvalid;
    return Hmd::DeviceDisconnected;
}

std::int32_t APS5_VABI sceHmdClose(std::int32_t handle) {
    (void)handle;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

std::int32_t APS5_VABI sceHmdGetDeviceInformation(Hmd::DeviceInformation* info) {
    if (!info) return Hmd::ParameterNull;
    if (!initialized.load()) return Hmd::NotInitialized;
    *info = {};
    info->status = Hmd::DeviceStatus::NotDetected;
    info->userId = -1;
    return 0;
}

std::int32_t APS5_VABI sceHmdGetDeviceInformationByHandle(std::int32_t handle,
                                                        Hmd::DeviceInformation* info) {
    (void)handle;
    (void)info;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

std::int32_t APS5_VABI sceHmdInternalGetDeviceStatus(Hmd::DeviceStatus* status) {
    if (!status) return Hmd::ParameterNull;
    *status = Hmd::DeviceStatus::NotDetected;
    return 0;
}

std::int32_t APS5_VABI sceHmdInternalMmapIsConnect() {
    return 0;
}

int APS5_VABI sceHmdInternalGetPUVersion(std::uint32_t* version) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalEventInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetVirtualDisplaySize(std::uint8_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSocialScreenSetOutput(std::uint32_t output, const HmdScreenOutputParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetVirtualDisplayDepth(std::uint8_t* depth) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetEyeStatus(std::int32_t handle, HmdEyeOffset* left, HmdEyeOffset* right) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionUnsetCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetCrashReportCommand(std::uint8_t command) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetDebugTextMode(std::uint8_t* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetWorkMemoryAlign(void) {
    return 0x400;
}

int APS5_VABI sceHmdInternalRegisterEventCallback(HmdEventCallback callback) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetWorkMemorySize(void) {
    return 0x20000;
}

int APS5_VABI sceHmdReprojectionStartWideNear(const HmdReprojectionWideNearParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetPUSerialNumber(char* serialNumber) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetDebugSocialScreenMode(std::uint8_t* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetM2LedBrightness(std::int32_t handle, std::uint32_t ledMask, const std::uint8_t* brightness) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetIPD(std::int32_t mode, HmdEyeDistances distances) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalMirroringModeSetAspect(const HmdMirroringAspect* aspect) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuGetStatus(std::uint64_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenSendVideo(const HmdScreenData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetRequiredPUPVersion(std::uint32_t* firstVersion, std::uint32_t* secondVersion) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionSetCallback(const HmdReprojectionCallbackParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuCheckAfterPvt(std::uint8_t* afterPvt) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalResetInertialSensor(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionSetUserEventStart(std::int64_t eventQueue, std::int32_t eventId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionSetOutputMinColor(float red, float green, float blue) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetVirtualDisplayDepth(std::uint8_t depth) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionWorkMemoryAlign(void) {
    return 0x400;
}

int APS5_VABI sceHmdReprojectionStartMultilayer(const HmdReprojectionMultilayerParameter* layers, std::uint32_t layerCount, const HmdReprojectionSubmissionParameter* submission, const HmdReprojectionPose* pose, std::uint64_t flipArg, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalPushVr2dData(const HmdVr2dData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionClearUserEventEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetIPD(HmdEyeDistances* distances) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetVRMode(std::int32_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCrashReportReadDataSize(std::uint32_t* dataSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetVr2dData(HmdVr2dData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenSendAudio(const HmdScreenData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetPuPositionParam(HmdPuPositionParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionInitializeCapture(const HmdCaptureInitializeParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGet2DEyeOffset(std::int32_t handle, HmdEyeOffset* leftOffset, HmdEyeOffset* rightOffset) {
    (void)handle;
    (void)leftOffset;
    (void)rightOffset;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

int APS5_VABI sceHmdInternalSetDebugSocialScreenMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetHmuPowerStatusForDebug(std::uint8_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionWorkMemorySize(void) {
    return 0x20000;
}

int APS5_VABI sceHmdReprojectionSetDisplayBuffers(std::int32_t videoOutHandle, std::int32_t firstBufferIndex, std::int32_t secondBufferIndex, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenSetFadeAndSwitch(const HmdScreenFadeParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCreateSharedMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalMirroringModeSetAspectDebug(const HmdMirroringAspectDebug* aspect) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetPuBuildNumber(std::uint32_t* buildNumber) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetCompoundEyeCorrectionCommand(void) {
    return static_cast<int>(0x81110009);
}

int APS5_VABI sceHmdInternalSetS3dPassMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStopCapture(HmdCaptureInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceHmdInternalIsCommonDlgMiniAppVr2d(void) {
    return false;
}

int APS5_VABI sceHmdInternalMmapGetSensorCalibrationData(void* buffer, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetCorrectionCommand(std::int32_t handle, const HmdDistortionCommandParameter* parameter, HmdCommandBuffer* commandBuffer) {
    (void)parameter;
    (void)commandBuffer;
    return CommandQueryHandle(handle, __func__);
}

int APS5_VABI sceHmdReprojectionQueryOnionBuffAlign(void) {
    return 0x100;
}

int APS5_VABI sceHmdInternalGetDebugMode(std::uint8_t* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceHmdInternalIsMiniAppVr2d(void) {
    return false;
}

bool APS5_VABI sceHmdInternalIsCommonDlgVr2d(void) {
    return false;
}

int APS5_VABI sceHmdReprojectionSetOutputMinColor(float red, float green, float blue) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetPortConnection(std::uint64_t connection) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionDebugGetLastInfo(HmdReprojectionDebugInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetM2LedOn(std::int32_t handle, std::uint32_t ledMask, std::uint32_t brightness) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetFieldOfView(std::int32_t handle, HmdFieldOfView* fieldOfView) {
    (void)handle;
    if (!fieldOfView) return Hmd::ParameterNull;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

int APS5_VABI sceHmdReprojectionAddDisplayBuffer(std::int32_t bufferIndex, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetIpdSettingEnableForSystemService(std::uint8_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetLedOn(std::int32_t handle, std::uint32_t ledMask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternal3dAudioSendData(std::int32_t port, const void* data, std::int32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionInitialize(const HmdReprojectionInitializeParameter* parameter, std::uint32_t mode, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetDeviceInformationByHandle(std::int32_t handle, HmdDeviceInformation* information) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStopLiveCapture(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCrashReportReadData(void* buffer, std::uint32_t* bytesRead, std::uint32_t bufferSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetDeviceInformation(HmdDeviceInformation* information) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStartCapture(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetDefaultLedData(std::int32_t handle, std::uint32_t* mask, HmdLedData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionQueryGarlicBuffAlign(void) {
    return 0x100;
}

int APS5_VABI sceHmdInternalSetHmuPowerControl(std::uint8_t control) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalBindDeviceWithUserId(std::int32_t handle, std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetHmuSerialNumber(std::uint8_t* serialNumber) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGetWideNearCorrectionCommand(std::int32_t handle, const HmdWideNearDistortionCommandParameter* parameter, HmdCommandBuffer* commandBuffer) {
    (void)parameter;
    (void)commandBuffer;
    return CommandQueryHandle(handle, __func__);
}

int APS5_VABI sceHmdInternalCheckS3dPassModeAvailable(bool* available) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenGetAudioStatus(HmdScreenStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSeparateModeGetAudioStatus(HmdScreenStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStartLiveCapture(const HmdLiveCaptureParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceHmdInternalIsGameVr2d(void) {
    return false;
}

int APS5_VABI sceHmdGetAssyError(HmdAssemblyError* data) {
    if (!data) return Hmd::ParameterNull;
    return initialized.load() ? Hmd::DeviceDisconnected : Hmd::NotInitialized;
}

int APS5_VABI sceHmdReprojectionFinalize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSocialScreenSetFadeAndSwitch(const HmdScreenFadeParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetVirtualDisplayHeight(std::uint8_t height) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionInitialize(void* workMemory) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuSetMode(std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetForcedCrash(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuCheckPartialUpdateAvailable(bool* available) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSeparateModeClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetDebugTextMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCrashReportClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetHmuPowerControlForDebug(std::uint8_t control) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetVirtualDisplayHeight(std::uint8_t* height) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStart(const HmdReprojectionFrameParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalResetLedForVsh(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetDebugMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCrashReportCancel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSeparateModeGetVideoStatus(HmdScreenStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCheckDeviceModelMk3(bool* isMk3) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdSetVr2dEnable(std::int32_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetSidetone(std::uint8_t level) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSeparateModeOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalMmapGetModeId(std::uint32_t* modeId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdDistortionGet2dVrCommand(std::int32_t handle, const Hmd2dVrCommandParameter* parameter, HmdCommandBuffer* commandBuffer) {
    if (const auto result = Vr2dCommandArguments(parameter, commandBuffer); result != 0) return result;
    return CommandQueryHandle(handle, __func__);
}

int APS5_VABI sceHmdInternalDfuSendSize(std::int32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetWideNearDistortionCorrectionCommand(std::int32_t handle, const HmdWideNearDistortionCommandParameter* parameter, HmdCommandBuffer* commandBuffer) {
    (void)parameter;
    (void)commandBuffer;
    return CommandQueryHandle(handle, __func__);
}

int APS5_VABI sceHmdReprojectionStartMultilayer2(const HmdReprojectionMultilayerParameter* layers, std::uint32_t layerCount, const HmdReprojectionSubmissionParameter* submission, std::uint64_t flipArg, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionCorrectionCommand(std::int32_t handle, const HmdDistortionCommandParameter* parameter, HmdCommandBuffer* commandBuffer) {
    (void)parameter;
    (void)commandBuffer;
    return CommandQueryHandle(handle, __func__);
}

int APS5_VABI sceHmdInternalSetDeviceConnection(std::uint64_t connection) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetVirtualDisplaySize(std::uint8_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionUnsetDisplayBuffers(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetHmuOpticalParam(HmdOpticalParameter* parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSeparateModeSendVideo(const HmdScreenData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalResetLedForVrTracker(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionQueryOnionBuffSize(void) {
    return 0x810;
}

int APS5_VABI sceHmdReprojectionStartWithOverlay(const HmdReprojectionFrameParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const HmdReprojectionOverlayParameter* overlay, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionSetUserEventEnd(std::int64_t eventQueue, std::int32_t eventId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalCrashReportOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetDemoMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStartWideNearWithOverlay(const HmdReprojectionWideNearParameter* parameter, const HmdReprojectionPose* pose, std::uint64_t flipArg, const HmdReprojectionOverlayParameter* overlay, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetDistortionParams(HmdDistortionParameters* parameters) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuSend(const HmdDfuData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionClearUserEventStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetUserType(std::uint8_t userType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetDemoMode(std::uint8_t* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalMapSharedMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetCrashDumpInfo(HmdCrashDumpInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetTv4kCapability(std::uint8_t* capability) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalDfuReset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStart2dVr(const HmdReprojection2dVrParameter* parameter, std::uint64_t flipArg, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalMmapGetCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetDefaultLedData(std::int32_t handle, std::uint32_t mask, const HmdLedData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdGetInertialSensorData(std::int32_t handle, HmdInertialSensorData* data, std::uint32_t count) {
    (void)handle;
    (void)data;
    (void)count;
    return initialized.load() ? Hmd::InvalidHandle : Hmd::NotInitialized;
}

int APS5_VABI sceHmdInternal3dAudioClose(std::int32_t port) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetBrightness(std::uint8_t brightness) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSocialScreenGetFadeState(std::int32_t* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSeparateModeSendAudio(const HmdScreenData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetStatusReport(std::int32_t handle, HmdStatusReport* report) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetBrightness(std::uint8_t* brightness) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionFinalizeCapture(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetPortStatus(std::uint64_t status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenGetVideoStatus(HmdScreenStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternal3dAudioOpen(std::int32_t port) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalAnotherScreenGetFadeState(std::int32_t* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalSetDebugGpo(std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetIpdSettingEnableForSystemService(std::uint8_t* enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionDebugGetLastInfoMultilayer(HmdReprojectionMultilayerDebugInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdInternalGetPuRevision(std::uint32_t* revision) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmdReprojectionQueryGarlicBuffSize(void) {
    return 0x100000;
}

int APS5_VABI sceHmdGet2dVrCommand(std::int32_t handle, const Hmd2dVrCommandParameter* parameter, HmdCommandBuffer* commandBuffer) {
    if (const auto result = Vr2dCommandArguments(parameter, commandBuffer); result != 0) return result;
    return CommandQueryHandle(handle, __func__);
}

}
