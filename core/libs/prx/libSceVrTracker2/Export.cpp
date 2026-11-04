#include "prx/libSceVrTracker2/include/Api.hpp"
#include "prx/libSceHmd2/libSceHmd2.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

static_assert(sizeof(VrTracker2InitializeParameter) == 72);
static_assert(offsetof(VrTracker2InitializeParameter, threadAffinityMasks) == 16);
static_assert(offsetof(VrTracker2InitializeParameter, workMemory) == 32);
static_assert(offsetof(VrTracker2InitializeParameter, coordinateSystem) == 52);
static_assert(offsetof(VrTracker2InitializeParameter, reserved) == 58);
static_assert(sizeof(VrTracker2MemoryParameter) == 32);
static_assert(sizeof(VrTracker2MemoryRequirement) == 32);

static_assert(sizeof(VrTracker2LegacyParameter12) == 12);
static_assert(sizeof(VrTracker2LegacyParameter20) == 20);

static_assert(sizeof(VrTracker2MeshRequirement) == 16);
static_assert(sizeof(VrTracker2Pose) == 32);
static_assert(offsetof(VrTracker2Pose, orientation) == 16);

struct VrTracker2LegacyResultParameter {
    std::uint32_t size;
    std::int32_t deviceHandle;
    std::uint32_t reserved;
    std::uint8_t padding[4];
    std::uint64_t timestamp;
    std::uint32_t userTag;
    std::uint8_t padding2[4];
};

static_assert(sizeof(VrTracker2ResultParameter) == 48);
static_assert(offsetof(VrTracker2ResultParameter, timestamp) == 16);
static_assert(offsetof(VrTracker2ResultParameter, userTag) == 32);
static_assert(sizeof(VrTracker2LegacyResultParameter) == 32);
static_assert(offsetof(VrTracker2LegacyResultParameter, userTag) == 24);

static_assert(sizeof(VrTracker2BoundaryGeometry) == 3080);
static_assert(offsetof(VrTracker2BoundaryGeometry, points) == 8);
static_assert(sizeof(VrTracker2OrientedBoundingBox) == 80);
static_assert(offsetof(VrTracker2OrientedBoundingBox, transform) == 4);
static_assert(offsetof(VrTracker2OrientedBoundingBox, extents) == 68);

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t VR_TRACKER2_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x8A740001u);
constexpr std::int32_t VR_TRACKER2_ERROR_ALREADY_INITIALIZED = static_cast<std::int32_t>(0x8A740002u);
constexpr std::int32_t VR_TRACKER2_ERROR_INVALID_ARGUMENT = static_cast<std::int32_t>(0x8A740006u);

constexpr std::uint32_t TrackerMode = 0;
constexpr std::uint32_t SystemTrackingMode = 1000;
constexpr std::uint32_t SystemClientMode = 1001;
constexpr std::uint32_t MinimumWorkMemoryAlignment = 0x10000;
constexpr std::int32_t MinimumThreadPriority = 256;
constexpr std::int32_t MaximumThreadPriority = 767;
constexpr std::uint64_t TrackerAffinityLimit = 0x2000;
constexpr std::uint64_t SystemAffinityMask = 0xEA00;
constexpr std::uint32_t OptionModeSdkVersion = 0x7000000;

std::atomic<std::uint32_t> state{0};

std::uint32_t WorkMemorySize(std::uint32_t mode) {
    if (mode == SystemTrackingMode) return 7u << 20;
    if (mode == SystemClientMode) return 3u << 20;
    return 32u << 20;
}

std::uint32_t CompiledSdkVersion() {
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t sdkVersion;
    std::memcpy(&sdkVersion, process + 0x10, sizeof(sdkVersion));
    return sdkVersion;
}

std::int32_t ValidateMode(std::uint32_t mode) {
    if (mode == SystemTrackingMode || mode == SystemClientMode) NotImplemented_nid_no_patch("sceVrTracker2Initialize: application category of a system mode");
    return mode == TrackerMode ? SCE_OK : VR_TRACKER2_ERROR_INVALID_ARGUMENT;
}

bool PriorityAllowed(std::int32_t priority) {
    return priority >= MinimumThreadPriority && priority <= MaximumThreadPriority;
}

bool AffinityAllowed(std::uint32_t mode, std::uint64_t mask) {
    if (mode == TrackerMode) return mask < TrackerAffinityLimit;
    return (mask & ~SystemAffinityMask) == 0;
}

