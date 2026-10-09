#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include <stdexcept>

extern "C" {

int APS5_VABI sceVideoOutAdjustColor_(int handle, const VideoOutColorSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSettings)) throw std::runtime_error("sceVideoOutAdjustColor_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutAdjustColor(handle, settings);
}

int APS5_VABI sceVideoOutColorSettingsSetGamma_(VideoOutColorSettings* settings, float gamma, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSettings)) throw std::runtime_error("sceVideoOutColorSettingsSetGamma_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSettingsSetGamma(settings, gamma);
}

int APS5_VABI sceVideoOutRegisterBuffers(int handle, int startIndex, void* const* addresses, int bufferNum, const void* attribute) {
    (void)handle;
    (void)startIndex;
    (void)addresses;
    (void)bufferNum;
    (void)attribute;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceVideoOutSetBufferAttribute(void* attribute, uint32_t pixelFormat, uint32_t tilingMode, uint32_t aspectRatio, uint32_t width, uint32_t height, uint32_t pitchInPixel) {
    (void)attribute;
    (void)pixelFormat;
    (void)tilingMode;
    (void)aspectRatio;
    (void)width;
    (void)height;
    (void)pitchInPixel;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoOutGetBufferLabelAddress(int handle, uintptr_t* labelAddress) {
    (void)handle;
    (void)labelAddress;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceVideoOutModeSetAny_(void* mode, uint32_t modeSize) {
    (void)mode;
    (void)modeSize;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoOutConfigureOutputMode_(int handle, uint32_t reserved, const void* mode, const void* options, uint32_t modeSize, uint32_t optionsSize) {
    (void)handle;
    (void)reserved;
    (void)mode;
    (void)options;
    (void)modeSize;
    (void)optionsSize;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutSubmitChangeBufferAttribute(int handle, int attributeIndex, const void* attribute) {
    (void)handle;
    (void)attributeIndex;
    (void)attribute;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
