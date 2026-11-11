#include "strcodec.h"
#include <stdint.h>

_Static_assert(sizeof(CWMIQuantizer) == 20, "CWMIQuantizer size");
_Static_assert(sizeof(CWMIMBInfo) == 1160, "CWMIMBInfo size");
_Static_assert(sizeof(CAdaptiveScan) == 8 && sizeof(CCodingContext) == 704, "CCodingContext size");

void JxrRemapQp_nid_no_patch(void* quantizer, int32_t shift, int32_t scaled) {
    remapQP((CWMIQuantizer*)quantizer, shift, scaled);
}

void JxrFormatQuantizer_nid_no_patch(void** quantizers, uint8_t mode, size_t channels, size_t position,
                                     int32_t shiftedUv, int32_t scaled) {
    formatQuantizer((CWMIQuantizer**)quantizers, mode, channels, position, shiftedUv, scaled);
}

int32_t JxrGetAcPredMode_nid_no_patch(void* macroblock, int32_t format) {
    return getACPredMode((CWMIMBInfo*)macroblock, (COLORFORMAT)format);
}

void JxrInitZigzagScan_nid_no_patch(void* context) {
    InitZigzagScan((CCodingContext*)context);
}
