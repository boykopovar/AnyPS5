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
    return 0;
}
