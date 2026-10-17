#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include <stdexcept>

extern "C" {

int APS5_VABI sceVideoOutSetFlipMaster() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutClearFlipMaster() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutAdjustColor_(int handle, const VideoOutColorSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSettings)) throw std::runtime_error("sceVideoOutAdjustColor_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutAdjustColor(handle, settings);
}

int APS5_VABI sceVideoOutColorSettingsSetGamma_(VideoOutColorSettings* settings, float gamma, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSettings)) throw std::runtime_error("sceVideoOutColorSettingsSetGamma_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSettingsSetGamma(settings, gamma);
}

}

extern "C" {

int APS5_VABI sceVideoOutCursorDisable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorEnable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetHotSpot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetImageAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetPosition() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
