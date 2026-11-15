#include "strcodec.h"
#include <stdint.h>

_Static_assert(sizeof(BitIOInfo) == 48, "BitIOInfo size");

static void PutMasked(BitIOInfo* io, uint32_t value, uint32_t bits) {
    putBit16z(io, value & (0xffffu >> (16 - bits)), bits);
}

uint32_t JxrBitIoReference_nid_no_patch(int operation, void* state, uint32_t value, uint32_t bits) {
    BitIOInfo* io = (BitIOInfo*)state;
    switch (operation) {
    case 0: return peekBit16(io, bits);
    case 1: return flushBit16(io, bits);
    case 2: return getBit16(io, bits);
    case 3: return getBool16(io);
    case 4: return (uint32_t)getBit16s(io, bits);
    case 5: return getBit32(io, bits);
    case 6: return flushToByte(io);
    case 7: return getSizeRead(io);
    case 8: return getPosRead(io);
    case 9: putBit16z(io, value, bits); return 0;
    case 10: PutMasked(io, value, bits); return 0;
    case 11:
        if (bits > 16) {
            PutMasked(io, value >> (bits - 16), 16);
            bits -= 16;
        }
        PutMasked(io, value, bits);
        return 0;
    case 12: fillToByte(io); return 0;
    case 13: return getSizeWrite(io);
    case 14: return dquantBits((U8)value);
    default: return 0xffffffffu;
    }
}
