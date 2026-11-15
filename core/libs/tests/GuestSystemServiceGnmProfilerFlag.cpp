#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(!sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag());
    Require(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(1) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag());
    Require(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(0) == SYSTEM_SERVICE_OK);
    Require(!sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag());
    for (const std::int32_t enabled : {2, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        Require(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(enabled) == SYSTEM_SERVICE_OK);
        Require(sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag());
        Require(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(0) == SYSTEM_SERVICE_OK);
        Require(!sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag());
    }
    Require(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(1) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(1) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag());
}
