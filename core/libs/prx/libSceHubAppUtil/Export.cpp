#include "prx/libSceHubAppUtil/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<int> g_installStatus{0};
std::atomic<bool> g_deeplinkActive{false};

}

extern "C" {

int APS5_VABI sceHubAppUtilDeeplinkToLiveDetailView(void) {
    g_deeplinkActive.store(true);
    return 0;
}

int APS5_VABI sceHubAppUtilDeeplinkToPSSubscriptions(void) {
    g_deeplinkActive.store(true);
    return 0;
}

int APS5_VABI sceHubAppUtilGetInstallationStatus(void) {
    return g_installStatus.load();
}

}
