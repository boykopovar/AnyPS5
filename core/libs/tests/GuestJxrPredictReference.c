#include "strcodec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct JxrPredictCase {
    CWMImageStrCodec codec;
    CWMITile tiles[4];
    CWMIQuantizer quantizers[4][3][MAX_CHANNELS][16];
    CWMIPredInfo rows[MAX_CHANNELS][2][8];
} JxrPredictCase;

static uint32_t Next(uint32_t* seed) {
    *seed = *seed * 1664525u + 1013904223u;
    return *seed >> 8;
}

void* JxrPredictMake_nid_no_patch(uint32_t seed, size_t channels, int32_t format, uint32_t tiles, size_t width) {
    JxrPredictCase* value = (JxrPredictCase*)calloc(1, sizeof(JxrPredictCase));
    unsigned char* bytes;
    size_t i, tile, band, channel, row, column;
    if (value == NULL) return NULL;
    bytes = (unsigned char*)value;
    for (i = 0; i < sizeof(JxrPredictCase); ++i) bytes[i] = (unsigned char)Next(&seed);
    memset(&value->codec, 0, sizeof(value->codec));
    value->codec.m_param.cNumChannels = channels;
    value->codec.m_param.cfColorFormat = (COLORFORMAT)format;
    value->codec.WMISCP.cNumOfSliceMinus1V = tiles - 1;
    value->codec.cmbWidth = width;
    value->codec.pTile = value->tiles;
    value->codec.MBInfo.iQIndexLP = (U8)(Next(&seed) & 3);
    for (tile = 0; tile < 4; ++tile) {
        for (channel = 0; channel < MAX_CHANNELS; ++channel) {
            value->tiles[tile].pQuantizerDC[channel] = value->quantizers[tile][0][channel];
            value->tiles[tile].pQuantizerLP[channel] = value->quantizers[tile][1][channel];
            value->tiles[tile].pQuantizerHP[channel] = value->quantizers[tile][2][channel];
        }
        for (band = 0; band < 3; ++band)
            for (channel = 0; channel < MAX_CHANNELS; ++channel)
                for (i = 0; i < 16; ++i) value->quantizers[tile][band][channel][i].iIndex = (U8)(Next(&seed) & 3);
    }
    for (channel = 0; channel < MAX_CHANNELS; ++channel) {
        value->codec.PredInfo[channel] = value->rows[channel][0];
        value->codec.PredInfoPrevRow[channel] = value->rows[channel][1];
        for (row = 0; row < 2; ++row)
            for (column = 0; column < 8; ++column) {
                CWMIPredInfo* info = &value->rows[channel][row][column];
                info->iQPIndex = (Int)(Next(&seed) & 3);
                info->iDC = (PixelI)(Next(&seed) % 2001) - 1000;
                info->piAD = info->iAD;
            }
    }
    return value;
}

void* JxrPredictClone_nid_no_patch(const void* source) {
    const JxrPredictCase* from = (const JxrPredictCase*)source;
    JxrPredictCase* value = (JxrPredictCase*)malloc(sizeof(JxrPredictCase));
    const unsigned char* base = (const unsigned char*)from;
    size_t tile, channel, row, column;
    if (value == NULL) return NULL;
    memcpy(value, from, sizeof(JxrPredictCase));
    value->codec.pTile = value->tiles;
    for (tile = 0; tile < 4; ++tile)
        for (channel = 0; channel < MAX_CHANNELS; ++channel) {
            value->tiles[tile].pQuantizerDC[channel] =
                (CWMIQuantizer*)((unsigned char*)value + ((const unsigned char*)from->tiles[tile].pQuantizerDC[channel] - base));
            value->tiles[tile].pQuantizerLP[channel] =
                (CWMIQuantizer*)((unsigned char*)value + ((const unsigned char*)from->tiles[tile].pQuantizerLP[channel] - base));
            value->tiles[tile].pQuantizerHP[channel] =
                (CWMIQuantizer*)((unsigned char*)value + ((const unsigned char*)from->tiles[tile].pQuantizerHP[channel] - base));
        }
    for (channel = 0; channel < MAX_CHANNELS; ++channel) {
        value->codec.PredInfo[channel] = value->rows[channel][0];
        value->codec.PredInfoPrevRow[channel] = value->rows[channel][1];
        for (row = 0; row < 2; ++row)
            for (column = 0; column < 8; ++column) value->rows[channel][row][column].piAD = value->rows[channel][row][column].iAD;
    }
    return value;
}

