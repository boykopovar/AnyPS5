#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct JxrTileFixture {
    uint64_t before;
    CWMImageStrCodec codec;
    uint64_t after;
} JxrTileFixture;

static JxrTileFixture fixture;
static CWMITile sentinel;
static unsigned char codecBefore[sizeof(CWMImageStrCodec)];

void* JxrTileFixtureReset(uint32_t sliceCount) {
    memset(&fixture, 0, sizeof(fixture));
    memset(&sentinel, 0x5a, sizeof(sentinel));
    fixture.before = UINT64_C(0x123456789abcdef0);
    fixture.after = UINT64_C(0xfedcba9876543210);
    fixture.codec.cbStruct = 0x11223344;
    fixture.codec.WMII.cWidth = 0x55667788;
    fixture.codec.WMII.cHeight = 0x99aabbcc;
    fixture.codec.WMISCP.cNumOfSliceMinus1V = sliceCount;
    fixture.codec.cRow = 0x10203040;
    fixture.codec.m_param.uQPMode = 0x50607080;
    fixture.codec.pTile = &sentinel;
    memcpy(codecBefore, &fixture.codec, sizeof(fixture.codec));
    return &fixture.codec;
}

void* JxrTileFixtureTilePointer(void* codec) {
    return ((CWMImageStrCodec*)codec)->pTile;
}

void* JxrTileFixtureSentinel(void) {
    return &sentinel;
}

size_t JxrTileFixtureTileSize(void) {
    return sizeof(CWMITile);
}

size_t JxrTileFixtureMaxTiles(void) {
    return MAX_TILES;
}

int JxrTileFixtureCodecIntact(void* codec) {
    JxrTileFixture* value = (JxrTileFixture*)((uint8_t*)codec - offsetof(JxrTileFixture, codec));
    unsigned char expected[sizeof(CWMImageStrCodec)];
    memcpy(expected, codecBefore, sizeof(expected));
    memcpy(expected + offsetof(CWMImageStrCodec, pTile), &value->codec.pTile, sizeof(value->codec.pTile));
    return value->before == UINT64_C(0x123456789abcdef0) &&
        value->after == UINT64_C(0xfedcba9876543210) &&
        memcmp(expected, &value->codec, sizeof(expected)) == 0;
}

int JxrTileFixtureTilesInitialized(void* codec) {
    CWMImageStrCodec* value = (CWMImageStrCodec*)codec;
    CWMITile expected;
    size_t i;

    if(value->pTile == NULL)
        return 0;
    memset(&expected, 0, sizeof(expected));
    expected.cNumQPLP = expected.cNumQPHP = 1;
    for(i = 0; i <= value->WMISCP.cNumOfSliceMinus1V; i ++)
        if(memcmp(&value->pTile[i], &expected, sizeof(expected)) != 0)
            return 0;
    return 1;
}
