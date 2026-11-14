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
    return 0;
}
