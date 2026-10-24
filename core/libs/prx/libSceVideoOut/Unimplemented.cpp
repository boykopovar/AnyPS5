#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include <stdexcept>

extern "C" {

int APS5_VABI sceVideoOutSetFlipMaster(std::int32_t handle, std::int32_t masterHandle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutClearFlipMaster(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorEnable(int32_t, int32_t, const void*) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorDisable(int32_t, int32_t) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetImageAddress(int32_t, int32_t, const void*) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetPosition(int32_t, int32_t, uint32_t, uint32_t) {
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

int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(VideoOutColorSpaceConversionSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSpaceConversionSettings)) throw std::runtime_error("sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(settings);
}

int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(VideoOutColorSpaceConversionSettings* settings, float lumaScale, uint32_t option, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSpaceConversionSettings)) throw std::runtime_error("sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(settings, lumaScale, option);
}

int APS5_VABI sceVideoOutAdjustColorSpaceConversion_(int handle, const VideoOutColorSpaceConversionSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSpaceConversionSettings)) throw std::runtime_error("sceVideoOutAdjustColorSpaceConversion_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutAdjustColorSpaceConversion(handle, settings);
}


int APS5_VABI sceDbgVideoOutAddOutputModeVr60Privilege() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
