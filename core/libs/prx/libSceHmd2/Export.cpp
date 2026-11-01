#include "prx/libSceHmd2/include/LegacyExports.hpp"
#include "prx/libSceHmd2/include/Api.hpp"
#include "prx/libSceHmd2/Hmd2.hpp"
#include "Hmd2.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceHmd/Hmd.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "SceTypes.hpp"

#include <cstddef>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace Hmd2 {

static_assert(sizeof(ReprojectionOutput) == 32);
static_assert(sizeof(ReprojectionInitializeParameter) == 72);
static_assert(offsetof(ReprojectionInitializeParameter, threadPriority) == 16);
static_assert(offsetof(ReprojectionInitializeParameter, threadAffinity) == 20);
static_assert(offsetof(ReprojectionInitializeParameter, renderingMode) == 32);
static_assert(offsetof(ReprojectionInitializeParameter, auxiliaryMemory) == 40);
static_assert(offsetof(ReprojectionInitializeParameter, timingInterval) == 48);

static_assert(sizeof(ReprojectionOption) == 128);
static_assert(sizeof(ReprojectionParameter) == 24);
static_assert(sizeof(ReprojectionBufferedParameter) == 24);
static_assert(offsetof(ReprojectionParameter, option) == 16);
static_assert(offsetof(ReprojectionBufferedParameter, option) == 16);

static_assert(sizeof(MirroringParameter) == 16);
static_assert(sizeof(MirroringOption) == 32);

static_assert(sizeof(GazeResultParameter) == 16);
static_assert(sizeof(FoveatedRenderingResult) == 24);
static_assert(offsetof(FoveatedRenderingResult, direction) == 8);

static_assert(sizeof(GazeObjectSelectionTransform) == 64);
static_assert(sizeof(GazeObjectSelectionObject) == 96);
static_assert(offsetof(GazeObjectSelectionObject, transform) == 16);
static_assert(offsetof(GazeObjectSelectionObject, boundingBoxes) == 80);
static_assert(offsetof(GazeObjectSelectionObject, boundingBoxCount) == 88);
static_assert(sizeof(GazeObjectSelectionParameter) == 120);
static_assert(offsetof(GazeObjectSelectionParameter, transform) == 24);
static_assert(offsetof(GazeObjectSelectionParameter, selectionMode) == 88);
static_assert(offsetof(GazeObjectSelectionParameter, timestamp) == 96);
static_assert(offsetof(GazeObjectSelectionParameter, objects) == 104);
static_assert(offsetof(GazeObjectSelectionParameter, objectCount) == 112);
static_assert(sizeof(GazeObjectSelectionResult) == 56);
static_assert(offsetof(GazeObjectSelectionResult, objectId) == 8);
static_assert(offsetof(GazeObjectSelectionResult, gazeTimestamp) == 16);
static_assert(offsetof(GazeObjectSelectionResult, origin) == 28);
static_assert(offsetof(GazeObjectSelectionResult, direction) == 40);
static_assert(offsetof(GazeObjectSelectionResult, distance) == 52);

static_assert(sizeof(CalibrationPoint) == 12);
static_assert(sizeof(GazeCalibrationData) == 16);
static_assert(offsetof(GazeCalibrationData, size) == 8);
static_assert(sizeof(GazeUserCalibrationData) == 16);
static_assert(offsetof(GazeUserCalibrationData, size) == 8);
static_assert(sizeof(GazeUserCalibrationStatus) == 20);
static_assert(offsetof(GazeUserCalibrationStatus, point) == 8);
static_assert(sizeof(GazeObjectSelectionInitializeParameter) == 24);
static_assert(offsetof(GazeObjectSelectionInitializeParameter, memory) == 8);
static_assert(offsetof(GazeObjectSelectionInitializeParameter, memorySize) == 16);
static_assert(sizeof(GazeObjectSelectionMemoryParameter) == 8);
static_assert(sizeof(GazeObjectSelectionMemoryInfo) == 16);
static_assert(offsetof(GazeObjectSelectionMemoryInfo, alignment) == 8);

}

namespace {

constexpr std::int32_t DeviceHandle = 0x0e010000;
constexpr Hmd2::FieldOfView DefaultFieldOfView{1.84177089f, 0.947190642f, 1.32898104f, 1.32898104f};

std::mutex stateMutex;
bool initialized = false;
std::int32_t openHandle = 0;

std::int32_t BeforeInitialize(std::int32_t answer, const char* name) {
    std::lock_guard lock(stateMutex);
    if (initialized) NotImplemented_nid_no_patch(name);
    return answer;
}

std::uint32_t CompiledSdkVersion() {
    static const auto* processParameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, processParameters + 0x10, sizeof(version));
    return version;
}

std::int32_t GetDeviceInformation(Hmd2::DeviceInformation* info) {
    if (!info) return Hmd2::ParameterNull;
    *info = {};
    if (!initialized) return Hmd2::NotInitialized;
    info->status = Hmd2::DeviceStatus::NotDetected;
    if (CompiledSdkVersion() < Hmd2::UserIdSdkVersion) info->userId = -1;
    info->panelWidth = 4000;
    info->panelHeight = 2040;
    info->latency90Hz = 5555;
    info->latency120Hz = 4160;
    info->virtualImageDistance = 2.0f;
    return 0;
}

}

