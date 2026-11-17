#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(!sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomLock() == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomLock() == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomUnlock() == SYSTEM_SERVICE_OK);
    Require(!sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    Require(sceShellCoreUtilAccessibilityZoomUnlock() == SYSTEM_SERVICE_OK);
    Require(!sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
    const std::int32_t others[] = {SYSTEM_SERVICE_RUNNING_APP_ID + 1, SYSTEM_SERVICE_RUNNING_APP_ID - 1, 0, -1,
                                   std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
    for (const std::int32_t appId : others) {
        Require(Throws([&] { static_cast<void>(sceShellCoreUtilIsAccessibilityZoomLocked(appId)); }));
    }
    Require(sceShellCoreUtilAccessibilityZoomLock() == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilIsAccessibilityZoomLocked(SYSTEM_SERVICE_RUNNING_APP_ID));
}
