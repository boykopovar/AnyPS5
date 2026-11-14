#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(CWMITile) == 432 && sizeof(CWMIPredInfo) == 48, "tile and prediction sizes");
_Static_assert(sizeof(CWMImageStrCodec) == 35944, "CWMImageStrCodec size");
_Static_assert(offsetof(CWMImageStrCodec, WMISCP) == 128 && offsetof(CWMImageStrCodec, MBInfo) == 33048 &&
                   offsetof(CWMImageStrCodec, m_param) == 34208 && offsetof(CWMImageStrCodec, m_bCtxLeft) == 34432 &&
                   offsetof(CWMImageStrCodec, pTile) == 34448 && offsetof(CWMImageStrCodec, cmbWidth) == 34528 &&
                   offsetof(CWMImageStrCodec, PredInfo) == 35392,
               "CWMImageStrCodec layout");

static const char* CheckTile(const CWMImageStrCodec* codec, size_t tile) {
    if (codec == NULL) return "null codec";
    if (codec->m_param.cNumChannels > MAX_CHANNELS) return "more than 16 channels";
    if (codec->WMISCP.cNumOfSliceMinus1V >= MAX_TILES) return "more than 4096 tile columns";
    if (tile > codec->WMISCP.cNumOfSliceMinus1V) return "tile column out of range";
    if (codec->pTile == NULL) return "null tile table";
    return NULL;
}

const char* JxrSetUniformQuantizer_nid_no_patch(void* value, size_t band) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    const char* error = CheckTile(codec, 0);
    if (error) return error;
    setUniformQuantizer(codec, band);
    return NULL;
}

const char* JxrUseDcQuantizer_nid_no_patch(void* value, size_t tile) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    const char* error = CheckTile(codec, tile);
    size_t channel;
    if (error) return error;
    for (channel = 0; channel < codec->m_param.cNumChannels; ++channel)
        if (codec->pTile[tile].pQuantizerDC[channel] == NULL || codec->pTile[tile].pQuantizerLP[channel] == NULL)
            return "null channel quantizer";
    useDCQuantizer(codec, tile);
    return NULL;
}

const char* JxrUseLpQuantizer_nid_no_patch(void* value, size_t count, size_t tile) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    const char* error = CheckTile(codec, tile);
    size_t channel;
    if (error) return error;
    if (count > 16) return "more than 16 quantizers";
    if (count == 0) return NULL;
    for (channel = 0; channel < codec->m_param.cNumChannels; ++channel)
        if (codec->pTile[tile].pQuantizerLP[channel] == NULL || codec->pTile[tile].pQuantizerHP[channel] == NULL)
            return "null channel quantizer";
    useLPQuantizer(codec, count, tile);
    return NULL;
}

static const char* CheckRows(const CWMImageStrCodec* codec, size_t channels, size_t column, int previous) {
    size_t channel;
    if (column >= codec->cmbWidth) return "macroblock column out of range";
    for (channel = 0; channel < channels; ++channel)
        if (codec->PredInfo[channel] == NULL || (previous && codec->PredInfoPrevRow[channel] == NULL))
            return "null prediction row";
    return NULL;
}

const char* JxrGetDcAcPredMode_nid_no_patch(void* value, size_t column, int32_t* mode) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    const char* error;
    if (codec == NULL) return "null codec";
    if (!codec->m_bCtxLeft && !codec->m_bCtxTop) {
        const COLORFORMAT format = codec->m_param.cfColorFormat;
        if (column == 0) return "no left macroblock";
        error = CheckRows(codec, format == Y_ONLY || format == NCOMPONENT ? 1 : 3, column, 1);
    } else if (codec->m_bCtxLeft && !codec->m_bCtxTop) {
        error = CheckRows(codec, 1, column, 1);
    } else if (!codec->m_bCtxLeft) {
        if (column == 0) return "no left macroblock";
        error = CheckRows(codec, 1, column, 0);
    } else {
        error = NULL;
    }
    if (error) return error;
    *mode = getDCACPredMode(codec, column);
    return NULL;
}

const char* JxrUpdatePredInfo_nid_no_patch(void* value, void* macroblock, size_t column, int32_t format) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    size_t channels, channel;
    const char* error;
    if (codec == NULL) return "null codec";
    if (macroblock == NULL) return "null macroblock";
    if (format < Y_ONLY || format >= CFT_MAX) return "unknown color format";
    if (codec->m_param.cNumChannels > MAX_CHANNELS) return "more than 16 channels";
    channels = format == YUV_420 || format == YUV_422 ? 3 : codec->m_param.cNumChannels;
    error = CheckRows(codec, channels, column, 0);
    if (error) return error;
    for (channel = 0; channel < channels; ++channel)
        if (codec->PredInfo[channel][column].piAD == NULL) return "null AC prediction buffer";
    updatePredInfo(codec, (CWMIMBInfo*)macroblock, column, (COLORFORMAT)format);
    return NULL;
}