bool OptionModeAllowed(std::int8_t optionMode) {
    if (optionMode == -1 || optionMode == 0) return true;
    return optionMode == 1 && CompiledSdkVersion() >= OptionModeSdkVersion;
}

class InitializeSession {
public:
    std::int32_t OpenTracking() {
        return track(sceHmd2InternalTrackingOpen(), trackingOpened);
    }

    std::int32_t StartTracking() {
        return track(sceHmd2InternalTrackingStart(), trackingStarted);
    }

    std::int32_t OpenLedDetector() {
        return track(sceHmd2InternalLedDetectorOpen(), ledDetectorOpened);
    }

    std::int32_t StartLedDetector() {
        return track(sceHmd2InternalLedDetectorStart(), ledDetectorStarted);
    }

    void Rollback() {
        if (ledDetectorStarted) sceHmd2InternalLedDetectorStop();
        if (ledDetectorOpened) sceHmd2InternalLedDetectorClose();
        if (trackingStarted) sceHmd2InternalTrackingStop();
        if (trackingOpened) sceHmd2InternalTrackingClose();
        state.store(0);
    }

    std::int32_t Abort(std::int32_t result) {
        Rollback();
        return result;
    }

private:
    static std::int32_t track(std::int32_t result, bool& done) {
        done = result == SCE_OK;
        return result;
    }

    bool trackingOpened = false;
    bool trackingStarted = false;
    bool ledDetectorOpened = false;
    bool ledDetectorStarted = false;
};

std::int32_t ClearThenReportNotInitialized(void* output, std::size_t size) {
    if (output == nullptr) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    std::memset(output, 0, size);
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

}

static_assert(sizeof(VrTracker2RelativePoseResult) == 64);

static_assert(sizeof(VrTracker2ScalarResult) == 8);
static_assert(sizeof(VrTracker2TrackingOptions) == 28);
static_assert(offsetof(VrTracker2TrackingOptions, vector) == 16);
static_assert(sizeof(VrTracker2TrackingOptionsResult) == 40);
static_assert(offsetof(VrTracker2TrackingOptionsResult, position) == 28);

static_assert(sizeof(VrTracker2ModeParameter) == 12);
static_assert(sizeof(VrTracker2DiagnosticText) == 3072);

static_assert(offsetof(VrTracker2CameraFrame, pixels) == 24);
static_assert(offsetof(VrTracker2CameraFrame, bufferIndex) == 32);
static_assert(sizeof(VrTracker2CameraFrame) == 40);

static_assert(sizeof(VrTracker2SpatialQueryResult) == 36);
static_assert(offsetof(VrTracker2SpatialQueryResult, vectors) == 8);
static_assert(offsetof(VrTracker2SpatialQueryResult, value) == 32);

static_assert(sizeof(VrTracker2CoordinateResultParameter) == 56);
static_assert(offsetof(VrTracker2CoordinateResultParameter, parameter) == 8);
static_assert(sizeof(VrTracker2BoundaryFitResult) == 20);
static_assert(sizeof(VrTracker2VectorResult) == 16);

