#include "strcodec.h"
#include <stdint.h>
#include <string.h>

static struct {
    uint64_t before;
    CWMImageStrCodec codec;
    CWMITile tiles[3];
    CWMIQuantizer quantizers[3][3][2];
    uint64_t after;
} fixture;
static unsigned char original[sizeof(fixture)];

void* JxrFreeTileFixtureReset(uint32_t mode, int32_t subband, uint32_t lastTile, int empty) {
    size_t tile, band;
    memset(&fixture, 0x5a, sizeof(fixture));
    fixture.before = UINT64_C(0x0123456789abcdef);
    fixture.after = UINT64_C(0xfedcba9876543210);
    fixture.codec.m_param.uQPMode = mode;
    fixture.codec.WMISCP.sbSubband = (SUBBAND)subband;
    fixture.codec.WMISCP.cNumOfSliceMinus1V = lastTile;
    fixture.codec.pTile = fixture.tiles;
    for(tile = 0; tile < 3; ++tile) {
        CWMIQuantizer** tables[3] = {fixture.tiles[tile].pQuantizerDC, fixture.tiles[tile].pQuantizerLP, fixture.tiles[tile].pQuantizerHP};
        for(band = 0; band < 3; ++band) {
            tables[band][0] = empty && ((tile + band) % 2 == 0) ? NULL : &fixture.quantizers[tile][band][0];
            tables[band][1] = &fixture.quantizers[tile][band][1];
        }
    }
    memcpy(original, &fixture, sizeof(fixture));
    return &fixture.codec;
}

void* JxrFreeTileFixtureSlot(size_t tile, size_t band) {
    if(band == 0) return fixture.tiles[tile].pQuantizerDC[0];
    if(band == 1) return fixture.tiles[tile].pQuantizerLP[0];
    return fixture.tiles[tile].pQuantizerHP[0];
}

void* JxrFreeTileFixtureTiles(void) { return fixture.tiles; }
int JxrFreeTileFixtureIntact(void) { return memcmp(original, &fixture, sizeof(fixture)) == 0; }
void JxrFreeTileFixtureClearTiles(void) { fixture.codec.pTile = NULL; }
