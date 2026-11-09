#include "strcodec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { JxrRowPixels = 16 * 16 * 8 };

typedef struct JxrRowsCase {
    CWMImageStrCodec codec[2];
    PixelI buffers[2][MAX_CHANNELS][2][JxrRowPixels];
    CWMIPredInfo rows[2][MAX_CHANNELS][2][8];
    CCodingContext context;
} JxrRowsCase;

static uint32_t Next(uint32_t* seed) {
    *seed = *seed * 1664525u + 1013904223u;
    return *seed >> 8;
}

static void Rebase(void* field, const JxrRowsCase* from, JxrRowsCase* to) {
    uintptr_t pointer;
    memcpy(&pointer, field, sizeof(pointer));
    if (pointer >= (uintptr_t)from && pointer < (uintptr_t)from + sizeof(JxrRowsCase)) {
        pointer = (uintptr_t)to + (pointer - (uintptr_t)from);
        memcpy(field, &pointer, sizeof(pointer));
    }
}

static void RebaseCodec(CWMImageStrCodec* codec, const JxrRowsCase* from, JxrRowsCase* to) {
    size_t channel;
    for (channel = 0; channel < MAX_CHANNELS; ++channel) {
        Rebase(&codec->pPlane[channel], from, to);
        Rebase(&codec->a0MBbuffer[channel], from, to);
        Rebase(&codec->a1MBbuffer[channel], from, to);
        Rebase(&codec->p0MBbuffer[channel], from, to);
        Rebase(&codec->p1MBbuffer[channel], from, to);
        Rebase(&codec->PredInfo[channel], from, to);
        Rebase(&codec->PredInfoPrevRow[channel], from, to);
    }
    Rebase(&codec->m_pNextSC, from, to);
}

void* JxrRowsMake_nid_no_patch(uint32_t seed, size_t channels, int32_t format, int32_t secondary) {
    JxrRowsCase* value = (JxrRowsCase*)malloc(sizeof(JxrRowsCase));
    unsigned char* bytes = (unsigned char*)value;
    size_t i, j, channel, tile;
    if (value == NULL) return NULL;
    for (i = 0; i < sizeof(JxrRowsCase); ++i) bytes[i] = (unsigned char)Next(&seed);
    for (j = 0; j < 2; ++j) {
        CWMImageStrCodec* codec = &value->codec[j];
        codec->m_param.cNumChannels = j == 0 ? channels : 1;
        codec->m_param.cfColorFormat = j == 0 ? (COLORFORMAT)format : Y_ONLY;
        codec->m_bSecondary = j == 1;
        codec->m_pNextSC = j == 0 && secondary ? &value->codec[1] : NULL;
        codec->WMISCP.cNumOfSliceMinus1V = Next(&seed) % 4;
        codec->WMISCP.cNumOfSliceMinus1H = Next(&seed) % 4;
        for (tile = 0; tile < 4; ++tile) {
            codec->WMISCP.uiTileX[tile] = (U32)(tile * 3);
            codec->WMISCP.uiTileY[tile] = (U32)(tile * 2);
        }
        codec->cTileColumn = 0;
        codec->cTileRow = 0;
        codec->cmbWidth = 12;
        codec->cmbHeight = 8;
        for (channel = 0; channel < MAX_CHANNELS; ++channel) {
            codec->a0MBbuffer[channel] = value->buffers[j][channel][0];
            codec->a1MBbuffer[channel] = value->buffers[j][channel][1];
            codec->p0MBbuffer[channel] = value->buffers[j][channel][0] + (Next(&seed) % 4) * 256;
            codec->p1MBbuffer[channel] = value->buffers[j][channel][1] + (Next(&seed) % 4) * 256;
            codec->PredInfo[channel] = value->rows[j][channel][0];
            codec->PredInfoPrevRow[channel] = value->rows[j][channel][1];
        }
    }
    return value;
}

void* JxrRowsClone_nid_no_patch(const void* source) {
    const JxrRowsCase* from = (const JxrRowsCase*)source;
    JxrRowsCase* value = (JxrRowsCase*)malloc(sizeof(JxrRowsCase));
    if (value == NULL) return NULL;
    memcpy(value, from, sizeof(JxrRowsCase));
    RebaseCodec(&value->codec[0], from, value);
    RebaseCodec(&value->codec[1], from, value);
    return value;
}

