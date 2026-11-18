#include "prx/libSceMediaFrameworkUtil/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

int APS5_VABI sceMediaFwUtilUnregisterNotifySystemVolume() {
    return 0;
}

int APS5_VABI sceMediaFwUtilRegisterNotifySystemVolume() {
    return 0;
}

int APS5_VABI sceMediaFwUtilPreprocessToKillMediaApp() {
    return 0;
}

int APS5_VABI sceMediaFwUtilKillCustomMusicCoreSynchronously() {
    return 0;
}

int APS5_VABI sceMediaFwUtilGetSystemVolume() {
    NotImplemented_nid_no_patch(__func__);
}

}
