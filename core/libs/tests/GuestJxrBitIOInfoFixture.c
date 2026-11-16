#include "strcodec.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

static unsigned char original[sizeof(CWMImageStrCodec)];

void* JxrBitIOInfoFixtureCreate(int indexTable, int format, U32 vertical, U32 horizontal, int subband) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)calloc(1, sizeof(*codec));
    if (!codec) return NULL;
    codec->cbStruct = 0x12345678;
    codec->WMISCP.sbSubband = (SUBBAND)subband;
    codec->WMISCP.bfBitstreamFormat = (BITSTREAMFORMAT)format;
    codec->WMISCP.cNumOfSliceMinus1V = vertical;
    codec->WMISCP.cNumOfSliceMinus1H = horizontal;
    codec->m_param.bIndexTable = indexTable;
    codec->pIOHeader = (BitIOInfo*)(UINTPTR_T)0x1234;
    codec->pIndexTable = (size_t*)(UINTPTR_T)0x2345;
    codec->m_ppBitIO = (BitIOInfo**)(UINTPTR_T)0x3456;
    codec->cNumBitIO = 0x4567;
    codec->cHeaderSize = 0x5678;
    memcpy(original, codec, sizeof(*codec));
    return codec;
}

void JxrBitIOInfoFixtureDestroy(void* value) {
    free(value);
}

size_t JxrBitIOInfoFixtureSize(void) { return sizeof(CWMImageStrCodec); }
size_t JxrBitIOInfoFixtureBitIOSize(void) { return sizeof(BitIOInfo); }
size_t JxrBitIOInfoFixturePacketLength(void) { return PACKETLENGTH; }
size_t JxrBitIOInfoFixtureMaxTiles(void) { return MAX_TILES; }
size_t JxrBitIOInfoFixtureCount(void* value) { return ((CWMImageStrCodec*)value)->cNumBitIO; }
int JxrBitIOInfoFixtureSubbands(void* value) { return ((CWMImageStrCodec*)value)->cSB; }
void* JxrBitIOInfoFixtureArray(void* value) { return ((CWMImageStrCodec*)value)->m_ppBitIO; }
void* JxrBitIOInfoFixtureIndex(void* value) { return ((CWMImageStrCodec*)value)->pIndexTable; }
void* JxrBitIOInfoFixtureRing(void* value, size_t index) { return ((CWMImageStrCodec*)value)->m_ppBitIO[index]; }
size_t JxrBitIOInfoFixtureHeaderSize(void* value) { return ((CWMImageStrCodec*)value)->cHeaderSize; }
void* JxrBitIOInfoFixtureHeader(void* value) { return ((CWMImageStrCodec*)value)->pIOHeader; }
size_t JxrBitIOInfoFixtureStructSize(void* value) { return ((CWMImageStrCodec*)value)->cbStruct; }

int JxrBitIOInfoFixtureUnchanged(void* value) {
    unsigned char expected[sizeof(CWMImageStrCodec)];
    CWMImageStrCodec* codec = (CWMImageStrCodec*)value;
    memcpy(expected, original, sizeof(expected));
    memcpy(expected + offsetof(CWMImageStrCodec, cSB), &codec->cSB, sizeof(codec->cSB));
    memcpy(expected + offsetof(CWMImageStrCodec, cNumBitIO), &codec->cNumBitIO, sizeof(codec->cNumBitIO));
    memcpy(expected + offsetof(CWMImageStrCodec, m_ppBitIO), &codec->m_ppBitIO, sizeof(codec->m_ppBitIO));
    memcpy(expected + offsetof(CWMImageStrCodec, pIndexTable), &codec->pIndexTable, sizeof(codec->pIndexTable));
    return memcmp(expected, codec, sizeof(expected)) == 0;
}