int JxrRowsEqual_nid_no_patch(const void* left, const void* right) {
    const JxrRowsCase* a = (const JxrRowsCase*)left;
    const JxrRowsCase* b = (const JxrRowsCase*)right;
    CWMImageStrCodec* normal = (CWMImageStrCodec*)malloc(sizeof(a->codec));
    int same;
    if (normal == NULL) return 0;
    memcpy(normal, b->codec, sizeof(b->codec));
    RebaseCodec(&normal[0], b, (JxrRowsCase*)a);
    RebaseCodec(&normal[1], b, (JxrRowsCase*)a);
    same = memcmp(normal, a->codec, sizeof(a->codec)) == 0 && memcmp(a->buffers, b->buffers, sizeof(a->buffers)) == 0 &&
           memcmp(a->rows, b->rows, sizeof(a->rows)) == 0 && memcmp(&a->context, &b->context, sizeof(a->context)) == 0;
    free(normal);
    return same;
}

void* JxrRowsCodec_nid_no_patch(void* value, size_t which) {
    return &((JxrRowsCase*)value)->codec[which];
}

void* JxrRowsContext_nid_no_patch(void* value) {
    return &((JxrRowsCase*)value)->context;
}

size_t JxrRowsCodecBytes_nid_no_patch(void) {
    return sizeof(CWMImageStrCodec);
}

void JxrRowsSetImage_nid_no_patch(void* value, int32_t yuv, int32_t imageFormat, int32_t paramFormat, int32_t depth,
                                   size_t bitsPerUnit, size_t lines, size_t stride) {
    CWMImageStrCodec* codec = &((JxrRowsCase*)value)->codec[0];
    codec->WMISCP.bYUVData = yuv;
    codec->WMII.cfColorFormat = (COLORFORMAT)imageFormat;
    codec->m_param.cfColorFormat = (COLORFORMAT)paramFormat;
    codec->WMII.bdBitDepth = (BITDEPTH_BITS)depth;
    codec->WMII.cBitsPerUnit = bitsPerUnit;
    codec->WMIBI.cLine = lines;
    codec->WMIBI.cbStride = stride;
}

void JxrRowsSetTiles_nid_no_patch(void* value, uint32_t columns, uint32_t rows, size_t column, size_t row) {
    CWMImageStrCodec* codec = &((JxrRowsCase*)value)->codec[0];
    codec->WMISCP.cNumOfSliceMinus1V = columns;
    codec->WMISCP.cNumOfSliceMinus1H = rows;
    codec->cTileColumn = column;
    codec->cTileRow = row;
}

void JxrRowsSetChannels_nid_no_patch(void* value, size_t which, size_t channels) {
    ((JxrRowsCase*)value)->codec[which].m_param.cNumChannels = channels;
}

void JxrRowsSetFormat_nid_no_patch(void* value, int32_t format) {
    ((JxrRowsCase*)value)->codec[0].m_param.cfColorFormat = (COLORFORMAT)format;
}

void JxrRowsSetSecondary_nid_no_patch(void* value, int32_t secondary) {
    ((JxrRowsCase*)value)->codec[0].m_bSecondary = secondary;
}

void JxrRowsLink_nid_no_patch(void* codec, void* next) {
    ((CWMImageStrCodec*)codec)->m_pNextSC = (CWMImageStrCodec*)next;
}

void JxrRowsSetRowBuffer_nid_no_patch(void* value, size_t which, size_t channel, int32_t current, void* buffer) {
    CWMImageStrCodec* codec = &((JxrRowsCase*)value)->codec[which];
    if (current) codec->p1MBbuffer[channel] = (PixelI*)buffer;
    else codec->p0MBbuffer[channel] = (PixelI*)buffer;
}

int32_t JxrRowsReferenceCheck_nid_no_patch(void* value, size_t width, size_t rows) {
    return checkImageBuffer(&((JxrRowsCase*)value)->codec[0], width, rows);
}

void JxrRowsReferenceTile_nid_no_patch(void* value, size_t column, size_t row) {
    getTilePos(&((JxrRowsCase*)value)->codec[0], column, row);
}

void JxrRowsReferenceInit_nid_no_patch(void* value) {
    initMRPtr(&((JxrRowsCase*)value)->codec[0]);
}

void JxrRowsReferenceAdvance_nid_no_patch(void* value) {
    advanceMRPtr(&((JxrRowsCase*)value)->codec[0]);
}

void JxrRowsReferenceSwap_nid_no_patch(void* value) {
    swapMRPtr(&((JxrRowsCase*)value)->codec[0]);
}

void JxrRowsReferenceNextRow_nid_no_patch(void* value) {
    advanceOneMBRow(&((JxrRowsCase*)value)->codec[0]);
}

void JxrRowsReferenceReset_nid_no_patch(void* value) {
    ResetCodingContext(&((JxrRowsCase*)value)->context);
}
