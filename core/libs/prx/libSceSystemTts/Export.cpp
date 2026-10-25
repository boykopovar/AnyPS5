#include "prx/libSceSystemTts/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSystemTtsCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemTtsIsAccessibilityAvailable() {
    NotImplemented_nid_no_patch(__func__);
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
