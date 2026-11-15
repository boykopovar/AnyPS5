#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(CWMImageStrCodec) == 35944 && sizeof(CCodingContext) == 704, "codec and coding context sizes");
_Static_assert(offsetof(CWMImageStrCodec, WMII) == 8 && offsetof(CWMImageStrCodec, WMISCP) == 128 &&
                   offsetof(CWMImageStrCodec, WMIBI) == 33008 && offsetof(CWMImageStrCodec, m_param) == 34208 &&
                   offsetof(CWMImageStrCodec, cTileRow) == 34416 && offsetof(CWMImageStrCodec, m_bCtxLeft) == 34432 &&
                   offsetof(CWMImageStrCodec, cmbWidth) == 34528 && offsetof(CWMImageStrCodec, pPlane) == 34736 &&
                   offsetof(CWMImageStrCodec, a0MBbuffer) == 34864 && offsetof(CWMImageStrCodec, PredInfo) == 35392 &&
                   offsetof(CWMImageStrCodec, m_pNextSC) == 35672 && offsetof(CWMImageStrCodec, m_bSecondary) == 35680,
               "CWMImageStrCodec layout");

size_t JxrCodecBytes_nid_no_patch(void) {
    return sizeof(CWMImageStrCodec);
}

size_t JxrCodingContextBytes_nid_no_patch(void) {
    return sizeof(CCodingContext);
}

void* JxrNextCodec_nid_no_patch(void* value) {
    return ((CWMImageStrCodec*)value)->m_pNextSC;
}

static const char* CheckChannels(const CWMImageStrCodec* codec) {
    size_t j;
    for (j = 0; codec != NULL && j < 2; ++j, codec = codec->m_pNextSC)
        if (codec->m_param.cNumChannels > MAX_CHANNELS) return "more than 16 channels";
    return NULL;
}

const char* JxrCheckImageBuffer_nid_no_patch(void* value, size_t width, size_t rows, int32_t* result) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    if (codec == NULL) return "null codec";
    *result = checkImageBuffer(codec, width, rows);
    return NULL;
}

const char* JxrGetTilePos_nid_no_patch(void* value, size_t column, size_t row) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    if (codec == NULL) return "null codec";
    if (codec->WMISCP.cNumOfSliceMinus1V >= MAX_TILES) return "more than 4096 tile columns";
    if (codec->WMISCP.cNumOfSliceMinus1H >= MAX_TILES) return "more than 4096 tile rows";
    if (column != 0 && codec->cTileColumn > codec->WMISCP.cNumOfSliceMinus1V) return "current tile column out of range";
    if (row != 0 && codec->cTileRow > codec->WMISCP.cNumOfSliceMinus1H) return "current tile row out of range";
    getTilePos(codec, column, row);
    return NULL;
}

const char* JxrInitMRPtr_nid_no_patch(void* value) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    if (codec == NULL) return "null codec";
    initMRPtr(codec);
    return NULL;
}

const char* JxrAdvanceMRPtr_nid_no_patch(void* value) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    const CWMImageStrCodec* current;
    const char* error;
    size_t j, channel;
    if (codec == NULL) return "null codec";
    if (codec->m_bSecondary) return "secondary codec";
    if ((int)codec->m_param.cfColorFormat < (int)Y_ONLY || (int)codec->m_param.cfColorFormat >= (int)CFT_MAX)
        return "unknown color format";
    error = CheckChannels(codec);
    if (error) return error;
    for (j = 0, current = codec; current != NULL && j < 2; ++j, current = current->m_pNextSC)
        for (channel = 0; channel < current->m_param.cNumChannels; ++channel)
            if (current->p0MBbuffer[channel] == NULL || current->p1MBbuffer[channel] == NULL)
                return "null macroblock row buffer";
    advanceMRPtr(codec);
    return NULL;
}

const char* JxrSwapMRPtr_nid_no_patch(void* value) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    if (codec == NULL) return "null codec";
    swapMRPtr(codec);
    return NULL;
}

const char* JxrAdvanceOneMBRow_nid_no_patch(void* value) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    const char* error;
    if (codec == NULL) return "null codec";
    error = CheckChannels(codec);
    if (error) return error;
    advanceOneMBRow(codec);
    return NULL;
}

const char* JxrResetCodingContext_nid_no_patch(void* value) {
    if (value == NULL) return "null coding context";
    ResetCodingContext((CCodingContext*)value);
    return NULL;
}

int32_t JxrAllocateCodingContextDec_nid_no_patch(void* codec, int32_t count) {
    return AllocateCodingContextDec((CWMImageStrCodec*)codec, count);
}
