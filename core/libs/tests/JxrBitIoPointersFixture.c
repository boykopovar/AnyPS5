#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct JxrContextFixture {
    uint64_t before;
    CCodingContext contexts[4];
    uint64_t after;
} JxrContextFixture;

typedef struct JxrCodecFixture {
    uint64_t before;
    CWMImageStrCodec codec;
    uint64_t after;
} JxrCodecFixture;

static JxrContextFixture contextFixture;
static JxrCodecFixture codecFixture;
static BitIOInfo inputs[16];
static BitIOInfo* inputPointers[16];
static BitIOInfo sentinels[16];
static BitIOInfo streamHeader;
static size_t expectedBitIOCount;
static U32 expectedSliceCount;
static U8 expectedSubbands;
static BITSTREAMFORMAT expectedFormat;

void* JxrBitIoFixtureReset(BITSTREAMFORMAT format, size_t bitIOCount, U8 subbands, U32 sliceCount) {
    size_t i;

    memset(&contextFixture, 0, sizeof(contextFixture));
    memset(&codecFixture, 0, sizeof(codecFixture));
    memset(inputs, 0, sizeof(inputs));
    memset(inputPointers, 0, sizeof(inputPointers));
    memset(sentinels, 0, sizeof(sentinels));
    for(i = 0; i < 16; i ++)
        inputPointers[i] = &inputs[i];
    memset(&streamHeader, 0, sizeof(streamHeader));
    contextFixture.before = UINT64_C(0x123456789abcdef0);
    contextFixture.after = UINT64_C(0xfedcba9876543210);
    codecFixture.before = UINT64_C(0x0f1e2d3c4b5a6978);
    codecFixture.after = UINT64_C(0x8877665544332211);
    for(i = 0; i < 4; i ++){
        contextFixture.contexts[i].m_pIODC = &sentinels[i * 4];
        contextFixture.contexts[i].m_pIOLP = &sentinels[i * 4 + 1];
        contextFixture.contexts[i].m_pIOAC = &sentinels[i * 4 + 2];
        contextFixture.contexts[i].m_pIOFL = &sentinels[i * 4 + 3];
    }

    codecFixture.codec.cNumBitIO = bitIOCount;
    codecFixture.codec.cSB = subbands;
    codecFixture.codec.WMISCP.cNumOfSliceMinus1V = sliceCount;
    codecFixture.codec.WMISCP.bfBitstreamFormat = format;
    codecFixture.codec.m_pCodingContext = contextFixture.contexts;
    codecFixture.codec.m_ppBitIO = inputPointers;
    codecFixture.codec.pIOHeader = &streamHeader;
    expectedBitIOCount = bitIOCount;
    expectedSliceCount = sliceCount;
    expectedSubbands = subbands;
    expectedFormat = format;
    return &codecFixture.codec;
}

BitIOInfo* JxrBitIoFixtureField(size_t context, size_t field) {
    CCodingContext* value = &contextFixture.contexts[context];
    switch(field) {
    case 0: return value->m_pIODC;
    case 1: return value->m_pIOLP;
    case 2: return value->m_pIOAC;
    default: return value->m_pIOFL;
    }
}

BitIOInfo* JxrBitIoFixtureInput(size_t index) {
    return &inputs[index];
}

BitIOInfo* JxrBitIoFixtureSentinel(size_t context, size_t field) {
    return &sentinels[context * 4 + field];
}

BitIOInfo* JxrBitIoFixtureHeader(void) {
    return &streamHeader;
}

int JxrBitIoFixtureGuardsAndCodecIntact(void* codec) {
    JxrCodecFixture* value = (JxrCodecFixture*)((uint8_t*)codec - offsetof(JxrCodecFixture, codec));
    return value->before == UINT64_C(0x0f1e2d3c4b5a6978) &&
        value->after == UINT64_C(0x8877665544332211) &&
        value->codec.cNumBitIO == expectedBitIOCount &&
        value->codec.cSB == expectedSubbands &&
        value->codec.WMISCP.cNumOfSliceMinus1V == expectedSliceCount &&
        value->codec.WMISCP.bfBitstreamFormat == expectedFormat &&
        value->codec.m_pCodingContext == contextFixture.contexts &&
        value->codec.m_ppBitIO == inputPointers &&
        value->codec.pIOHeader == &streamHeader &&
        contextFixture.before == UINT64_C(0x123456789abcdef0) &&
        contextFixture.after == UINT64_C(0xfedcba9876543210);
}
