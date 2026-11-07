#include "strcodec.h"
#include "JXRGlue.h"
#include "prx/libSceJxr/GuestPixelInfo.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(GUID) == 16, "GUID size");
_Static_assert(sizeof(COLORFORMAT) == 4 && sizeof(BITDEPTH_BITS) == 4, "enum size");
_Static_assert(sizeof(U32) == 4 && sizeof(JxrPixelInfo) == 56, "pixel info layout");
_Static_assert(offsetof(JxrPixelInfo, flags) == 32 && offsetof(JxrPixelInfo, interpretation) == 40, "LP64 offsets");

int64_t JxrLookup_nid_no_patch(JxrPixelInfo* value, uint8_t mode) {
    PKPixelInfo native = {0};
    GUID guid;
    ERR result;
    if (mode > LOOKUP_BACKWARD_TIF) return PixelFormatLookup(NULL, mode);
    if (mode == LOOKUP_FORWARD) {
        memcpy(&guid, value->guid, sizeof(guid));
        native.pGUIDPixFmt = &guid;
    } else {
        native.grBit = (WMP_GRBIT)value->flags;
        native.uInterpretation = value->interpretation;
        native.uSamplePerPixel = value->samplesPerPixel;
        native.uBitsPerSample = value->bitsPerSample;
        native.uSampleFormat = value->sampleFormat;
    }
    result = PixelFormatLookup(&native, mode);
    if (result != WMP_errSuccess) return result;
    value->guid = native.pGUIDPixFmt;
    value->channels = native.cChannel;
    value->colorFormat = native.cfColorFormat;
    value->bitDepth = native.bdBitDepth;
    value->bitsPerUnit = native.cbitUnit;
    value->flags = native.grBit;
    value->interpretation = native.uInterpretation;
    value->samplesPerPixel = native.uSamplePerPixel;
    value->bitsPerSample = native.uBitsPerSample;
    value->sampleFormat = native.uSampleFormat;
    return result;
}
