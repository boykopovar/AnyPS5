#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(BitIOInfo) == 48, "bit state size");
_Static_assert(offsetof(BitIOInfo, pbStart) == 16 && offsetof(BitIOInfo, offRef) == 40, "LP64 offsets");

uint32_t JxrReferencePeek(void* io, uint32_t bits) { return peekBit16((BitIOInfo*)io, bits); }
uint32_t JxrReferenceFlush(void* io, uint32_t bits) { return flushBit16((BitIOInfo*)io, bits); }
uint32_t JxrReferenceGet16(void* io, uint32_t bits) { return getBit16((BitIOInfo*)io, bits); }
uint32_t JxrReferenceGetBool(void* io) { return getBool16((BitIOInfo*)io); }
int32_t JxrReferenceGetSigned(void* io, uint32_t bits) { return getBit16s((BitIOInfo*)io, bits); }
uint32_t JxrReferenceGet32(void* io, uint32_t bits) { return getBit32((BitIOInfo*)io, bits); }
