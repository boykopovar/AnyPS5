#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

struct VrTracker2InitializeParameter {
    std::uint32_t size;
    std::uint32_t mode;
    std::int32_t threadPriorities[2];
    std::uint64_t threadAffinityMasks[2];
    void* workMemory;
    std::uint32_t workMemorySize;
    std::uint32_t workMemoryAlignment;
    std::uint32_t option;
    std::uint32_t coordinateSystem;
    std::uint8_t disableOption;
    std::int8_t optionMode;
    std::uint8_t reserved[14];
};

struct VrTracker2MemoryParameter {
    std::uint32_t size;
    std::uint32_t mode;
    std::uint8_t reserved[24];
};

struct VrTracker2MemoryRequirement {
    std::uint32_t size;
    std::uint32_t memorySize;
    std::uint32_t memoryAlignment;
    std::uint8_t reserved[20];
};

struct VrTracker2LegacyParameter12 {
    std::uint32_t size;
    std::uint8_t opaque[8];
};

struct VrTracker2LegacyParameter20 {
    std::uint32_t size;
    std::uint8_t opaque[16];
};

struct VrTracker2UnsupportedCoordinateParameter;

struct VrTracker2CalibrationResult;

struct VrTracker2TelemetryData;

struct VrTracker2PoseParameter;

struct VrTracker2PoseResult;

struct VrTracker2MeshRequirement {
    std::uint32_t vertexCount;
    std::uint32_t indexCount;
    std::uint32_t vertexDataSize;
    std::uint32_t indexDataSize;
};

struct VrTracker2Pose {
    float position[3];
    std::uint8_t padding[4];
    float orientation[4];
};

struct VrTracker2ResultParameter {
    std::uint32_t size;
    std::int32_t deviceHandle;
    std::uint32_t reserved;
    std::uint8_t padding[4];
    std::uint64_t timestamp;
    std::uint32_t reserved2;
    std::uint8_t padding2[4];
    std::uint64_t userTag;
    std::uint32_t reserved3[2];
};

struct VrTracker2Result;

struct VrTracker2CoordinateLocation;

struct VrTracker2BoundaryGeometry {
    std::uint32_t size;
    std::uint32_t pointCount;
    float points[256][3];
};

struct VrTracker2OrientedBoundingBox {
    std::uint32_t size;
    float transform[16];
    float extents[3];
};

struct VrTracker2RelativePoseResult {
    VrTracker2Pose pose;
    std::uint8_t reserved[32];
};

struct VrTracker2ScalarResult {
    std::uint32_t size;
    float value;
};

struct VrTracker2TrackingOptions {
    std::int32_t options[3];
    std::uint8_t reset;
    std::uint8_t reserved[3];
    float vector[3];
};

struct VrTracker2TrackingOptionsResult {
    VrTracker2TrackingOptions options;
    float position[3];
};

struct VrTracker2DiagnosticResult;

struct VrTracker2ExtendedResult;

struct VrTracker2TrackingConfiguration;

struct VrTracker2ModeParameter {
    std::uint32_t size;
    std::uint32_t mode;
    float value;
};

struct VrTracker2DiagnosticText {
    char sections[6][512];
};

struct VrTracker2CompositePoseResult;

struct VrTracker2CameraImages;

struct VrTracker2SensorFrame;

struct VrTracker2TrackingStatus;

struct VrTracker2BoundaryParameter;

struct VrTracker2CameraFrame {
    std::uint32_t selector;
    std::uint32_t identifiers[2];
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitch;
    const void* pixels;
    std::int32_t bufferIndex;
};

struct VrTracker2CameraFrameMetadata;

struct VrTracker2SpatialQueryResult {
    std::uint32_t size;
    std::int32_t status;
    float vectors[2][3];
    float value;
};

struct VrTracker2DeviceCalibrationStatus;

struct VrTracker2DeviceBoundaryParameter;

struct VrTracker2ResetPoseParameter;

struct VrTracker2MapData;

struct VrTracker2MapStatus;

struct VrTracker2RuntimeStatus;

struct VrTracker2DiagnosticState;

struct VrTracker2ExtendedDiagnosticState;

