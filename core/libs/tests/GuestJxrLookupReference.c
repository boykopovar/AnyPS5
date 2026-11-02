#include "strcodec.h"
#include "JXRGlue.h"
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(GUID) == 16, "GUID size");
_Static_assert(sizeof(U32) == 4, "U32 size");

int JxrReferenceCase_nid_no_patch(uint8_t hash, int reverse, int white, uint8_t* guid, uint64_t* fields) {
    PKPixelInfo native = {0};
    native.pGUIDPixFmt = GetPixelFormatFromHash(hash);
    if (!native.pGUIDPixFmt) return 0;
    if (PixelFormatLookup(&native, LOOKUP_FORWARD) != 0) return -1;
    if (reverse) {
        native.pGUIDPixFmt = NULL;
        native.grBit |= PK_pixfmtBGR | PK_pixfmtNeedConvert;
        if (white) native.uInterpretation = 0;
        if (PixelFormatLookup(&native, LOOKUP_BACKWARD_TIF) != 0) return -1;
    }
    memcpy(guid, native.pGUIDPixFmt, 16);
    fields[0] = native.cChannel;
    fields[1] = native.cfColorFormat;
    fields[2] = native.bdBitDepth;
    fields[3] = native.cbitUnit;
    fields[4] = native.grBit;
    fields[5] = native.uInterpretation;
    fields[6] = native.uSamplePerPixel;
    fields[7] = native.uBitsPerSample;
    fields[8] = native.uSampleFormat;
    return 1;
}
