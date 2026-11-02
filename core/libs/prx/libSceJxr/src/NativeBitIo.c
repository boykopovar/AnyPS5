#include "strcodec.h"
#include <stdint.h>

_Static_assert(sizeof(BitIOInfo) == 48, "BitIOInfo size");

uint32_t JxrPeekBit16_nid_no_patch(void* io, uint32_t bits) { return peekBit16((BitIOInfo*)io, bits); }
uint32_t JxrFlushBit16_nid_no_patch(void* io, uint32_t bits) { return flushBit16((BitIOInfo*)io, bits); }
uint32_t JxrGetBit16_nid_no_patch(void* io, uint32_t bits) { return getBit16((BitIOInfo*)io, bits); }
uint32_t JxrGetBool16_nid_no_patch(void* io) { return getBool16((BitIOInfo*)io); }
int32_t JxrGetBit16s_nid_no_patch(void* io, uint32_t bits) { return getBit16s((BitIOInfo*)io, bits); }
uint32_t JxrGetBit32_nid_no_patch(void* io, uint32_t bits) { return getBit32((BitIOInfo*)io, bits); }
uint32_t JxrFlushToByte_nid_no_patch(void* io) { return flushToByte((BitIOInfo*)io); }
void JxrPutBit16z_nid_no_patch(void* io, uint32_t value, uint32_t bits) { putBit16z((BitIOInfo*)io, value, bits); }
void JxrPutBit16_nid_no_patch(void* io, uint32_t value, uint32_t bits) { putBit16((BitIOInfo*)io, value, bits); }
void JxrPutBit32_nid_no_patch(void* io, uint32_t value, uint32_t bits) { putBit32((BitIOInfo*)io, value, bits); }
void JxrFillToByte_nid_no_patch(void* io) { fillToByte((BitIOInfo*)io); }
uint32_t JxrGetSizeRead_nid_no_patch(void* io) { return getSizeRead((BitIOInfo*)io); }
uint32_t JxrGetSizeWrite_nid_no_patch(void* io) { return getSizeWrite((BitIOInfo*)io); }
uint32_t JxrGetPosRead_nid_no_patch(void* io) { return getPosRead((BitIOInfo*)io); }
uint8_t JxrDquantBits_nid_no_patch(uint8_t qp) { return dquantBits(qp); }
