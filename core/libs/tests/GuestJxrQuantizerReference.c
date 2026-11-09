#include "strcodec.h"
#include <stdint.h>

void JxrQuantizerReferenceRemap_nid_no_patch(void* quantizer, int32_t shift, int32_t scaled) {
    remapQP((CWMIQuantizer*)quantizer, shift, scaled);
}

void JxrQuantizerReferenceFormat_nid_no_patch(void** quantizers, uint8_t mode, size_t channels, size_t position,
                                              int32_t shiftedUv, int32_t scaled) {
    formatQuantizer((CWMIQuantizer**)quantizers, mode, channels, position, shiftedUv, scaled);
}

int32_t JxrQuantizerReferenceAcMode_nid_no_patch(void* macroblock, int32_t format) {
    return getACPredMode((CWMIMBInfo*)macroblock, (COLORFORMAT)format);
}

void JxrQuantizerReferenceZigzag_nid_no_patch(void* context) {
    InitZigzagScan((CCodingContext*)context);
}