extern "C" {

std::int32_t APS5_VABI sceVrTracker2Finalize() {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2GetCoordinateSystem(std::uint32_t* coordinateSystem) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2GetPlayAreaBoundaryGeometry(VrTracker2BoundaryGeometry* geometry) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2GetPlayAreaOrientedBoundingBox(VrTracker2OrientedBoundingBox* box) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2GetResult(const VrTracker2ResultParameter* parameter, VrTracker2Result* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2LocateCoordinateSystem(std::uint32_t coordinateSystem, VrTracker2CoordinateLocation* location) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2RegisterDevice(std::uint32_t deviceType, std::int32_t deviceHandle) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2ResetLocalCoordinate(std::int32_t deviceHandle) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2SetCoordinateSystem(std::uint32_t coordinateSystem) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2UnregisterDevice(std::int32_t deviceHandle) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI sceVrTracker2QueryMemory(const VrTracker2MemoryParameter* parameter, VrTracker2MemoryRequirement* requirement) {
    if (parameter == nullptr || requirement == nullptr) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    if (parameter->size != sizeof(VrTracker2MemoryParameter)) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    if (parameter->mode == SystemTrackingMode || parameter->mode == SystemClientMode) {
        if (CompiledSdkVersion() < 0x1000000) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    } else if (parameter->mode != TrackerMode) {
        return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    }
    const std::uint32_t memorySize = WorkMemorySize(parameter->mode);
    for (const auto value : parameter->reserved)
        if (value != 0) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    *requirement = {sizeof(VrTracker2MemoryRequirement), memorySize, 64u << 10, {}};
    return SCE_OK;
}

std::int32_t APS5_VABI sceVrTracker2Initialize(const VrTracker2InitializeParameter* parameter) {
    if (parameter == nullptr || parameter->size != sizeof(VrTracker2InitializeParameter)) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    const std::uint32_t mode = parameter->mode;
    if (const std::int32_t result = ValidateMode(mode); result != SCE_OK) return result;
    if (parameter->workMemory == nullptr) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    if (parameter->workMemorySize < WorkMemorySize(mode)) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    if (parameter->workMemoryAlignment < MinimumWorkMemoryAlignment) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    for (const auto value : parameter->reserved)
        if (value != 0) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    for (const auto priority : parameter->threadPriorities)
        if (!PriorityAllowed(priority)) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    for (const auto mask : parameter->threadAffinityMasks)
        if (!AffinityAllowed(mode, mask)) return VR_TRACKER2_ERROR_INVALID_ARGUMENT;
    std::uint32_t expected = 0;
    if (!state.compare_exchange_strong(expected, 1)) return VR_TRACKER2_ERROR_ALREADY_INITIALIZED;
    InitializeSession session;
    if (mode == TrackerMode) std::memset(parameter->workMemory, 0, WorkMemorySize(mode));
    if (mode != SystemClientMode) {
        if (!OptionModeAllowed(parameter->optionMode)) return session.Abort(VR_TRACKER2_ERROR_INVALID_ARGUMENT);
        if (const std::int32_t result = session.OpenTracking(); result != SCE_OK) return session.Abort(result);
        if (const std::int32_t result = session.StartTracking(); result != SCE_OK) return session.Abort(result);
        if (const std::int32_t result = session.OpenLedDetector(); result != SCE_OK) return session.Abort(result);
        if (const std::int32_t result = session.StartLedDetector(); result != SCE_OK) return session.Abort(result);
    }
    session.Rollback();
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("1m6DD9BBmSE", VrTracker2GetRelativePose);
std::int32_t APS5_VABI VrTracker2GetRelativePose(std::uint32_t firstCoordinateSystem, std::uint32_t secondCoordinateSystem, VrTracker2RelativePoseResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("SgunslIO-C0", VrTracker2GetDiagnosticResult);
std::int32_t APS5_VABI VrTracker2GetDiagnosticResult(VrTracker2DiagnosticResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("KMIToVnHrhc", VrTracker2SetTrackingOptions);
std::int32_t APS5_VABI VrTracker2SetTrackingOptions(const VrTracker2TrackingOptions* options) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("XfEBVemeJyE", VrTracker2GetTrackingOptions);
std::int32_t APS5_VABI VrTracker2GetTrackingOptions(VrTracker2TrackingOptionsResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("y79S3-zrJKg", VrTracker2GetExtendedResult);
std::int32_t APS5_VABI VrTracker2GetExtendedResult(VrTracker2ExtendedResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("b777Kpfe2J8", VrTracker2GetCompositePose);
std::int32_t APS5_VABI VrTracker2GetCompositePose(const VrTracker2ResultParameter* parameter, VrTracker2CompositePoseResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("UfUYgiT7VK0", VrTracker2GetCounter);
std::int32_t APS5_VABI VrTracker2GetCounter(std::uint64_t* counter, std::uint32_t selector) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("MB7B6396K6U", VrTracker2SetTrackingConfiguration);
std::int32_t APS5_VABI VrTracker2SetTrackingConfiguration(const VrTracker2TrackingConfiguration* configuration) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("H2Mnx7gz4ac", VrTracker2NotInitialized);
APS5_EXPORT("SgNHzN6XaEc", VrTracker2GetTrackingConfiguration);
std::int32_t APS5_VABI VrTracker2GetTrackingConfiguration(VrTracker2TrackingConfiguration* configuration) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("sYBNLrMxjAM", VrTracker2GetCameraImages);
std::int32_t APS5_VABI VrTracker2GetCameraImages(VrTracker2CameraImages* images) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("3gQncidodmw", VrTracker2GetCameraImage);
std::int32_t APS5_VABI VrTracker2GetCameraImage(void* image, std::uint32_t size, std::uint32_t* frameId) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("qnCmEbUGqWY", VrTracker2GetDiagnosticText);
std::int32_t APS5_VABI VrTracker2GetDiagnosticText(VrTracker2DiagnosticText* text) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("oGNn7JIdMHw", VrTracker2GetSensorFrame);
std::int32_t APS5_VABI VrTracker2GetSensorFrame(VrTracker2SensorFrame* frame) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("80IxOn8swIA", VrTracker2NotInitialized);
APS5_EXPORT("NTbJNAZA4ro", VrTracker2NotInitialized);
APS5_EXPORT("914+9oJxNts", VrTracker2GetTrackingStatus);
std::int32_t APS5_VABI VrTracker2GetTrackingStatus(VrTracker2TrackingStatus* status) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("c6SZtdEjH0E", VrTracker2GetBoundaryParameter);
std::int32_t APS5_VABI VrTracker2GetBoundaryParameter(VrTracker2BoundaryParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("74IXIMe+Lto", VrTracker2SetBoundaryParameter);
std::int32_t APS5_VABI VrTracker2SetBoundaryParameter(const VrTracker2BoundaryParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("hmahOYMiYNY", VrTracker2SetMode);
std::int32_t APS5_VABI VrTracker2SetMode(const VrTracker2ModeParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("pNXc6ZlLq-8", VrTracker2GetScalarResult);
std::int32_t APS5_VABI VrTracker2GetScalarResult(VrTracker2ScalarResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("omJ7b8FqAN0", VrTracker2LegacyQuery12);
std::int32_t APS5_VABI VrTracker2LegacyQuery12(const VrTracker2LegacyParameter12* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("G2PWj1HLqn8", VrTracker2LegacyQuery20);
std::int32_t APS5_VABI VrTracker2LegacyQuery20(const VrTracker2LegacyParameter20* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("73zcxAFawYk", VrTracker2NotInitialized);
APS5_EXPORT("vegrWFNiQwc", VrTracker2NotInitialized);
APS5_EXPORT("6r01IawmypA", VrTracker2NotInitialized);
APS5_EXPORT("zeof7sF2fqE", VrTracker2UnsupportedCoordinateOperation);
std::int32_t APS5_VABI VrTracker2UnsupportedCoordinateOperation(const VrTracker2UnsupportedCoordinateParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("Z0rfHOHQV1w", VrTracker2NotInitialized);
APS5_EXPORT("4l7x+uy3dWw", VrTracker2NotInitialized);
APS5_EXPORT("ZXlt-Xb3da4", VrTracker2NotInitialized);
APS5_EXPORT("BSwAdB3GM3o", VrTracker2NotInitialized);
APS5_EXPORT("9ATuhVfn9Dw", VrTracker2NotInitialized);
APS5_EXPORT("wjmVbmf58rE", VrTracker2NotInitialized);
APS5_EXPORT("4+B+TcHEHks", VrTracker2NotInitialized);
APS5_EXPORT("cjDz4kYJKTw", VrTracker2NotInitialized);
APS5_EXPORT("BjVg0fKV9VA", VrTracker2NotInitialized);
APS5_EXPORT("oDV5j-2Luic", VrTracker2NotInitialized);
APS5_EXPORT("t+86o9sCZ-Y", VrTracker2AcquireCameraFrame);
std::int32_t APS5_VABI VrTracker2AcquireCameraFrame(std::uint32_t selector, VrTracker2CameraFrame* frame) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("TIpYP35JzUs", VrTracker2AcquireCameraFrameWithMetadata);
std::int32_t APS5_VABI VrTracker2AcquireCameraFrameWithMetadata(std::uint32_t selector, VrTracker2CameraFrame* frame, VrTracker2CameraFrameMetadata* metadata) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("LrSc+KuxIkg", VrTracker2ReleaseCameraFrame);
std::int32_t APS5_VABI VrTracker2ReleaseCameraFrame(const VrTracker2CameraFrame* frame) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("Ur+M5wy+QBY", VrTracker2SetScalarOption);
std::int32_t APS5_VABI VrTracker2SetScalarOption(float value) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("OvKC351Udqo", VrTracker2GetScalarOption);
std::int32_t APS5_VABI VrTracker2GetScalarOption(float* value) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("LQV4nz6wteY", VrTracker2NotInitialized);
APS5_EXPORT("x0pGthEwf88", VrTracker2NotInitialized);
APS5_EXPORT("d5Fjs0rHkBg", VrTracker2SelectCoordinateSystem);
std::int32_t APS5_VABI VrTracker2SelectCoordinateSystem(std::uint32_t coordinateSystem) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("Yed3ZONcuMk", VrTracker2GetSelectedCoordinateSystem);
std::int32_t APS5_VABI VrTracker2GetSelectedCoordinateSystem(std::uint32_t* coordinateSystem) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}
;
APS5_EXPORT("fkptW6uRdWc", VrTracker2ResetDevicePose);
std::int32_t APS5_VABI VrTracker2ResetDevicePose(std::int32_t deviceHandle) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("HDlD-xE1Xuk", VrTracker2ResetPose);
std::int32_t APS5_VABI VrTracker2ResetPose(const VrTracker2ResetPoseParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("IGdE6nsMTAY", VrTracker2QueryPosition);
std::int32_t APS5_VABI VrTracker2QueryPosition(const float* position, VrTracker2SpatialQueryResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("DSTergmOvvE", VrTracker2QueryDevicePosition);
std::int32_t APS5_VABI VrTracker2QueryDevicePosition(std::int32_t deviceHandle, VrTracker2SpatialQueryResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("B7hPwvvg79E", VrTracker2GetTrackingBounds);
std::int32_t APS5_VABI VrTracker2GetTrackingBounds(VrTracker2OrientedBoundingBox* bounds) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("ZwnIsDe1eNE", VrTracker2GetDeviceState);
std::int32_t APS5_VABI VrTracker2GetDeviceState(std::int32_t deviceHandle, std::int32_t* state) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("B2vw+s-0Dfo", VrTracker2GetDeviceCalibrationStatus);
std::int32_t APS5_VABI VrTracker2GetDeviceCalibrationStatus(std::int32_t deviceHandle, VrTracker2DeviceCalibrationStatus* status) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("4LNTLivfRuA", VrTracker2SetDeviceBoundary);
std::int32_t APS5_VABI VrTracker2SetDeviceBoundary(std::int32_t deviceHandle, std::int32_t selector, const VrTracker2DeviceBoundaryParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("edajhZhz9Uo", VrTracker2GetDeviceBoundary);
std::int32_t APS5_VABI VrTracker2GetDeviceBoundary(std::int32_t deviceHandle, std::int32_t selector, VrTracker2DeviceBoundaryParameter* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("QS+7oqiszuc", VrTracker2ApplyDeviceBoundary);
std::int32_t APS5_VABI VrTracker2ApplyDeviceBoundary(std::int32_t deviceHandle, std::int32_t selector) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("-dDyvzd2jf0", VrTracker2GetDeviceMapState);
std::int32_t APS5_VABI VrTracker2GetDeviceMapState(std::int32_t deviceHandle, std::int32_t* state) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("k8JDLkCrOgs", VrTracker2InitializeMap);
std::int32_t APS5_VABI VrTracker2InitializeMap(std::int32_t deviceHandle, const float* parameter) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("-HFx+dnDSOI", VrTracker2GetCoordinateTrackingResult);
std::int32_t APS5_VABI VrTracker2GetCoordinateTrackingResult(const VrTracker2CoordinateResultParameter* parameter, VrTracker2ExtendedTrackingResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("wgDeDXirl0g", VrTracker2GetExtendedTrackingResult);
std::int32_t APS5_VABI VrTracker2GetExtendedTrackingResult(const VrTracker2ResultParameter* parameter, VrTracker2ExtendedTrackingResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("a1kWSTYGBxg", VrTracker2FitBoundary);
std::int32_t APS5_VABI VrTracker2FitBoundary(const VrTracker2BoundaryGeometry* boundary, VrTracker2BoundaryFitResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("zUwxBEKr3fc", VrTracker2FitBoundaryWithBounds);
std::int32_t APS5_VABI VrTracker2FitBoundaryWithBounds(const VrTracker2BoundaryGeometry* boundary, VrTracker2OrientedBoundingBox* firstBounds, VrTracker2OrientedBoundingBox* secondBounds, VrTracker2BoundaryFitResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("TC7VTVzj0xc", VrTracker2GetDeviceCenter);
std::int32_t APS5_VABI VrTracker2GetDeviceCenter(std::int32_t deviceHandle, VrTracker2VectorResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("1DwROxtuANs", VrTracker2GetRuntimeStatus);
std::int32_t APS5_VABI VrTracker2GetRuntimeStatus(VrTracker2RuntimeStatus* status) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("wwbt4s8cbMQ", VrTracker2GetTrackingFlags);
std::int32_t APS5_VABI VrTracker2GetTrackingFlags(bool* active, std::uint8_t* flags) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("eGxBJ0IolOA", VrTracker2GetDiagnosticState);
std::int32_t APS5_VABI VrTracker2GetDiagnosticState(VrTracker2DiagnosticState* state) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("9BaFhspHF+E", VrTracker2GetExtendedDiagnosticState);
std::int32_t APS5_VABI VrTracker2GetExtendedDiagnosticState(VrTracker2ExtendedDiagnosticState* state) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("lb9q5nJ4vzk", VrTracker2GetSensorDiagnostics);
std::int32_t APS5_VABI VrTracker2GetSensorDiagnostics(VrTracker2SensorDiagnostics* diagnostics) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("ZsD1-thjXS4", VrTracker2GetDeviceDiagnostics);
std::int32_t APS5_VABI VrTracker2GetDeviceDiagnostics(VrTracker2DeviceDiagnostics* diagnostics) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("QdHQnM0XICI", VrTracker2GetTimingDiagnostics);
std::int32_t APS5_VABI VrTracker2GetTimingDiagnostics(VrTracker2TimingDiagnostics* diagnostics) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("Ek4WMOvwv6g", VrTracker2GetMapConfiguration);
std::int32_t APS5_VABI VrTracker2GetMapConfiguration(VrTracker2MapConfiguration* configuration) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("W1YAO6G+hNc", VrTracker2SetMapConfiguration);
std::int32_t APS5_VABI VrTracker2SetMapConfiguration(const VrTracker2MapConfiguration* configuration) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("HAJB7irjl+0", VrTracker2GetMapStatus);
std::int32_t APS5_VABI VrTracker2GetMapStatus(VrTracker2MapStatus* status) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("x2mN50nKScs", VrTracker2NotInitialized);
APS5_EXPORT("cCKExge5d3A", VrTracker2NotInitialized);
APS5_EXPORT("gn6e5I4g9bw", VrTracker2NotInitialized);
APS5_EXPORT("OYTKt9diFOY", VrTracker2NotInitialized);
APS5_EXPORT("loU5lT92Zrw", VrTracker2NotInitialized);
APS5_EXPORT("o6fsgA3R2Yc", VrTracker2NotInitialized);
APS5_EXPORT("AeBtEIE0idk", VrTracker2NotInitialized);
APS5_EXPORT("DBa3wov7dKQ", VrTracker2NotInitialized);
APS5_EXPORT("ojTGClQ52r4", VrTracker2BuildMesh);
std::int32_t APS5_VABI VrTracker2BuildMesh(VrTracker2MeshVertex* vertices, std::uint16_t* indices, std::uint32_t format) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("oOLE62H66z0", VrTracker2GetMapScalar);
std::int32_t APS5_VABI VrTracker2GetMapScalar(float* value, const VrTracker2MapData* data) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("MHekhnRBKAw", VrTracker2GetTimedPose);
std::int32_t APS5_VABI VrTracker2GetTimedPose(VrTracker2TimedPoseResult* result, std::uint64_t timestamp, std::uint64_t userTag) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("JnSmj+5E8WY", VrTracker2GetMapData);
std::int32_t APS5_VABI VrTracker2GetMapData(VrTracker2MapData* data, std::uint32_t size) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("N1P+melAgb8", VrTracker2GetPoseScalar);
std::int32_t APS5_VABI VrTracker2GetPoseScalar(float* value) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("-e7A3rAM3Yk", VrTracker2SubmitImageFrame);
std::int32_t APS5_VABI VrTracker2SubmitImageFrame(const VrTracker2ImageFrame* frame) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("RefT1spBleg", VrTracker2GetImagePose);
std::int32_t APS5_VABI VrTracker2GetImagePose(VrTracker2ImagePoseResult* result) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("DsOfryhbm4U", VrTracker2GetSensorSummary);
std::int32_t APS5_VABI VrTracker2GetSensorSummary(VrTracker2SensorSummary* summary) {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI VrTracker2NotInitialized() {
    return VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("XknY0IDcOwU", VrTracker2ClearWideResult);
std::int32_t APS5_VABI VrTracker2ClearWideResult(void* result) {
    return ClearThenReportNotInitialized(result, 24);
}

APS5_EXPORT("ark13GkiskA", VrTracker2ClearNarrowResult);
std::int32_t APS5_VABI VrTracker2ClearNarrowResult(void* result) {
    return ClearThenReportNotInitialized(result, 16);
}

APS5_EXPORT("-YvjOUph8B0", VrTracker2CheckSecondThenReportNotInitialized);
std::int32_t APS5_VABI VrTracker2CheckSecondThenReportNotInitialized(const void* first, const void* second) {
    (void)first;
    return second == nullptr ? VR_TRACKER2_ERROR_INVALID_ARGUMENT : VR_TRACKER2_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("QyZvFyf-QEk", VrTracker2Accept);
APS5_EXPORT("fTYZOZepkqE", VrTracker2Accept);
std::int32_t APS5_VABI VrTracker2Accept() {
    return SCE_OK;
}

APS5_EXPORT("TZIUIk9id3c", VrTracker2FillDefaults);
std::int32_t APS5_VABI VrTracker2FillDefaults(void* output) {
    if (output == nullptr) APS5_INVALID_ARG_EX;
    auto* bytes = static_cast<std::uint8_t*>(output);
    const std::uint32_t first = 0xC000;
    const std::uint32_t second = 0;
    const std::uint16_t third = 0x40;
    std::memcpy(bytes + 4, &first, sizeof first);
    std::memcpy(bytes + 8, &second, sizeof second);
    std::memcpy(bytes + 12, &third, sizeof third);
    std::memcpy(bytes + 14, &third, sizeof third);
    return SCE_OK;
}

APS5_EXPORT("IB0qcDVu8vU", VrTracker2ClearWord);
std::int32_t APS5_VABI VrTracker2ClearWord(void* output) {
    if (output == nullptr) APS5_INVALID_ARG_EX;
    std::memset(output, 0, sizeof(std::uint32_t));
    return SCE_OK;
}

APS5_EXPORT("jRmuXjsknsI", VrTracker2Unknown01);
std::int32_t APS5_VABI VrTracker2Unknown01(VrTracker2CalibrationResult* result, std::uint32_t capacity) {
    NotImplemented_nid_no_patch("jRmuXjsknsI");
    return 0;
}

std::int32_t APS5_VABI sceVrTracker2GetRawCameraMeshSize(VrTracker2MeshRequirement* requirement, std::uint32_t format) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("9R6W33Au4B4", VrTracker2Unknown03);
std::int32_t APS5_VABI VrTracker2Unknown03(std::uint32_t mode, void* output, std::size_t capacity, std::size_t* writtenSize) {
    NotImplemented_nid_no_patch("9R6W33Au4B4");
    return 0;
}

APS5_EXPORT("O8ZkTFtvUL4", VrTracker2Unknown04);
std::int32_t APS5_VABI VrTracker2Unknown04(const VrTracker2ResultParameter* parameter, VrTracker2Result* result) {
    NotImplemented_nid_no_patch("O8ZkTFtvUL4");
    return 0;
}

APS5_EXPORT("940WPg+WwIk", VrTracker2Unknown05);
std::int32_t APS5_VABI VrTracker2Unknown05(VrTracker2Pose* pose, std::uint64_t coordinateId) {
    NotImplemented_nid_no_patch("940WPg+WwIk");
    return 0;
}

APS5_EXPORT("oZDm1bynCaA", VrTracker2Unknown06);
std::int32_t APS5_VABI VrTracker2Unknown06(const VrTracker2TelemetryData* data) {
    NotImplemented_nid_no_patch("oZDm1bynCaA");
    return 0;
}

APS5_EXPORT("U1VYczwqukY", VrTracker2Unknown07);
std::int32_t APS5_VABI VrTracker2Unknown07(const VrTracker2PoseParameter* parameter, VrTracker2PoseResult* result) {
    NotImplemented_nid_no_patch("U1VYczwqukY");
    return 0;
}

}
