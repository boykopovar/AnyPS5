#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(SimpleBitIO) == 24, "SimpleBitIO size");
_Static_assert(offsetof(SimpleBitIO, cbRead) == 8, "SimpleBitIO byte count");
_Static_assert(offsetof(SimpleBitIO, bAccumulator) == 12, "SimpleBitIO accumulator");
_Static_assert(offsetof(SimpleBitIO, cBitLeft) == 16, "SimpleBitIO bit count");

static const uint8_t* referenceData;
static size_t referenceSize;
static size_t referenceOffset;

static ERR ReferenceRead(struct WMPStream* stream, void* buffer, size_t size) {
    (void)stream;
    uint8_t* bytes = (uint8_t*)buffer;
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = referenceOffset < referenceSize ? referenceData[referenceOffset] : 0;
        ++referenceOffset;
    }
    return WMP_errSuccess;
}

void JxrReferenceSimpleRead_nid_no_patch(const uint8_t* data, size_t size, const uint32_t* counts, size_t count, uint32_t* results, uint32_t* state) {
    struct WMPStream stream;
    SimpleBitIO io;
    memset(&stream, 0, sizeof(stream));
    stream.Read = ReferenceRead;
    referenceData = data;
    referenceSize = size;
    referenceOffset = 0;
    attach_SB(&io, &stream);
    for (size_t i = 0; i < count; ++i) {
        if (counts[i] == UINT32_MAX) {
            flushToByte_SB(&io);
            results[i] = 0;
        } else {
            results[i] = getBit32_SB(&io, counts[i]);
        }
        state[i * 3] = getByteRead_SB(&io);
        state[i * 3 + 1] = io.bAccumulator;
        state[i * 3 + 2] = io.cBitLeft;
    }
}
