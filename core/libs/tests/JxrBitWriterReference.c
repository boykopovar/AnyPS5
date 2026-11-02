#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(BitIOInfo) == 48, "bit state size");
_Static_assert(offsetof(BitIOInfo, pbStart) == 16 && offsetof(BitIOInfo, offRef) == 40, "LP64 offsets");

void JxrReferencePutBit16z(void* io, uint32_t bits, uint32_t width) { putBit16z((BitIOInfo*)io, bits, width); }
void JxrReferencePutBit16(void* io, uint32_t bits, uint32_t width) { putBit16((BitIOInfo*)io, bits, width); }
void JxrReferencePutBit32(void* io, uint32_t bits, uint32_t width) { putBit32((BitIOInfo*)io, bits, width); }
void JxrReferenceFillToByte(void* io) { fillToByte((BitIOInfo*)io); }
uint32_t JxrReferenceSizeRead(void* io) { return getSizeRead((BitIOInfo*)io); }
uint32_t JxrReferenceSizeWrite(void* io) { return getSizeWrite((BitIOInfo*)io); }
uint32_t JxrReferencePosRead(void* io) { return getPosRead((BitIOInfo*)io); }