struct VrTracker2SensorDiagnostics;

struct VrTracker2DeviceDiagnostics;

struct VrTracker2TimingDiagnostics;

struct VrTracker2MapConfiguration;

struct VrTracker2SensorSummary;

struct VrTracker2CoordinateResultParameter {
    std::uint32_t coordinateSystem;
    std::uint32_t reserved;
    VrTracker2ResultParameter parameter;
};

struct VrTracker2BoundaryFitResult {
    std::uint8_t flags[4];
    float dimensions[4];
};

struct VrTracker2VectorResult {
    std::uint32_t size;
    float value[3];
};

struct VrTracker2ExtendedTrackingResult;

struct VrTracker2MeshVertex;

struct VrTracker2TimedPoseResult;

struct VrTracker2ImagePoseResult;

struct VrTracker2ImageFrame;

extern "C" {

std::int32_t APS5_VABI sceVrTracker2Finalize();
std::int32_t APS5_VABI sceVrTracker2GetCoordinateSystem(std::uint32_t* coordinateSystem);
std::int32_t APS5_VABI sceVrTracker2GetPlayAreaBoundaryGeometry(VrTracker2BoundaryGeometry* geometry);
std::int32_t APS5_VABI sceVrTracker2GetPlayAreaOrientedBoundingBox(VrTracker2OrientedBoundingBox* box);
std::int32_t APS5_VABI sceVrTracker2GetResult(const VrTracker2ResultParameter* parameter, VrTracker2Result* result);
std::int32_t APS5_VABI sceVrTracker2LocateCoordinateSystem(std::uint32_t coordinateSystem, VrTracker2CoordinateLocation* location);
std::int32_t APS5_VABI sceVrTracker2RegisterDevice(std::uint32_t deviceType, std::int32_t deviceHandle);
std::int32_t APS5_VABI sceVrTracker2ResetLocalCoordinate(std::int32_t deviceHandle);
std::int32_t APS5_VABI sceVrTracker2SetCoordinateSystem(std::uint32_t coordinateSystem);
std::int32_t APS5_VABI sceVrTracker2UnregisterDevice(std::int32_t deviceHandle);
std::int32_t APS5_VABI sceVrTracker2QueryMemory(const VrTracker2MemoryParameter* parameter, VrTracker2MemoryRequirement* requirement);
std::int32_t APS5_VABI sceVrTracker2Initialize(const VrTracker2InitializeParameter* parameter);
std::int32_t APS5_VABI VrTracker2GetRelativePose(std::uint32_t firstCoordinateSystem, std::uint32_t secondCoordinateSystem, VrTracker2RelativePoseResult* result);
std::int32_t APS5_VABI VrTracker2GetDiagnosticResult(VrTracker2DiagnosticResult* result);
std::int32_t APS5_VABI VrTracker2SetTrackingOptions(const VrTracker2TrackingOptions* options);
std::int32_t APS5_VABI VrTracker2GetTrackingOptions(VrTracker2TrackingOptionsResult* result);
std::int32_t APS5_VABI VrTracker2GetExtendedResult(VrTracker2ExtendedResult* result);
std::int32_t APS5_VABI VrTracker2GetCompositePose(const VrTracker2ResultParameter* parameter, VrTracker2CompositePoseResult* result);
std::int32_t APS5_VABI VrTracker2GetCounter(std::uint64_t* counter, std::uint32_t selector);
std::int32_t APS5_VABI VrTracker2SetTrackingConfiguration(const VrTracker2TrackingConfiguration* configuration);
std::int32_t APS5_VABI VrTracker2GetTrackingConfiguration(VrTracker2TrackingConfiguration* configuration);
std::int32_t APS5_VABI VrTracker2GetCameraImages(VrTracker2CameraImages* images);
std::int32_t APS5_VABI VrTracker2GetCameraImage(void* image, std::uint32_t size, std::uint32_t* frameId);
std::int32_t APS5_VABI VrTracker2GetDiagnosticText(VrTracker2DiagnosticText* text);
std::int32_t APS5_VABI VrTracker2GetSensorFrame(VrTracker2SensorFrame* frame);
std::int32_t APS5_VABI VrTracker2GetTrackingStatus(VrTracker2TrackingStatus* status);
std::int32_t APS5_VABI VrTracker2GetBoundaryParameter(VrTracker2BoundaryParameter* parameter);
std::int32_t APS5_VABI VrTracker2SetBoundaryParameter(const VrTracker2BoundaryParameter* parameter);
std::int32_t APS5_VABI VrTracker2SetMode(const VrTracker2ModeParameter* parameter);
std::int32_t APS5_VABI VrTracker2GetScalarResult(VrTracker2ScalarResult* result);
std::int32_t APS5_VABI VrTracker2LegacyQuery12(const VrTracker2LegacyParameter12* parameter);
std::int32_t APS5_VABI VrTracker2LegacyQuery20(const VrTracker2LegacyParameter20* parameter);
std::int32_t APS5_VABI VrTracker2UnsupportedCoordinateOperation(const VrTracker2UnsupportedCoordinateParameter* parameter);
std::int32_t APS5_VABI VrTracker2AcquireCameraFrame(std::uint32_t selector, VrTracker2CameraFrame* frame);
std::int32_t APS5_VABI VrTracker2AcquireCameraFrameWithMetadata(std::uint32_t selector, VrTracker2CameraFrame* frame, VrTracker2CameraFrameMetadata* metadata);
std::int32_t APS5_VABI VrTracker2ReleaseCameraFrame(const VrTracker2CameraFrame* frame);
std::int32_t APS5_VABI VrTracker2SetScalarOption(float value);
std::int32_t APS5_VABI VrTracker2GetScalarOption(float* value);
std::int32_t APS5_VABI VrTracker2SelectCoordinateSystem(std::uint32_t coordinateSystem);
std::int32_t APS5_VABI VrTracker2GetSelectedCoordinateSystem(std::uint32_t* coordinateSystem);
std::int32_t APS5_VABI VrTracker2ResetDevicePose(std::int32_t deviceHandle);
std::int32_t APS5_VABI VrTracker2ResetPose(const VrTracker2ResetPoseParameter* parameter);
std::int32_t APS5_VABI VrTracker2QueryPosition(const float* position, VrTracker2SpatialQueryResult* result);
std::int32_t APS5_VABI VrTracker2QueryDevicePosition(std::int32_t deviceHandle, VrTracker2SpatialQueryResult* result);
std::int32_t APS5_VABI VrTracker2GetTrackingBounds(VrTracker2OrientedBoundingBox* bounds);
std::int32_t APS5_VABI VrTracker2GetDeviceState(std::int32_t deviceHandle, std::int32_t* state);
std::int32_t APS5_VABI VrTracker2GetDeviceCalibrationStatus(std::int32_t deviceHandle, VrTracker2DeviceCalibrationStatus* status);
std::int32_t APS5_VABI VrTracker2SetDeviceBoundary(std::int32_t deviceHandle, std::int32_t selector, const VrTracker2DeviceBoundaryParameter* parameter);
std::int32_t APS5_VABI VrTracker2GetDeviceBoundary(std::int32_t deviceHandle, std::int32_t selector, VrTracker2DeviceBoundaryParameter* parameter);
std::int32_t APS5_VABI VrTracker2ApplyDeviceBoundary(std::int32_t deviceHandle, std::int32_t selector);
std::int32_t APS5_VABI VrTracker2GetDeviceMapState(std::int32_t deviceHandle, std::int32_t* state);
std::int32_t APS5_VABI VrTracker2InitializeMap(std::int32_t deviceHandle, const float* parameter);
std::int32_t APS5_VABI VrTracker2GetCoordinateTrackingResult(const VrTracker2CoordinateResultParameter* parameter, VrTracker2ExtendedTrackingResult* result);
std::int32_t APS5_VABI VrTracker2GetExtendedTrackingResult(const VrTracker2ResultParameter* parameter, VrTracker2ExtendedTrackingResult* result);
std::int32_t APS5_VABI VrTracker2FitBoundary(const VrTracker2BoundaryGeometry* boundary, VrTracker2BoundaryFitResult* result);
std::int32_t APS5_VABI VrTracker2FitBoundaryWithBounds(const VrTracker2BoundaryGeometry* boundary, VrTracker2OrientedBoundingBox* firstBounds, VrTracker2OrientedBoundingBox* secondBounds, VrTracker2BoundaryFitResult* result);
std::int32_t APS5_VABI VrTracker2GetDeviceCenter(std::int32_t deviceHandle, VrTracker2VectorResult* result);
std::int32_t APS5_VABI VrTracker2GetRuntimeStatus(VrTracker2RuntimeStatus* status);
std::int32_t APS5_VABI VrTracker2GetTrackingFlags(bool* active, std::uint8_t* flags);
std::int32_t APS5_VABI VrTracker2GetDiagnosticState(VrTracker2DiagnosticState* state);
std::int32_t APS5_VABI VrTracker2GetExtendedDiagnosticState(VrTracker2ExtendedDiagnosticState* state);
std::int32_t APS5_VABI VrTracker2GetSensorDiagnostics(VrTracker2SensorDiagnostics* diagnostics);
std::int32_t APS5_VABI VrTracker2GetDeviceDiagnostics(VrTracker2DeviceDiagnostics* diagnostics);
std::int32_t APS5_VABI VrTracker2GetTimingDiagnostics(VrTracker2TimingDiagnostics* diagnostics);
std::int32_t APS5_VABI VrTracker2GetMapConfiguration(VrTracker2MapConfiguration* configuration);
std::int32_t APS5_VABI VrTracker2SetMapConfiguration(const VrTracker2MapConfiguration* configuration);
std::int32_t APS5_VABI VrTracker2GetMapStatus(VrTracker2MapStatus* status);
std::int32_t APS5_VABI VrTracker2BuildMesh(VrTracker2MeshVertex* vertices, std::uint16_t* indices, std::uint32_t format);
std::int32_t APS5_VABI VrTracker2GetMapScalar(float* value, const VrTracker2MapData* data);
std::int32_t APS5_VABI VrTracker2GetTimedPose(VrTracker2TimedPoseResult* result, std::uint64_t timestamp, std::uint64_t userTag);
std::int32_t APS5_VABI VrTracker2GetMapData(VrTracker2MapData* data, std::uint32_t size);
std::int32_t APS5_VABI VrTracker2GetPoseScalar(float* value);
std::int32_t APS5_VABI VrTracker2SubmitImageFrame(const VrTracker2ImageFrame* frame);
std::int32_t APS5_VABI VrTracker2GetImagePose(VrTracker2ImagePoseResult* result);
std::int32_t APS5_VABI VrTracker2GetSensorSummary(VrTracker2SensorSummary* summary);
std::int32_t APS5_VABI VrTracker2NotInitialized();
std::int32_t APS5_VABI VrTracker2ClearWideResult(void* result);
std::int32_t APS5_VABI VrTracker2ClearNarrowResult(void* result);
std::int32_t APS5_VABI VrTracker2CheckSecondThenReportNotInitialized(const void* first, const void* second);
std::int32_t APS5_VABI VrTracker2Accept();
std::int32_t APS5_VABI VrTracker2FillDefaults(void* output);
std::int32_t APS5_VABI VrTracker2ClearWord(void* output);
std::int32_t APS5_VABI VrTracker2Unknown01(VrTracker2CalibrationResult* result, std::uint32_t capacity);
std::int32_t APS5_VABI sceVrTracker2GetRawCameraMeshSize(VrTracker2MeshRequirement* requirement, std::uint32_t format);
std::int32_t APS5_VABI VrTracker2Unknown03(std::uint32_t mode, void* output, std::size_t capacity, std::size_t* writtenSize);
std::int32_t APS5_VABI VrTracker2Unknown04(const VrTracker2ResultParameter* parameter, VrTracker2Result* result);
std::int32_t APS5_VABI VrTracker2Unknown05(VrTracker2Pose* pose, std::uint64_t coordinateId);
std::int32_t APS5_VABI VrTracker2Unknown06(const VrTracker2TelemetryData* data);
std::int32_t APS5_VABI VrTracker2Unknown07(const VrTracker2PoseParameter* parameter, VrTracker2PoseResult* result);

}
