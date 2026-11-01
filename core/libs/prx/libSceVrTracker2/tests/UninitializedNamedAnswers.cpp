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
    return 0;
}