extern "C" {

int APS5_VABI sceHmd2ReprojectionGetStatus(Hmd2::ReprojectionStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionQuerySeeThroughBufferSizeAlign() {
    return {0x1106000, 0x100};
}

std::int32_t APS5_VABI sceHmd2Initialize(const void* param) {
    (void)param;
    return Hmd::UnsupportedFeature;
}

std::int32_t APS5_VABI sceHmd2GetFieldOfView(std::int32_t handle, Hmd2::FieldOfView* fieldOfView) {
    std::lock_guard lock(stateMutex);
    if (!fieldOfView) return Hmd2::ParameterNull;
    if (handle == 0 || handle != openHandle) return Hmd2::InvalidHandle;
    if (!initialized) return Hmd2::NotInitialized;
    *fieldOfView = DefaultFieldOfView;
    return 0;
}

std::int32_t APS5_VABI sceHmd2GetDeviceInformationByHandle(std::int32_t handle, Hmd2::DeviceInformation* info) {
    std::lock_guard lock(stateMutex);
    if (handle != openHandle) return Hmd2::InvalidHandle;
    return GetDeviceInformation(info);
}

std::int32_t APS5_VABI sceHmd2Terminate() {
    std::lock_guard lock(stateMutex);
    if (!initialized) return Hmd2::NotInitialized;
    initialized = false;
    return 0;
}

std::int32_t APS5_VABI sceHmd2Close(std::int32_t handle) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return Hmd2::NotInitialized;
    if (handle != openHandle) return Hmd2::InvalidHandle;
    openHandle = 0;
    return 0;
}

static std::int32_t Hmd2GazeHandle(const Hmd2::GazeResultParameter* param, const void* result, std::uint32_t resultSize, const char* name) {
    if (!param || !result) return Hmd2::ParameterNull;
    std::uint32_t size;
    std::memcpy(&size, result, sizeof(size));
    if (param->size != sizeof(Hmd2::GazeResultParameter) || size != resultSize) return Hmd2::ParameterInvalid;
    std::lock_guard lock(stateMutex);
    if (openHandle == 0 || param->handle != openHandle) return Hmd2::InvalidHandle;
    NotImplemented_nid_no_patch(name);
    return 0;
}

static void Hmd2StoreInvalidGaze(std::byte* result, std::size_t offset) {
    const std::int32_t invalidIndex = -1;
    const float invalidPair[2] = {-0.0f, 0.0f};
    const float invalidScalar = -1.0f;
    std::memcpy(result + 4, &invalidIndex, sizeof(invalidIndex));
    std::memcpy(result + offset, invalidPair, sizeof(invalidPair));
    std::memcpy(result + offset + sizeof(invalidPair), &invalidScalar, sizeof(invalidScalar));
}

int APS5_VABI sceHmd2GazeGetResult(const Hmd2::GazeResultParameter* param, Hmd2::GazeResult* result) {
    const std::int32_t status = Hmd2GazeHandle(param, result, 0x38, __func__);
    if (status != Hmd2::InvalidHandle) return status;
    auto* bytes = reinterpret_cast<std::byte*>(result);
    std::memset(bytes + 4, 0, 0x38 - 4);
    Hmd2StoreInvalidGaze(bytes, 0x14);
    return status;
}

int APS5_VABI sceHmd2GazeGetResultForFoveatedRendering(const Hmd2::GazeResultParameter* param, Hmd2::FoveatedRenderingResult* result) {
    const std::int32_t status = Hmd2GazeHandle(param, result, 0x18, __func__);
    if (status != Hmd2::InvalidHandle) return status;
    Hmd2StoreInvalidGaze(reinterpret_cast<std::byte*>(result), 8);
    return status;
}

std::int32_t APS5_VABI sceHmd2GetDeviceInformation(Hmd2::DeviceInformation* info) {
    std::lock_guard lock(stateMutex);
    return GetDeviceInformation(info);
}

std::int32_t APS5_VABI sceHmd2GetFieldOfViewWithoutHandle(Hmd2::FieldOfView* fieldOfView) {
    std::lock_guard lock(stateMutex);
    if (!fieldOfView) return Hmd2::ParameterNull;
    if (!initialized) return Hmd2::NotInitialized;
    *fieldOfView = DefaultFieldOfView;
    return 0;
}

std::int32_t APS5_VABI sceHmd2Open(std::int32_t userId, std::int32_t type, std::int32_t index, const void* param) {
    std::lock_guard lock(stateMutex);
    if (!initialized) return Hmd2::NotInitialized;
    if (type != 0 || index != 0 || param) return Hmd2::ParameterInvalid;
    if (userId != USER_SERVICE_INITIAL_USER_ID) {
        if (CompiledSdkVersion() >= Hmd2::UserIdSdkVersion) return Hmd2::ParameterInvalid;
        throw std::runtime_error("sceHmd2Open: user is not the initial user");
    }
    if (openHandle != 0) return Hmd2::AlreadyOpened;
    openHandle = DeviceHandle;
    return DeviceHandle;
}

int APS5_VABI sceHmd2ReprojectionBeginFrame(std::uint64_t frameId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionDisableMirroring() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionDisableVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionEnableMirroring(const Hmd2::MirroringParameter* parameter) {
    return Hmd2::ReprojectionNotInitialized;
}

int APS5_VABI sceHmd2ReprojectionEnableVrMode(std::uint64_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionGetMirroringDisplayBufferSizeAlign() {
    return {0x5FA0000, 0x10000};
}

Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionGetMirroringWorkMemorySizeAlign() {
    return {0x4000, 0x100};
}

int APS5_VABI sceHmd2ReprojectionGetPredictedDisplayTime(std::uint64_t frameId, std::uint64_t* time) {
    return Hmd2::ReprojectionNotInitialized;
}

int APS5_VABI sceHmd2ReprojectionInitialize(const Hmd2::ReprojectionInitializeParameter* parameter, const void* reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionQueryBufferSizeAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

Hmd2::SizeAlign APS5_VABI sceHmd2ReprojectionQueryDisplayBufferSizeAlign() {
    return {0x6000000, 0x10000};
}

int APS5_VABI sceHmd2ReprojectionSetAllowPositionalReprojection(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetMirroringOption(const Hmd2::MirroringOption* option) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetParam(std::uint32_t bufferSetId, std::uint64_t frameId, const Hmd2::ReprojectionParameter* parameter, std::uint64_t reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetParamWithBuffer(std::uint32_t bufferSetId, std::uint64_t frameId, const Hmd2::ReprojectionBufferedParameter* parameter, std::uint64_t reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetRenderConfig(std::uint64_t flags, std::uint64_t reserved, std::uint64_t reserved2) {
    return Hmd2::ReprojectionNotInitialized;
}

int APS5_VABI sceHmd2ReprojectionSetTiming(std::int32_t timing) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceHmd2SetVibration(std::int32_t handle, std::uint32_t level) {
    (void)level;
    std::lock_guard lock(stateMutex);
    if (handle != openHandle) return Hmd2::InvalidHandle;
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSeeThroughDraw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSeeThroughIC() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionVideoOutEq() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionWaitShader() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2SeeThroughCallbackGetPoseDataFunc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionClear() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSet() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetEventGpuEnd(KernelEqueue queue, std::int32_t eventId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetEventGpuStart(KernelEqueue queue, std::int32_t eventId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2RpeorjectionSetParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI Hmd2BeforeInitializeZero() {
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("vDgpdS2N9Rw", Hmd2BeforeInitializeZero);
APS5_EXPORT("Ri35gKTZpvE", Hmd2BeforeInitializeZero);
APS5_EXPORT("9oyW8puiKeI", Hmd2BeforeInitializeZero);
APS5_EXPORT("1HZNE6jhBnQ", Hmd2BeforeInitializeZero);
APS5_EXPORT("cdwAKBZLXjs", Hmd2BeforeInitializeZero);
APS5_EXPORT("Yub1MF58jfY", Hmd2BeforeInitializeZero);
APS5_EXPORT("JYXAuQFnPyw", Hmd2BeforeInitializeZero);
APS5_EXPORT("VuPKqCu3wzw", Hmd2BeforeInitializeZero);
APS5_EXPORT("GSThagVyW5U", Hmd2BeforeInitializeZero);
APS5_EXPORT("J4qE5N937MY", Hmd2BeforeInitializeZero);
APS5_EXPORT("9KnNj-dyASg", Hmd2BeforeInitializeZero);
APS5_EXPORT("Qta2bkstbFk", Hmd2BeforeInitializeZero);
APS5_EXPORT("5ZcObnDRBXE", Hmd2BeforeInitializeZero);
APS5_EXPORT("AvPNn0qx4WQ", Hmd2BeforeInitializeZero);
int APS5_VABI sceHmd2SetIpdOffset(std::uint8_t enabled, float offset) {
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("-kZT7X7OMg0", Hmd2BeforeInitializeZero);
APS5_EXPORT("1ewLtng-SVk", Hmd2BeforeInitializeZero);
APS5_EXPORT("OXQ1EDf02XU", Hmd2BeforeInitializeZero);
APS5_EXPORT("IAxfMTvFm0M", Hmd2BeforeInitializeZero);
APS5_EXPORT("qTJtAtaLeno", Hmd2BeforeInitializeZero);
APS5_EXPORT("j7Q16UX6l34", Hmd2BeforeInitializeZero);
APS5_EXPORT("pmmNM06rLqU", Hmd2BeforeInitializeZero);
APS5_EXPORT("zdQfyDK9Ods", Hmd2BeforeInitializeZero);
APS5_EXPORT("aPRwp9GnljM", Hmd2BeforeInitializeZero);
APS5_EXPORT("MjY1gBSaed0", Hmd2BeforeInitializeZero);
APS5_EXPORT("evAMjJYHqW4", Hmd2BeforeInitializeZero);
APS5_EXPORT("r93FLEZ3BBk", Hmd2BeforeInitializeZero);
APS5_EXPORT("IdYURdEdNFo", Hmd2BeforeInitializeZero);
int APS5_VABI sceHmd2GazeActivateGazeTracking(std::int32_t mode) {
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("NmKIfVxr2s0", Hmd2BeforeInitializeZero);
APS5_EXPORT("KeDLkkp62Gg", Hmd2BeforeInitializeZero);
APS5_EXPORT("+aadu6-ra+A", Hmd2BeforeInitializeZero);
APS5_EXPORT("FgnTiwwGSzk", Hmd2BeforeInitializeZero);
APS5_EXPORT("PWAUwOTHbbo", Hmd2BeforeInitializeZero);
APS5_EXPORT("WzaEApHnsP8", Hmd2BeforeInitializeZero);
APS5_EXPORT("dXzQI8QVY7Y", Hmd2BeforeInitializeZero);
APS5_EXPORT("cCvFKGqRh1k", Hmd2BeforeInitializeZero);
APS5_EXPORT("uj4-2VEAFPU", Hmd2BeforeInitializeZero);
APS5_EXPORT("sIaGStI-iLM", Hmd2BeforeInitializeZero);
APS5_EXPORT("JzwpZOemDO8", Hmd2BeforeInitializeZero);
APS5_EXPORT("IsC4R4bZFVU", Hmd2BeforeInitializeZero);

std::int32_t APS5_VABI Hmd2BeforeInitializeNotInitialized() {
    return BeforeInitialize(Hmd2::NotInitialized, __func__);
}
APS5_EXPORT("sXaPEPY-fLA", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("3F5inHhKzXg", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("sJAiB-6p55U", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("E8hDydLjsSs", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("xEQkSb2WJ7Y", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("aaP4dnR4q-0", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("lEU0TeUoGwI", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("cFVyjHBpUUU", Hmd2BeforeInitializeNotInitialized);
APS5_EXPORT("zROtb46jTj4", Hmd2BeforeInitializeNotInitialized);

std::int32_t APS5_VABI Hmd2BeforeInitializeError0004() {
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
APS5_EXPORT("kAMsjYHBbNQ", Hmd2BeforeInitializeError0004);
APS5_EXPORT("5m73zmLpsQ0", Hmd2BeforeInitializeError0004);
APS5_EXPORT("EPeH4xuQ2XQ", Hmd2BeforeInitializeError0004);
APS5_EXPORT("TJ8lyRsjFY4", Hmd2BeforeInitializeError0004);
APS5_EXPORT("-XQVcLnyX68", Hmd2BeforeInitializeError0004);
APS5_EXPORT("giQwQoHhYYA", Hmd2BeforeInitializeError0004);
APS5_EXPORT("zNc9IkgawMI", Hmd2BeforeInitializeError0004);
int APS5_VABI sceHmd2GazeStartUserCalibration() {
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
int APS5_VABI sceHmd2GazeStopUserCalibration() {
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
APS5_EXPORT("CKlkvNngofQ", Hmd2BeforeInitializeError0004);
int APS5_VABI sceHmd2GazeSetUserCalibrationPoint(Hmd2::CalibrationPoint point) {
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
APS5_EXPORT("qlzgyG3s0RY", Hmd2BeforeInitializeError0004);
APS5_EXPORT("kTIDrDLzMv0", Hmd2BeforeInitializeError0004);
APS5_EXPORT("9Gee0ef8a+U", Hmd2BeforeInitializeError0004);
int APS5_VABI sceHmd2GazeSetEnabledEye(std::int32_t eye) {
    return BeforeInitialize(Hmd2::Error0004, __func__);
}

std::int32_t APS5_VABI Hmd2BeforeInitializeParameterNull() {
    return BeforeInitialize(Hmd2::ParameterNull, __func__);
}
APS5_EXPORT("MNtiJTDOPAU", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("RRy9WmqZfgU", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("uobR4m9lgwc", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("-XPk5KWBYdo", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("J44idh9izZM", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("Ps0wAvYaa1E", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("kLnMR8oQ1MY", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("kMz4rbBxvjo", Hmd2BeforeInitializeParameterNull);
int APS5_VABI sceHmd2GazeGetCalibrationData(std::int32_t type, Hmd2::GazeCalibrationData* data) {
    return BeforeInitialize(Hmd2::ParameterNull, __func__);
}
int APS5_VABI sceHmd2GazeSetUserCalibrationData(const Hmd2::GazeUserCalibrationData* data) {
    return BeforeInitialize(Hmd2::ParameterNull, __func__);
}
APS5_EXPORT("TJpyGGP9YEY", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("Dih+Odk+RC0", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("oaqdSJw+HVY", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("m35gtHuanXU", Hmd2BeforeInitializeParameterNull);
APS5_EXPORT("CTM7QPLJF9Q", Hmd2BeforeInitializeParameterNull);
int APS5_VABI sceHmd2GazeObjectSelectionInitialize(const Hmd2::GazeObjectSelectionInitializeParameter* parameter) {
    return BeforeInitialize(Hmd2::ParameterNull, __func__);
}

std::int32_t APS5_VABI Hmd2BeforeInitializeParameterInvalid() {
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}
APS5_EXPORT("bBh5bCp9coo", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("G3PNDE2VjfY", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("nScCT0OPU0o", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("6Q0Ng7OTN9o", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("-fJKh4SnFR0", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("jUA2s-l0npY", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("+9XhyAIJVj4", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("eBGK-o6E4RQ", Hmd2BeforeInitializeParameterInvalid);
APS5_EXPORT("MTuFeL9184E", Hmd2BeforeInitializeParameterInvalid);

std::int32_t APS5_VABI Hmd2BeforeInitializeReprojectionNotInitialized() {
    return BeforeInitialize(Hmd2::ReprojectionNotInitialized, __func__);
}
APS5_EXPORT("d3SEIYTP6wc", Hmd2BeforeInitializeReprojectionNotInitialized);
APS5_EXPORT("EVECbTvBezo", Hmd2BeforeInitializeReprojectionNotInitialized);
APS5_EXPORT("ciusPdJKjSU", Hmd2BeforeInitializeReprojectionNotInitialized);
int APS5_VABI sceHmd2ReprojectionSetUserEventStart(KernelEqueue queue, std::int32_t eventId) {
    return BeforeInitialize(Hmd2::ReprojectionNotInitialized, __func__);
}
int APS5_VABI sceHmd2ReprojectionSetUserEventEnd(KernelEqueue queue, std::int32_t eventId) {
    return BeforeInitialize(Hmd2::ReprojectionNotInitialized, __func__);
}
int APS5_VABI sceHmd2ReprojectionClearUserEventStart() {
    return BeforeInitialize(Hmd2::ReprojectionNotInitialized, __func__);
}
int APS5_VABI sceHmd2ReprojectionClearUserEventEnd() {
    return BeforeInitialize(Hmd2::ReprojectionNotInitialized, __func__);
}
int APS5_VABI sceHmd2ReprojectionGetCurrentOutput(Hmd2::ReprojectionOutput* output) {
    return BeforeInitialize(Hmd2::ReprojectionNotInitialized, __func__);
}
APS5_EXPORT("aGOWaEeg04I", Hmd2BeforeInitializeReprojectionNotInitialized);
APS5_EXPORT("9up+EPhw1No", Hmd2BeforeInitializeReprojectionNotInitialized);

std::int32_t APS5_VABI Hmd2BeforeInitializeError002C() {
    return BeforeInitialize(Hmd2::Error002C, __func__);
}
int APS5_VABI sceHmd2GazeObjectSelectionTerminate() {
    return BeforeInitialize(Hmd2::Error002C, __func__);
}

std::int32_t APS5_VABI Hmd2BeforeInitializeError1002() {
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("2LVD1EC5SYE", Hmd2BeforeInitializeError1002);
APS5_EXPORT("Bf87AL6zWxU", Hmd2BeforeInitializeError1002);
APS5_EXPORT("XKt7w1MODFY", Hmd2BeforeInitializeError1002);
APS5_EXPORT("Y5s22M-qUDA", Hmd2BeforeInitializeError1002);
APS5_EXPORT("JJKY-BtQJLg", Hmd2BeforeInitializeError1002);

std::int32_t APS5_VABI Hmd2BeforeInitializeError1003() {
    return BeforeInitialize(Hmd2::Error1003, __func__);
}
APS5_EXPORT("9kj9vw8UShQ", Hmd2BeforeInitializeError1003);

std::int32_t APS5_VABI Hmd2BeforeInitializeMinusOne() {
    return BeforeInitialize(-1, __func__);
}
APS5_EXPORT("EAkv8Mqt33E", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("Rv7bkmKvmz0", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("qsmAtGw1Ncc", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("uKhwYoMsIsA", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("WNdaigr94dI", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("P6S5ZVWf-H0", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("sun6NWE1sdg", Hmd2BeforeInitializeMinusOne);
APS5_EXPORT("--TFsJYXo04", Hmd2BeforeInitializeMinusOne);

std::int32_t APS5_VABI sceHmd2InternalTrackingOpen() {
    return BeforeInitialize(0, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalTrackingClose() {
    return BeforeInitialize(0, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalLedDetectorOpen() {
    return BeforeInitialize(0, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalLedDetectorClose() {
    return BeforeInitialize(0, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalTrackingStart() {
    return BeforeInitialize(-1, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalTrackingStop() {
    return BeforeInitialize(-1, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalLedDetectorStart() {
    return BeforeInitialize(-1, __func__);
}

std::int32_t APS5_VABI sceHmd2InternalLedDetectorStop() {
    return BeforeInitialize(-1, __func__);
}

std::int32_t APS5_VABI Hmd2BeforeInitializeValue0400() {
    return BeforeInitialize(0x400, __func__);
}
APS5_EXPORT("BWqcwjCozzI", Hmd2BeforeInitializeValue0400);

std::int32_t APS5_VABI Hmd2BeforeInitializeValue20000() {
    return BeforeInitialize(0x20000, __func__);
}
APS5_EXPORT("PRz7TdBGUI0", Hmd2BeforeInitializeValue20000);

int APS5_VABI sceHmd2InternalSetPortStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalLedDetectorStartBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalImageStartBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetPortConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetCameraSeeThroughMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetFakeLensSeparationDistance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalDfuSend(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalLedDetectorOpenBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalImageOpenBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalLedDetectorStopBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetDeviceConnection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalLedDetectorCloseBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetSidetone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalGetDeviceId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetBrightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalImageStopBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalSetVibrationMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalGetVr2dData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2InternalImageCloseBySlot(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI Hmd2CheckFirstThenError0004(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
APS5_EXPORT("9WeZr4kYfzk", Hmd2CheckFirstThenError0004);
APS5_EXPORT("N+5h1hCOFAg", Hmd2CheckFirstThenError0004);
APS5_EXPORT("w2vEs2y5ZmE", Hmd2CheckFirstThenError0004);
APS5_EXPORT("L5gat1pe0y0", Hmd2CheckFirstThenError0004);
APS5_EXPORT("4GnLHR9NJ1w", Hmd2CheckFirstThenError0004);
APS5_EXPORT("AHk7nGSzef0", Hmd2CheckFirstThenError0004);
APS5_EXPORT("ncjX9d3mc0M", Hmd2CheckFirstThenError0004);
APS5_EXPORT("l63QYAUuogc", Hmd2CheckFirstThenError0004);
APS5_EXPORT("mrcBySCdXSo", Hmd2CheckFirstThenError0004);
APS5_EXPORT("ieBra+td8sU", Hmd2CheckFirstThenError0004);
APS5_EXPORT("s+rzBnewgGs", Hmd2CheckFirstThenError0004);
APS5_EXPORT("Nfcm0GBdzZI", Hmd2CheckFirstThenError0004);
APS5_EXPORT("Tc9rasWfrm4", Hmd2CheckFirstThenError0004);
APS5_EXPORT("ckTbKTX4KZg", Hmd2CheckFirstThenError0004);
APS5_EXPORT("D7576PUbUjk", Hmd2CheckFirstThenError0004);
APS5_EXPORT("tTJ04SrBT8U", Hmd2CheckFirstThenError0004);
int APS5_VABI sceHmd2GazeGetDeviceStatus(std::int32_t* status) {
    if (status == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
int APS5_VABI sceHmd2GazeGetUserCalibrationStatus(Hmd2::GazeUserCalibrationStatus* status) {
    if (status == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error0004, __func__);
}
APS5_EXPORT("kozvHBNyxf0", Hmd2CheckFirstThenError0004);
APS5_EXPORT("638Tmg5y6U4", Hmd2CheckFirstThenError0004);

std::int32_t APS5_VABI Hmd2CheckFirstThenZero(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("8ETtFtp5zqY", Hmd2CheckFirstThenZero);
APS5_EXPORT("CWPkBm3EXQA", Hmd2CheckFirstThenZero);
APS5_EXPORT("R62liQQIAaA", Hmd2CheckFirstThenZero);
APS5_EXPORT("-bcWLHsdzo0", Hmd2CheckFirstThenZero);
int APS5_VABI sceHmd2SetFieldOfView(const Hmd2::FieldOfView* fieldOfView) {
    if (fieldOfView == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("xNBSz8xrGdk", Hmd2CheckFirstThenZero);
APS5_EXPORT("yUVPfN-jRIU", Hmd2CheckFirstThenZero);
APS5_EXPORT("BKPR3RZ5aO4", Hmd2CheckFirstThenZero);

std::int32_t APS5_VABI Hmd2CheckFirstThenError1002(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("IG4y+drWAWw", Hmd2CheckFirstThenError1002);
APS5_EXPORT("rhYUml9vppE", Hmd2CheckFirstThenError1002);
APS5_EXPORT("6574xHS+jZk", Hmd2CheckFirstThenError1002);
APS5_EXPORT("mXzaY6xVoxg", Hmd2CheckFirstThenError1002);
APS5_EXPORT("6af-HIZQQn4", Hmd2CheckFirstThenError1002);
APS5_EXPORT("6qWtdTHQsTk", Hmd2CheckFirstThenError1002);
APS5_EXPORT("pUm8t8xWhFg", Hmd2CheckFirstThenError1002);
APS5_EXPORT("A0XKI-WjxK8", Hmd2CheckFirstThenError1002);

std::int32_t APS5_VABI Hmd2CheckFirstThenMinusOne(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(-1, __func__);
}
APS5_EXPORT("A9s2j8OioPQ", Hmd2CheckFirstThenMinusOne);
APS5_EXPORT("YIgtmrL+HLY", Hmd2CheckFirstThenMinusOne);
APS5_EXPORT("Ng8zEkGxDNs", Hmd2CheckFirstThenMinusOne);
APS5_EXPORT("EtslTnqO1xU", Hmd2CheckFirstThenMinusOne);
APS5_EXPORT("hfrnRTryKQQ", Hmd2CheckFirstThenMinusOne);

std::int32_t APS5_VABI Hmd2CheckFirstTwoThenParameterInvalid(const void* a0, const void* a1) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}
APS5_EXPORT("PPX68fNToFM", Hmd2CheckFirstTwoThenParameterInvalid);
int APS5_VABI sceHmd2GazeObjectSelectionQueryMemory(const Hmd2::GazeObjectSelectionMemoryParameter* parameter, Hmd2::GazeObjectSelectionMemoryInfo* info) {
    if (parameter == nullptr) return Hmd2::ParameterNull;
    if (info == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}
int APS5_VABI sceHmd2GazeObjectSelectionGetSelectedObject(const Hmd2::GazeObjectSelectionParameter* parameter, Hmd2::GazeObjectSelectionResult* result) {
    if (parameter == nullptr) return Hmd2::ParameterNull;
    if (result == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}

std::int32_t APS5_VABI Hmd2CheckSecondThenError002C(const void* a0, const void* a1) {
    (void)a0;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error002C, __func__);
}
APS5_EXPORT("tWJPX2ycRMM", Hmd2CheckSecondThenError002C);
APS5_EXPORT("5UMB9PJA70o", Hmd2CheckSecondThenError002C);
APS5_EXPORT("xQs62qCk1EI", Hmd2CheckSecondThenError002C);

std::int32_t APS5_VABI Hmd2CheckFirstThenParameterInvalid(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}
APS5_EXPORT("MMc+bSlqkzk", Hmd2CheckFirstThenParameterInvalid);
APS5_EXPORT("NpZHgvSYgEA", Hmd2CheckFirstThenParameterInvalid);

std::int32_t APS5_VABI Hmd2CheckAllSixThenZero(const void* a0, const void* a1, const void* a2, const void* a3, const void* a4, const void* a5) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    if (a2 == nullptr) return Hmd2::ParameterNull;
    if (a3 == nullptr) return Hmd2::ParameterNull;
    if (a4 == nullptr) return Hmd2::ParameterNull;
    if (a5 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("z-serf9vsb0", Hmd2CheckAllSixThenZero);
APS5_EXPORT("IRMUFu+uG5s", Hmd2CheckAllSixThenZero);

std::int32_t APS5_VABI Hmd2CheckSecondThenParameterInvalid(const void* a0, const void* a1) {
    (void)a0;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}
APS5_EXPORT("72U64E+0z6U", Hmd2CheckSecondThenParameterInvalid);
int APS5_VABI sceHmd2GazeGetCalibrationDataMaxSize(std::int32_t type, std::uint32_t* size) {
    (void)type;
    if (size == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::ParameterInvalid, __func__);
}

std::int32_t APS5_VABI Hmd2CheckFirstThenError800d0203(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error800d0203, __func__);
}
APS5_EXPORT("Im0qL0j9Ubg", Hmd2CheckFirstThenError800d0203);

std::int32_t APS5_VABI Hmd2CheckFirstThenError1003(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1003, __func__);
}
APS5_EXPORT("ng6eI8uA0h4", Hmd2CheckFirstThenError1003);

std::int32_t APS5_VABI Hmd2CheckFirstThenError1010(const void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1010, __func__);
}
APS5_EXPORT("mPPGgjyJrCI", Hmd2CheckFirstThenError1010);

std::int32_t APS5_VABI Hmd2CheckFirstTwoThenError1002(const void* a0, const void* a1) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("ZupCJmV5BDI", Hmd2CheckFirstTwoThenError1002);

std::int32_t APS5_VABI Hmd2CheckFirstTwoThenError1003(const void* a0, const void* a1) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1003, __func__);
}
APS5_EXPORT("mAQZwOX1IHU", Hmd2CheckFirstTwoThenError1003);

std::int32_t APS5_VABI Hmd2CheckFirstAndThirdThenZero(const void* a0, const void* a1, const void* a2) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    (void)a1;
    if (a2 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("oacDkQkinLo", Hmd2CheckFirstAndThirdThenZero);

std::int32_t APS5_VABI Hmd2CheckFirstAndThirdThenError1003(const void* a0, const void* a1, const void* a2) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    (void)a1;
    if (a2 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1003, __func__);
}
APS5_EXPORT("zn7PHGzzEyI", Hmd2CheckFirstAndThirdThenError1003);

std::int32_t APS5_VABI Hmd2CheckSecondThenInvalidHandle(const void* a0, const void* a1) {
    (void)a0;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::InvalidHandle, __func__);
}
int APS5_VABI sceHmd2GazeInsertRawDataWithHandle(std::int32_t handle, Hmd2::GazeRawData* data) {
    (void)handle;
    if (data == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::InvalidHandle, __func__);
}

std::int32_t APS5_VABI Hmd2CheckSecondThenError1002(const void* a0, const void* a1) {
    (void)a0;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("W-ZIlUXcrNA", Hmd2CheckSecondThenError1002);

std::int32_t APS5_VABI Hmd2WriteStateTwo(void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    static constexpr std::uint8_t bytes[] = {0x02, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(a0, bytes, sizeof bytes);
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("omimgG5itSs", Hmd2WriteStateTwo);

std::int32_t APS5_VABI Hmd2WriteSize0x2000Align0x100(std::uint64_t* a0, std::uint64_t* a1) {
    if (a0 == nullptr || a1 == nullptr) return Hmd2::ParameterNull;
    *a0 = 0x2000;
    *a1 = 0x100;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("6PeHcAaKYg8", Hmd2WriteSize0x2000Align0x100);

std::int32_t APS5_VABI Hmd2WriteFourConstants(void* a0) {
    if (a0 == nullptr) return Hmd2::ParameterNull;
    static constexpr std::uint8_t bytes[] = {0xe1, 0xf0, 0x74, 0x3c, 0xd8, 0xf0, 0x74, 0x3c, 0x1f, 0xf1, 0x74, 0x3c, 0xd8, 0xf0, 0x74, 0x3c};
    std::memcpy(a0, bytes, sizeof bytes);
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("yur+xPdWPAg", Hmd2WriteFourConstants);

std::int32_t APS5_VABI Hmd2WriteSixHalvesThenInvalidHandle(std::uint64_t a0, void* a1) {
    (void)a0;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    static constexpr std::uint8_t bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f};
    std::memcpy(a1, bytes, sizeof bytes);
    return BeforeInitialize(Hmd2::InvalidHandle, __func__);
}
APS5_EXPORT("RnieDDp5ge4", Hmd2WriteSixHalvesThenInvalidHandle);
APS5_EXPORT("jM3NEJA11yw", Hmd2WriteSixHalvesThenInvalidHandle);

std::int32_t APS5_VABI Hmd2WriteTwelveHalvesThenInvalidHandle(std::uint64_t a0, void* a1) {
    (void)a0;
    if (a1 == nullptr) return Hmd2::ParameterNull;
    static constexpr std::uint8_t bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(a1, bytes, sizeof bytes);
    return BeforeInitialize(Hmd2::InvalidHandle, __func__);
}
APS5_EXPORT("b0H-n-tUk0U", Hmd2WriteTwelveHalvesThenInvalidHandle);

std::int32_t APS5_VABI Hmd2SlotUpTo3CheckSecondThirdThenZero(std::uint64_t a0, const void* a1, const void* a2) {
    if (a1 == nullptr || a2 == nullptr) return Hmd2::ParameterNull;
    if (a0 > 3) return Hmd2::ParameterInvalid;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("MXhloJ6Iw3w", Hmd2SlotUpTo3CheckSecondThirdThenZero);

std::int32_t APS5_VABI Hmd2SlotUpTo1ThenError1002(std::uint64_t a0) {
    if (a0 > 1) return Hmd2::ParameterInvalid;
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("19wrIIiqFgM", Hmd2SlotUpTo1ThenError1002);

std::int32_t APS5_VABI Hmd2SlotUpTo1CheckSecondThenError1002(std::uint64_t a0, const void* a1) {
    if (a1 == nullptr) return Hmd2::ParameterNull;
    if (a0 > 1) return Hmd2::ParameterInvalid;
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("Z389vAm47Ys", Hmd2SlotUpTo1CheckSecondThenError1002);

std::int32_t APS5_VABI Hmd2SlotUpTo1ThenMinusOne(std::uint64_t a0) {
    if (a0 > 1) return Hmd2::ParameterInvalid;
    return BeforeInitialize(-1, __func__);
}
APS5_EXPORT("cDcrvEG8j8k", Hmd2SlotUpTo1ThenMinusOne);
APS5_EXPORT("tuqJO6O6cb4", Hmd2SlotUpTo1ThenMinusOne);
APS5_EXPORT("fd4fu1cziaI", Hmd2SlotUpTo1ThenMinusOne);

std::int32_t APS5_VABI Hmd2SlotUpTo2ThenMinusOne(std::uint64_t a0) {
    if (a0 > 2) return Hmd2::ParameterInvalid;
    return BeforeInitialize(-1, __func__);
}
APS5_EXPORT("UxMg7iL4nRs", Hmd2SlotUpTo2ThenMinusOne);
APS5_EXPORT("K1GELwlaaGc", Hmd2SlotUpTo2ThenMinusOne);

std::int32_t APS5_VABI Hmd2SlotUpTo2ThenError1002(std::uint64_t a0) {
    if (a0 > 2) return Hmd2::ParameterInvalid;
    return BeforeInitialize(Hmd2::Error1002, __func__);
}
APS5_EXPORT("RFn39VCHjGY", Hmd2SlotUpTo2ThenError1002);

std::int32_t APS5_VABI Hmd2SlotUpTo2ThenError1003(std::uint64_t a0) {
    if (a0 > 2) return Hmd2::ParameterInvalid;
    return BeforeInitialize(Hmd2::Error1003, __func__);
}
APS5_EXPORT("FRoSjceDv10", Hmd2SlotUpTo2ThenError1003);

std::int32_t APS5_VABI Hmd2SlotUpTo2CheckSecondThenError002C(std::uint64_t a0, const void* a1) {
    if (a1 == nullptr) return Hmd2::ParameterNull;
    if (a0 > 2) return Hmd2::ParameterInvalid;
    return BeforeInitialize(Hmd2::Error002C, __func__);
}
APS5_EXPORT("wQ44gBwBw44", Hmd2SlotUpTo2CheckSecondThenError002C);

std::int32_t APS5_VABI Hmd2IndexZeroCheckSecondThenZero(std::uint64_t a0, const void* a1) {
    if (a1 == nullptr) return Hmd2::ParameterNull;
    if (a0 != 0) return Hmd2::Error100F;
    return BeforeInitialize(0, __func__);
}
APS5_EXPORT("wiv8+7ogrq0", Hmd2IndexZeroCheckSecondThenZero);

int APS5_VABI sceHmd2Unknown_CBkVZi9WhVo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CBkVZi9WhVo", sceHmd2Unknown_CBkVZi9WhVo_nid_no_patch);

}

extern "C" {

int APS5_VABI hmd2LegacyUnknown_Prx62Z9TC9M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Prx62Z9TC9M", hmd2LegacyUnknown_Prx62Z9TC9M_nid_no_patch);

int APS5_VABI hmd2LegacyUnknown_ZIkYdfc5p1o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZIkYdfc5p1o", hmd2LegacyUnknown_ZIkYdfc5p1o_nid_no_patch);

int APS5_VABI hmd2LegacyUnknown_hvGZSIlA7lk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hvGZSIlA7lk", hmd2LegacyUnknown_hvGZSIlA7lk_nid_no_patch);

int APS5_VABI hmd2LegacyUnknown_mt1yhLrXOMk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mt1yhLrXOMk", hmd2LegacyUnknown_mt1yhLrXOMk_nid_no_patch);

int APS5_VABI hmd2LegacyUnknown_qug_minus_eu8URpA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qug-eu8URpA", hmd2LegacyUnknown_qug_minus_eu8URpA_nid_no_patch);

int APS5_VABI hmd2LegacyUnknown_rmaZr_minus_K8c_plus_s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rmaZr-K8c+s", hmd2LegacyUnknown_rmaZr_minus_K8c_plus_s_nid_no_patch);

int APS5_VABI hmd2LegacyUnknown_y0VEHlxFQFs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("y0VEHlxFQFs", hmd2LegacyUnknown_y0VEHlxFQFs_nid_no_patch);

}
