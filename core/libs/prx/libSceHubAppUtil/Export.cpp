#include "prx/libSceHubAppUtil/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceHubAppUtilDeeplinkToLiveDetailView() {
    puts_nid_postfix("[PPRBUG-22821] sceHubAppUtilDeeplinkToLiveDetailView() is not available.");
    return static_cast<int>(0x818D0001u);
}

int APS5_VABI sceHubAppUtilDeeplinkToPSSubscriptions() {
    puts_nid_postfix("[PPRBUG-22821] sceHubAppUtilDeeplinkToPSSubscriptions() is not available.");
    return static_cast<int>(0x818D0001u);
}

int APS5_VABI sceHubAppUtilGetInstallationStatus() {
    puts_nid_postfix("[PPRBUG-22821] sceHubAppUtilGetInstallationStatus() is not available.");
    return static_cast<int>(0x818D0001u);
}

}