int JxrPredictEqual_nid_no_patch(const void* left, const void* right) {
    const JxrPredictCase* a = (const JxrPredictCase*)left;
    const JxrPredictCase* b = (const JxrPredictCase*)right;
    size_t tile, channel, row, column;
    if (memcmp(a->quantizers, b->quantizers, sizeof(a->quantizers)) != 0) return 0;
    for (tile = 0; tile < 4; ++tile)
        for (channel = 0; channel < MAX_CHANNELS; ++channel) {
            if ((const unsigned char*)a->tiles[tile].pQuantizerDC[channel] - (const unsigned char*)a !=
                (const unsigned char*)b->tiles[tile].pQuantizerDC[channel] - (const unsigned char*)b) return 0;
            if ((const unsigned char*)a->tiles[tile].pQuantizerLP[channel] - (const unsigned char*)a !=
                (const unsigned char*)b->tiles[tile].pQuantizerLP[channel] - (const unsigned char*)b) return 0;
            if ((const unsigned char*)a->tiles[tile].pQuantizerHP[channel] - (const unsigned char*)a !=
                (const unsigned char*)b->tiles[tile].pQuantizerHP[channel] - (const unsigned char*)b) return 0;
        }
    for (channel = 0; channel < MAX_CHANNELS; ++channel)
        for (row = 0; row < 2; ++row)
            for (column = 0; column < 8; ++column) {
                const CWMIPredInfo* x = &a->rows[channel][row][column];
                const CWMIPredInfo* y = &b->rows[channel][row][column];
                if (x->iQPIndex != y->iQPIndex || x->iCBP != y->iCBP || x->iDC != y->iDC ||
                    memcmp(x->iAD, y->iAD, sizeof(x->iAD)) != 0)
                    return 0;
            }
    return 1;
}

void JxrPredictContext_nid_no_patch(void* value, int32_t left, int32_t top) {
    ((JxrPredictCase*)value)->codec.m_bCtxLeft = left;
    ((JxrPredictCase*)value)->codec.m_bCtxTop = top;
}

void JxrPredictFillMacroblock_nid_no_patch(void* macroblock, uint32_t seed, uint8_t index) {
    CWMIMBInfo* info = (CWMIMBInfo*)macroblock;
    size_t channel, i;
    for (channel = 0; channel < MAX_CHANNELS; ++channel)
        for (i = 0; i < 16; ++i) info->iBlockDC[channel][i] = (I32)(Next(&seed) % 2001) - 1000;
    info->iQIndexLP = index;
}

void* JxrPredictMacroblockOf_nid_no_patch(void* value) {
    return &((JxrPredictCase*)value)->codec.MBInfo;
}

void JxrPredictSetPredInfo_nid_no_patch(void* value, size_t channel, int previous, void* row) {
    if (previous) ((JxrPredictCase*)value)->codec.PredInfoPrevRow[channel] = (CWMIPredInfo*)row;
    else ((JxrPredictCase*)value)->codec.PredInfo[channel] = (CWMIPredInfo*)row;
}

void JxrPredictSetTileQuantizer_nid_no_patch(void* value, size_t tile, size_t band, size_t channel, void* quantizer) {
    CWMITile* entry = &((JxrPredictCase*)value)->tiles[tile];
    if (band == 0) entry->pQuantizerDC[channel] = (CWMIQuantizer*)quantizer;
    else if (band == 1) entry->pQuantizerLP[channel] = (CWMIQuantizer*)quantizer;
    else entry->pQuantizerHP[channel] = (CWMIQuantizer*)quantizer;
}

void JxrPredictReferenceUniform_nid_no_patch(void* value, size_t band) {
    setUniformQuantizer(&((JxrPredictCase*)value)->codec, band);
}

void JxrPredictReferenceUseDc_nid_no_patch(void* value, size_t tile) {
    useDCQuantizer(&((JxrPredictCase*)value)->codec, tile);
}

void JxrPredictReferenceUseLp_nid_no_patch(void* value, size_t count, size_t tile) {
    useLPQuantizer(&((JxrPredictCase*)value)->codec, count, tile);
}

int32_t JxrPredictReferenceMode_nid_no_patch(void* value, size_t column) {
    return getDCACPredMode(&((JxrPredictCase*)value)->codec, column);
}

void JxrPredictReferenceUpdate_nid_no_patch(void* value, void* macroblock, size_t column, int32_t format) {
    updatePredInfo(&((JxrPredictCase*)value)->codec, (CWMIMBInfo*)macroblock, column, (COLORFORMAT)format);
}
