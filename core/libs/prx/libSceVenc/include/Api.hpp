#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVencCreateEncoder(const void* parameters, const void* memory, void** encoder);
int APS5_VABI sceVencDeleteEncoder(void* encoder);
int APS5_VABI sceVencGetConfig(void* encoder, void* config);
int APS5_VABI sceVencMapMemory(void* encoder, const void* region);
int APS5_VABI sceVencQueryHeader(const void* parameters, void* header);
int APS5_VABI sceVencQueryMemorySize(const void* parameters, void* memory);
int APS5_VABI sceVencQueryPreset(const void* preset, void* parameters, void* startParameters);
int APS5_VABI sceVencSetConfig(void* encoder, const void* config);
int APS5_VABI sceVencSetDecodedFrame(void* encoder, const void* frame);
int APS5_VABI sceVencSetPictureConfig(void* encoder, const void* config);
int APS5_VABI sceVencSetRateControlConfig(void* encoder, const void* config);
int APS5_VABI sceVencSetReferenceFrameInvalidationConfig(void* encoder, const void* config);
int APS5_VABI sceVencStartEncode(void* encoder, const void* startParameters);
int APS5_VABI sceVencStopEncode(void* encoder);

int APS5_VABI sceVencGetHeader(const void* parameters, const void* startParameters, const void* config,
    const void* memory, void* header);
int APS5_VABI sceVencSetNormalPrivacyProtectionConfig(void* encoder, const void* config);
int APS5_VABI sceVencSetImagePasteConfig(void* encoder, const void* config);
int APS5_VABI sceVencQueryPreset2(const void* preset, void* parameters, void* startParameters, void* config);
int APS5_VABI sceVencSetCodecConfig(void* encoder, const void* config);

}
