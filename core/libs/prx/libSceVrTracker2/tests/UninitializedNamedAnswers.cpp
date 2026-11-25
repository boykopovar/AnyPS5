#include "prx/libSceVrTracker2/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::int32_t NotInitialized = static_cast<std::int32_t>(0x8A740001u);

constexpr std::uint8_t Sentinel = 0x5a;
constexpr std::size_t ProbeSize = 16 * 1024;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

struct Probe {
    unsigned char* bytes;
    Probe() : bytes(new unsigned char[ProbeSize]) {
        for (std::size_t index = 0; index < ProbeSize; ++index) bytes[index] = Sentinel;
    }
    ~Probe() { delete[] bytes; }
    bool Untouched() const {
        for (std::size_t index = 0; index < ProbeSize; ++index) {
            if (bytes[index] != Sentinel) return false;
        }
        return true;
    }
};

}

int main() {
    Probe output;
    Probe parameter;

    Require(sceVrTracker2GetCoordinateSystem(reinterpret_cast<std::uint32_t*>(output.bytes)) == NotInitialized,
            "get coordinate system before initialization");
    Require(output.Untouched(), "get coordinate system wrote nothing");

    Require(sceVrTracker2GetPlayAreaBoundaryGeometry(reinterpret_cast<VrTracker2BoundaryGeometry*>(output.bytes)) == NotInitialized,
            "get play area boundary geometry before initialization");
    Require(output.Untouched(), "get play area boundary geometry wrote nothing");

    Require(sceVrTracker2GetPlayAreaOrientedBoundingBox(reinterpret_cast<VrTracker2OrientedBoundingBox*>(output.bytes)) == NotInitialized,
            "get play area oriented bounding box before initialization");
    Require(output.Untouched(), "get play area oriented bounding box wrote nothing");

    Require(sceVrTracker2ActivateMap() == NotInitialized, "activate map before initialization");
    Require(sceVrTracker2InitializeMap(1, reinterpret_cast<const float*>(parameter.bytes)) == NotInitialized, "initialize map before initialization");
    Require(parameter.Untouched(), "initialize map wrote nothing to the parameter");
    Require(sceVrTracker2RefineMap() == NotInitialized, "refine map before initialization");
    Require(sceVrTracker2RemoveMap() == NotInitialized, "remove map before initialization");

    Require(sceVrTracker2CheckDeviceIsInsidePlayAreaBoundary(1, reinterpret_cast<VrTracker2SpatialQueryResult*>(output.bytes)) == NotInitialized,
            "check device inside boundary before initialization");
    Require(output.Untouched(), "check device inside boundary wrote nothing");
    Require(sceVrTracker2CheckPointIsInsidePlayAreaBoundary(reinterpret_cast<const float*>(parameter.bytes),
                                                            reinterpret_cast<VrTracker2SpatialQueryResult*>(output.bytes)) == NotInitialized,
            "check point inside boundary before initialization");
    Require(parameter.Untouched(), "check point inside boundary wrote nothing to the parameter");
    Require(output.Untouched(), "check point inside boundary wrote nothing to the result");

    Require(sceVrTracker2GetPlayAreaConfig(reinterpret_cast<VrTracker2ScalarResult*>(output.bytes)) == NotInitialized,
            "get play area configuration before initialization");
    Require(output.Untouched(), "get play area configuration wrote nothing");
    Require(sceVrTracker2SetPlayAreaConfig(reinterpret_cast<const VrTracker2ModeParameter*>(parameter.bytes)) == NotInitialized,
            "set play area configuration before initialization");
    Require(parameter.Untouched(), "set play area configuration wrote nothing to the parameter");
    Require(sceVrTracker2GetPlayAreaData(reinterpret_cast<VrTracker2BoundaryParameter*>(output.bytes)) == NotInitialized,
            "get play area data before initialization");
    Require(output.Untouched(), "get play area data wrote nothing");
    Require(sceVrTracker2SetPlayAreaData(reinterpret_cast<const VrTracker2BoundaryParameter*>(parameter.bytes)) == NotInitialized,
            "set play area data before initialization");
    Require(parameter.Untouched(), "set play area data wrote nothing to the parameter");
    Require(sceVrTracker2GetPlayAreaData2(1, 0, reinterpret_cast<VrTracker2DeviceBoundaryParameter*>(output.bytes)) == NotInitialized,
            "get device play area data before initialization");
    Require(output.Untouched(), "get device play area data wrote nothing");
    Require(sceVrTracker2SetPlayAreaData2(1, 0, reinterpret_cast<const VrTracker2DeviceBoundaryParameter*>(parameter.bytes)) == NotInitialized,
            "set device play area data before initialization");
    Require(parameter.Untouched(), "set device play area data wrote nothing to the parameter");

    Require(sceVrTracker2GetPlayAreaInformation(reinterpret_cast<VrTracker2RuntimeStatus*>(output.bytes)) == NotInitialized,
            "get play area information before initialization");
    Require(output.Untouched(), "get play area information wrote nothing");
    Require(sceVrTracker2GetPlayAreaResult(reinterpret_cast<VrTracker2TrackingStatus*>(output.bytes)) == NotInitialized,
            "get play area result before initialization");
    Require(output.Untouched(), "get play area result wrote nothing");
    Require(sceVrTracker2GetPlayAreaOrientedBoundingBoxEx(reinterpret_cast<VrTracker2OrientedBoundingBox*>(output.bytes)) == NotInitialized,
            "get extended play area bounding box before initialization");
    Require(output.Untouched(), "get extended play area bounding box wrote nothing");
    Require(sceVrTracker2GetCurrentPlayAreaId(1, reinterpret_cast<std::int32_t*>(output.bytes)) == NotInitialized,
            "get current play area id before initialization");
    Require(output.Untouched(), "get current play area id wrote nothing");
    Require(sceVrTracker2SetCurrentPlayAreaId(1, 0) == NotInitialized, "set current play area id before initialization");

    Require(sceVrTracker2GetResult(reinterpret_cast<const VrTracker2ResultParameter*>(parameter.bytes),
                                   reinterpret_cast<VrTracker2Result*>(output.bytes)) == NotInitialized,
            "get result before initialization");
    Require(parameter.Untouched(), "get result wrote nothing to the parameter");
    Require(output.Untouched(), "get result wrote nothing to the result");

    Require(sceVrTracker2LocateCoordinateSystem(1, reinterpret_cast<VrTracker2CoordinateLocation*>(output.bytes)) == NotInitialized,
            "locate coordinate system before initialization");
    Require(output.Untouched(), "locate coordinate system wrote nothing");

    Require(sceVrTracker2RegisterDevice(1, 2) == NotInitialized, "register device before initialization");
    Require(sceVrTracker2UnregisterDevice(2) == NotInitialized, "unregister device before initialization");
    Require(sceVrTracker2SetCoordinateSystem(1) == NotInitialized, "set coordinate system before initialization");
    Require(sceVrTracker2ResetLocalCoordinate(2) == NotInitialized, "reset local coordinate before initialization");
    Require(sceVrTracker2GetRawCameraMeshSize(reinterpret_cast<VrTracker2MeshRequirement*>(output.bytes), 0) == NotInitialized,
            "get raw camera mesh size before initialization");
    Require(output.Untouched(), "get raw camera mesh size wrote nothing");

    Require(sceVrTracker2GetControllerConfig(reinterpret_cast<VrTracker2TrackingConfiguration*>(output.bytes)) == NotInitialized,
            "get controller configuration before initialization");
    Require(output.Untouched(), "get controller configuration wrote nothing");
    Require(sceVrTracker2SetControllerConfig(reinterpret_cast<const VrTracker2TrackingConfiguration*>(parameter.bytes)) == NotInitialized,
            "set controller configuration before initialization");
    Require(parameter.Untouched(), "set controller configuration wrote nothing to the parameter");
    Require(sceVrTracker2GetControllerImage(reinterpret_cast<VrTracker2CameraImages*>(output.bytes)) == NotInitialized,
            "get controller image before initialization");
    Require(output.Untouched(), "get controller image wrote nothing");
    Require(sceVrTracker2GetControllerTraceResult() == NotInitialized, "get controller trace result before initialization");

    Require(sceVrTracker2GetSlamConfig(reinterpret_cast<VrTracker2TrackingOptionsResult*>(output.bytes)) == NotInitialized,
            "get SLAM configuration before initialization");
    Require(output.Untouched(), "get SLAM configuration wrote nothing");
    Require(sceVrTracker2SetSlamConfig(reinterpret_cast<const VrTracker2TrackingOptions*>(parameter.bytes)) == NotInitialized,
            "set SLAM configuration before initialization");
    Require(parameter.Untouched(), "set SLAM configuration wrote nothing to the parameter");
    Require(sceVrTracker2GetSlamResult(reinterpret_cast<const VrTracker2ResultParameter*>(parameter.bytes),
                                       reinterpret_cast<VrTracker2CompositePoseResult*>(output.bytes)) == NotInitialized,
            "get SLAM result before initialization");
    Require(parameter.Untouched(), "get SLAM result wrote nothing to the parameter");
    Require(output.Untouched(), "get SLAM result wrote nothing to the result");

    Require(sceVrTracker2GetRelocalizerConfig(reinterpret_cast<const VrTracker2LegacyParameter20*>(parameter.bytes)) == NotInitialized,
            "get relocalizer configuration before initialization");
    Require(parameter.Untouched(), "get relocalizer configuration wrote nothing to the parameter");
    Require(sceVrTracker2SetRelocalizerConfig(reinterpret_cast<const VrTracker2LegacyParameter12*>(parameter.bytes)) == NotInitialized,
            "set relocalizer configuration before initialization");
    Require(parameter.Untouched(), "set relocalizer configuration wrote nothing to the parameter");

    Require(sceVrTracker2GetCoordinateSystemEx(reinterpret_cast<std::uint32_t*>(output.bytes)) == NotInitialized,
            "get extended coordinate system before initialization");
    Require(output.Untouched(), "get extended coordinate system wrote nothing");
    Require(sceVrTracker2SetCoordinateSystemEx(1) == NotInitialized, "set extended coordinate system before initialization");
    Require(sceVrTracker2LocateCoordinateSystemEx(1, 2, reinterpret_cast<VrTracker2RelativePoseResult*>(output.bytes)) == NotInitialized,
            "locate extended coordinate system before initialization");
    Require(output.Untouched(), "locate extended coordinate system wrote nothing");
    Require(sceVrTracker2ResetLocalCoordinateEx(1) == NotInitialized, "reset extended local coordinate before initialization");
    Require(sceVrTracker2ResetLocalCoordinateWithPose(reinterpret_cast<const sceVrTracker2ResetLocalCoordinateWithPoseParameter*>(parameter.bytes)) == NotInitialized,
            "reset local coordinate with pose before initialization");
    Require(parameter.Untouched(), "reset local coordinate with pose wrote nothing to the parameter");

    Require(sceVrTracker2GetHeadPosFromEye(1, reinterpret_cast<VrTracker2VectorResult*>(output.bytes)) == NotInitialized,
            "get head position from eye before initialization");
    Require(output.Untouched(), "get head position from eye wrote nothing");
    Require(sceVrTracker2GetNearObjectDepth(reinterpret_cast<float*>(output.bytes)) == NotInitialized, "get near object depth before initialization");
    Require(output.Untouched(), "get near object depth wrote nothing");
    Require(sceVrTracker2GetIrTrackerResult(reinterpret_cast<VrTracker2SensorFrame*>(output.bytes)) == NotInitialized,
            "get IR tracker result before initialization");
    Require(output.Untouched(), "get IR tracker result wrote nothing");
    Require(sceVrTracker2IrGetDebugRawImage(output.bytes, 64, reinterpret_cast<std::uint32_t*>(output.bytes + 4096)) == NotInitialized,
            "get IR debug raw image before initialization");
    Require(output.Untouched(), "get IR debug raw image wrote nothing");
    Require(sceVrTracker2GetRawCameraMesh(reinterpret_cast<VrTracker2MeshVertex*>(output.bytes), reinterpret_cast<std::uint16_t*>(output.bytes + 4096), 0) == NotInitialized,
            "get raw camera mesh before initialization");
    Require(output.Untouched(), "get raw camera mesh wrote nothing");
    Require(sceVrTracker2GetResultForPlayArea(reinterpret_cast<const VrTracker2ResultParameter*>(parameter.bytes),
                                              reinterpret_cast<VrTracker2ExtendedTrackingResult*>(output.bytes)) == NotInitialized,
            "get play area result for parameter before initialization");
    Require(parameter.Untouched(), "get play area result for parameter wrote nothing to the parameter");
    Require(output.Untouched(), "get play area result for parameter wrote nothing to the result");
    Require(sceVrTracker2GetResultForPlayAreaEx(reinterpret_cast<const VrTracker2CoordinateResultParameter*>(parameter.bytes),
                                                reinterpret_cast<VrTracker2ExtendedTrackingResult*>(output.bytes)) == NotInitialized,
            "get extended play area result before initialization");
    Require(parameter.Untouched(), "get extended play area result wrote nothing to the parameter");
    Require(output.Untouched(), "get extended play area result wrote nothing to the result");
    Require(VrTracker2GetCounter(reinterpret_cast<std::uint64_t*>(output.bytes), 0) == NotInitialized,
            "get counter before initialization");
    Require(output.Untouched(), "get counter wrote nothing");
    Require(VrTracker2GetDiagnosticResult(reinterpret_cast<VrTracker2DiagnosticResult*>(output.bytes)) == NotInitialized,
            "get diagnostic result before initialization");
    Require(output.Untouched(), "get diagnostic result wrote nothing");
    Require(VrTracker2GetExtendedResult(reinterpret_cast<VrTracker2ExtendedResult*>(output.bytes)) == NotInitialized,
            "get extended result before initialization");
    Require(output.Untouched(), "get extended result wrote nothing");
    Require(VrTracker2GetDiagnosticText(reinterpret_cast<VrTracker2DiagnosticText*>(output.bytes)) == NotInitialized,
            "get diagnostic text before initialization");
    Require(output.Untouched(), "get diagnostic text wrote nothing");
    Require(VrTracker2GetDiagnosticState(reinterpret_cast<VrTracker2DiagnosticState*>(output.bytes)) == NotInitialized,
            "get diagnostic state before initialization");
    Require(output.Untouched(), "get diagnostic state wrote nothing");
    Require(VrTracker2GetExtendedDiagnosticState(reinterpret_cast<VrTracker2ExtendedDiagnosticState*>(output.bytes)) == NotInitialized,
            "get extended diagnostic state before initialization");
    Require(output.Untouched(), "get extended diagnostic state wrote nothing");
    Require(VrTracker2GetSensorDiagnostics(reinterpret_cast<VrTracker2SensorDiagnostics*>(output.bytes)) == NotInitialized,
            "get sensor diagnostics before initialization");
    Require(output.Untouched(), "get sensor diagnostics wrote nothing");
    Require(VrTracker2GetDeviceDiagnostics(reinterpret_cast<VrTracker2DeviceDiagnostics*>(output.bytes)) == NotInitialized,
            "get device diagnostics before initialization");
    Require(output.Untouched(), "get device diagnostics wrote nothing");
    Require(VrTracker2GetTimingDiagnostics(reinterpret_cast<VrTracker2TimingDiagnostics*>(output.bytes)) == NotInitialized,
            "get timing diagnostics before initialization");
    Require(output.Untouched(), "get timing diagnostics wrote nothing");
    Require(VrTracker2GetSensorSummary(reinterpret_cast<VrTracker2SensorSummary*>(output.bytes)) == NotInitialized,
            "get sensor summary before initialization");
    Require(output.Untouched(), "get sensor summary wrote nothing");
    Require(VrTracker2GetTrackingFlags(reinterpret_cast<bool*>(output.bytes), reinterpret_cast<std::uint8_t*>(output.bytes + 8)) == NotInitialized,
            "get tracking flags before initialization");
    Require(output.Untouched(), "get tracking flags wrote nothing");
    Require(VrTracker2GetDeviceState(1, reinterpret_cast<std::int32_t*>(output.bytes)) == NotInitialized,
            "get device state before initialization");
    Require(output.Untouched(), "get device state wrote nothing");
    Require(VrTracker2GetDeviceCalibrationStatus(1, reinterpret_cast<VrTracker2DeviceCalibrationStatus*>(output.bytes)) == NotInitialized,
            "get device calibration status before initialization");
    Require(output.Untouched(), "get device calibration status wrote nothing");
    Require(VrTracker2AcquireCameraFrame(0, reinterpret_cast<VrTracker2CameraFrame*>(output.bytes)) == NotInitialized,
            "acquire camera frame before initialization");
    Require(output.Untouched(), "acquire camera frame wrote nothing");
    Require(VrTracker2AcquireCameraFrameWithMetadata(0, reinterpret_cast<VrTracker2CameraFrame*>(output.bytes), reinterpret_cast<VrTracker2CameraFrameMetadata*>(output.bytes + 4096)) == NotInitialized,
            "acquire camera frame with metadata before initialization");
    Require(output.Untouched(), "acquire camera frame with metadata wrote nothing");
    Require(VrTracker2ReleaseCameraFrame(reinterpret_cast<const VrTracker2CameraFrame*>(parameter.bytes)) == NotInitialized,
            "release camera frame before initialization");
    Require(parameter.Untouched(), "release camera frame wrote nothing to the frame");
    Require(VrTracker2SetScalarOption(1.0f) == NotInitialized,
            "set scalar option before initialization");
    Require(VrTracker2GetScalarOption(reinterpret_cast<float*>(output.bytes)) == NotInitialized,
            "get scalar option before initialization");
    Require(output.Untouched(), "get scalar option wrote nothing");
    Require(VrTracker2FitBoundary(reinterpret_cast<const VrTracker2BoundaryGeometry*>(parameter.bytes), reinterpret_cast<VrTracker2BoundaryFitResult*>(output.bytes)) == NotInitialized,
            "fit boundary before initialization");
    Require(parameter.Untouched(), "fit boundary wrote nothing to the geometry");
    Require(output.Untouched(), "fit boundary wrote nothing");
    Require(VrTracker2FitBoundaryWithBounds(reinterpret_cast<const VrTracker2BoundaryGeometry*>(parameter.bytes), reinterpret_cast<VrTracker2OrientedBoundingBox*>(output.bytes), reinterpret_cast<VrTracker2OrientedBoundingBox*>(output.bytes + 4096), reinterpret_cast<VrTracker2BoundaryFitResult*>(output.bytes + 8192)) == NotInitialized,
            "fit boundary with bounds before initialization");
    Require(output.Untouched(), "fit boundary with bounds wrote nothing");
    Require(VrTracker2UnsupportedCoordinateOperation(reinterpret_cast<const VrTracker2UnsupportedCoordinateParameter*>(parameter.bytes)) == NotInitialized,
            "unsupported coordinate operation before initialization");
    Require(parameter.Untouched(), "unsupported coordinate operation wrote nothing to the parameter");
    Require(VrTracker2GetMapConfiguration(reinterpret_cast<VrTracker2MapConfiguration*>(output.bytes)) == NotInitialized,
            "get map configuration before initialization");
    Require(output.Untouched(), "get map configuration wrote nothing");
    Require(VrTracker2SetMapConfiguration(reinterpret_cast<const VrTracker2MapConfiguration*>(parameter.bytes)) == NotInitialized,
            "set map configuration before initialization");
    Require(parameter.Untouched(), "set map configuration wrote nothing to the configuration");
    Require(VrTracker2GetMapStatus(reinterpret_cast<VrTracker2MapStatus*>(output.bytes)) == NotInitialized,
            "get map status before initialization");
    Require(output.Untouched(), "get map status wrote nothing");
    Require(VrTracker2GetMapScalar(reinterpret_cast<float*>(output.bytes), reinterpret_cast<const VrTracker2MapData*>(parameter.bytes)) == NotInitialized,
            "get map scalar before initialization");
    Require(parameter.Untouched(), "get map scalar wrote nothing to the map data");
    Require(output.Untouched(), "get map scalar wrote nothing");
    Require(VrTracker2GetTimedPose(reinterpret_cast<VrTracker2TimedPoseResult*>(output.bytes), 1, 2) == NotInitialized,
            "get timed pose before initialization");
    Require(output.Untouched(), "get timed pose wrote nothing");
    Require(VrTracker2GetMapData(reinterpret_cast<VrTracker2MapData*>(output.bytes), 32) == NotInitialized,
            "get map data before initialization");
    Require(output.Untouched(), "get map data wrote nothing");
    Require(SceVrTracker2DepthCubeMapUpdate(reinterpret_cast<const VrTracker2ImageFrame*>(parameter.bytes)) == NotInitialized,
            "depth cube map update before initialization");
    Require(parameter.Untouched(), "depth cube map update wrote nothing to the frame");
    Require(SceVrTracker2DepthCubeMapGetData(reinterpret_cast<VrTracker2ImagePoseResult*>(output.bytes)) == NotInitialized,
            "depth cube map get data before initialization");
    Require(output.Untouched(), "depth cube map get data wrote nothing");
    return 0;
}
