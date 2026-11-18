#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSystemTts/include/InventoryExports.hpp"
#include "prx/libSceSystemTts/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSystemTtsCancel() {
    return 0;
}

int APS5_VABI sceSystemTtsIsAccessibilityAvailable() {
    return 0;
}

int APS5_VABI sceSystemTtsRegisterCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemTtsSpeakRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemTtsUnregisterCallback() {
    return 0;
}

int APS5_VABI sceSystemTtsConvertText() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsPauseResume() {
    return 0;
}

int APS5_VABI sceSystemTtsRepeat() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemTtsWait() {
    return 0;
}

}

extern "C" {

int APS5_VABI libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aIz2UUh6oi0", libSceSystemTtsUnknown_aIz2UUh6oi0_nid_no_patch);

}
