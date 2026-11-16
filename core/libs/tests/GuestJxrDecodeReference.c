#include "JXRGlue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t JxrDecodeFixtureSize_nid_no_patch(void) {
    return sizeof(PKImageDecode);
}

size_t JxrDecodeFixtureGetPosOffset_nid_no_patch(void) {
    return offsetof(struct WMPStream, GetPos);
}

size_t JxrDecodeFixtureCloseOffset_nid_no_patch(void) {
    return offsetof(struct WMPStream, Close);
}

void JxrDecodeFixtureSetStream_nid_no_patch(void* decoder, void* stream, int32_t owner) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    pID->pStream = (struct WMPStream*)stream;
    pID->fStreamOwner = owner;
}

void* JxrDecodeFixtureStream_nid_no_patch(void* decoder) {
    return ((PKImageDecode*)decoder)->pStream;
}

void* JxrDecodeFixtureStreamAddress_nid_no_patch(void* decoder) {
    return &((PKImageDecode*)decoder)->pStream;
}

size_t JxrDecodeFixtureStartOffset_nid_no_patch(void* decoder) {
    return ((PKImageDecode*)decoder)->offStart;
}

void JxrDecodeFixtureSetImage_nid_no_patch(void* decoder, uint32_t width, uint32_t height, uint32_t frames,
                                           float resolutionX, float resolutionY, const void* format) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    pID->uWidth = width;
    pID->uHeight = height;
    pID->cFrame = frames;
    pID->fResX = resolutionX;
    pID->fResY = resolutionY;
    memcpy(&pID->guidPixFormat, format, sizeof(pID->guidPixFormat));
}

void JxrDecodeFixtureGetImage_nid_no_patch(void* decoder, uint32_t* frames, float* resolutionX,
                                           float* resolutionY, void* format) {
    const PKImageDecode* pID = (const PKImageDecode*)decoder;
    *frames = pID->cFrame;
    *resolutionX = pID->fResX;
    *resolutionY = pID->fResY;
    memcpy(format, &pID->guidPixFormat, sizeof(pID->guidPixFormat));
}

uint8_t* JxrDecodeFixtureMisc_nid_no_patch(void* decoder) {
    return (uint8_t*)&((PKImageDecode*)decoder)->WMP.wmiDEMisc;
}

size_t JxrDecodeFixtureMiscSize_nid_no_patch(void) {
    return sizeof(((PKImageDecode*)0)->WMP.wmiDEMisc);
}
