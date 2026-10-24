#ifndef CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_OUTPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_OUTPUT_HPP

#include <cstdint>
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

int ValidateVideoOutOutputMode_nid_no_patch(uint64_t mode, const VideoOutOutputOptions* options, int displayRefreshRate);

extern "C" {

int APS5_VABI sceVideoOutOpen(int user_id, int bus_type, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved);
int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved);
int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options);
int APS5_VABI sceVideoOutAllowOutputResolutionWqhdDetection(int handle);
int APS5_VABI sceVideoOutSetFlipRate(int handle, int rate);
int APS5_VABI sceVideoOutSetWindowModeMargins(int handle, int top, int bottom);
int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status);
int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status);
int APS5_VABI sceVideoOutGetOutputStatusWithVrr(int handle, void* status);
int APS5_VABI sceVideoOutGetResolutionStatus(int handle, VideoOutResolutionStatus* status);
int APS5_VABI sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status);
int APS5_VABI sceVideoOutIsFlipPending(int handle);
int APS5_VABI sceVideoOutWaitVblank(int handle);
int APS5_VABI sceVideoOutLatencyControlWaitBeforeInput(int handle);
int APS5_VABI sceVideoOutLatencyMeasureSetStartPoint(int handle, uint32_t point);
int APS5_VABI sceVideoOutColorSettingsSetGamma(VideoOutColorSettings* settings, float gamma);
int APS5_VABI sceVideoOutAdjustColor(int handle, const VideoOutColorSettings* settings);
int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(VideoOutColorSpaceConversionSettings* settings);
int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(VideoOutColorSpaceConversionSettings* settings, float lumaScale, uint32_t option);
int APS5_VABI sceVideoOutAdjustColorSpaceConversion(int handle, const VideoOutColorSpaceConversionSettings* settings);
int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(VideoOutColorSpaceConversionSettings* settings, uint32_t settings_size);
int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(VideoOutColorSpaceConversionSettings* settings, float lumaScale, uint32_t option, uint32_t settings_size);
int APS5_VABI sceVideoOutAdjustColorSpaceConversion_(int handle, const VideoOutColorSpaceConversionSettings* settings, uint32_t settings_size);
int APS5_VABI sceVideoOutSetGlobalBlendSpace(int handle, uint32_t space, uint32_t option);
int APS5_VABI sceVideoOutVrrPegToFixedRate(int handle, uint64_t arg1, uint64_t arg2);
int APS5_VABI sceVideoOutVrrUnpegFromFixedRate(int handle);

}

#endif
