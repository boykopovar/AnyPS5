#include "JXRGlue.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t fixturePosition;
static int64_t fixtureGetPosResult;
static int64_t fixtureCloseResult;
static size_t fixtureGetPosCalls;
static size_t fixtureCloseCalls;
static void* fixtureExpectedCloseSlot;
static int fixtureCloseSlotMatched;

static int64_t APS5_VABI FixtureGetPos(struct WMPStream* stream, size_t* position) {
    (void)stream;
    ++fixtureGetPosCalls;
    if (fixtureGetPosResult >= 0) *position = fixturePosition;
    return fixtureGetPosResult;
}

static int64_t APS5_VABI FixtureClose(struct WMPStream** stream) {
    ++fixtureCloseCalls;
    fixtureCloseSlotMatched = stream == (struct WMPStream**)fixtureExpectedCloseSlot;
    *stream = NULL;
    return fixtureCloseResult;
}

size_t JxrDecodeFixtureSize(void) {
    const size_t longStart = (offsetof(PKImageDecode, WMP.bHasAlpha) + sizeof(Bool) + 7) & ~(size_t)7;
    const size_t rowStart = (longStart + 4 * sizeof(int64_t) + sizeof(Bool) + 7) & ~(size_t)7;
    return sizeof(PKImageDecode) + rowStart - offsetof(PKImageDecode, WMP.DecoderCurrMBRow);
}
size_t JxrDecodeFixtureInitializeOffset(void) { return offsetof(PKImageDecode, Initialize); }
size_t JxrDecodeFixtureWmpOffset(void) { return offsetof(PKImageDecode, WMP); }

int32_t JxrDecodeFixtureCallbacksValid(void* value) {
    PKImageDecode* decoder = (PKImageDecode*)value;
    return decoder->Initialize != NULL && decoder->GetPixelFormat != NULL && decoder->GetSize != NULL &&
        decoder->GetResolution != NULL && decoder->GetColorContext != NULL && decoder->GetDescriptiveMetadata != NULL &&
        decoder->GetRawStream == NULL && decoder->Copy != NULL && decoder->GetFrameCount != NULL &&
        decoder->SelectFrame != NULL && decoder->Release != NULL;
}

int32_t JxrDecodeFixtureZeroState(void* value) {
    const uint8_t* bytes = (const uint8_t*)value;
    for (size_t i = offsetof(PKImageDecode, pStream); i < JxrDecodeFixtureSize(); ++i)
        if (bytes[i] != 0) return 0;
    return 1;
}

int32_t JxrDecodeFixtureFormatMatches(const void* value) {
    const PKPixelFormatGUID expected = GUID_PKPixelFormatDontCare;
    return memcmp(value, &expected, sizeof(expected)) == 0;
}

int64_t JxrDecodeFixtureCallGetPixelFormat(void* value, void* format) {
    typedef int64_t (APS5_VABI *Callback)(void*, void*);
    return ((Callback)((PKImageDecode*)value)->GetPixelFormat)(value, format);
}

int64_t JxrDecodeFixtureCallGetSize(void* value, int32_t* width, int32_t* height) {
    typedef int64_t (APS5_VABI *Callback)(void*, int32_t*, int32_t*);
    return ((Callback)((PKImageDecode*)value)->GetSize)(value, width, height);
}

int64_t JxrDecodeFixtureCallGetResolution(void* value, float* x, float* y) {
    typedef int64_t (APS5_VABI *Callback)(void*, float*, float*);
    return ((Callback)((PKImageDecode*)value)->GetResolution)(value, x, y);
}

int64_t JxrDecodeFixtureCallGetColorContext(void* value, uint8_t* buffer, uint32_t* size) {
    typedef int64_t (APS5_VABI *Callback)(void*, uint8_t*, uint32_t*);
    return ((Callback)((PKImageDecode*)value)->GetColorContext)(value, buffer, size);
}

int64_t JxrDecodeFixtureCallGetMetadata(void* value, void* metadata) {
    typedef int64_t (APS5_VABI *Callback)(void*, void*);
    return ((Callback)((PKImageDecode*)value)->GetDescriptiveMetadata)(value, metadata);
}

int64_t JxrDecodeFixtureCallCopy(void* value, const void* rectangle, uint8_t* buffer, uint32_t stride) {
    typedef int64_t (APS5_VABI *Callback)(void*, const void*, uint8_t*, uint32_t);
    return ((Callback)((PKImageDecode*)value)->Copy)(value, rectangle, buffer, stride);
}

int64_t JxrDecodeFixtureCallGetFrameCount(void* value, uint32_t* count) {
    typedef int64_t (APS5_VABI *Callback)(void*, uint32_t*);
    return ((Callback)((PKImageDecode*)value)->GetFrameCount)(value, count);
}

int64_t JxrDecodeFixtureCallSelectFrame(void* value, uint32_t frame) {
    typedef int64_t (APS5_VABI *Callback)(void*, uint32_t);
    return ((Callback)((PKImageDecode*)value)->SelectFrame)(value, frame);
}

int64_t JxrDecodeFixtureCallInitialize(void* value, void* stream) {
    typedef int64_t (APS5_VABI *Callback)(void*, void*);
    return ((Callback)((PKImageDecode*)value)->Initialize)(value, stream);
}

int64_t JxrDecodeFixtureCallRelease(void** value) {
    typedef int64_t (APS5_VABI *Callback)(void**);
    return ((Callback)((PKImageDecode*)*value)->Release)(value);
}

void* JxrDecodeFixtureCreateStream(size_t position, int64_t getPosResult, int64_t closeResult) {
    struct WMPStream* stream = (struct WMPStream*)calloc(1, sizeof(*stream));
    if (!stream) return NULL;
    fixturePosition = position;
    fixtureGetPosResult = getPosResult;
    fixtureCloseResult = closeResult;
    fixtureGetPosCalls = 0;
    fixtureCloseCalls = 0;
    fixtureExpectedCloseSlot = NULL;
    fixtureCloseSlotMatched = 0;
    stream->GetPos = (ERR (*)(struct WMPStream*, size_t*))FixtureGetPos;
    stream->Close = (ERR (*)(struct WMPStream**))FixtureClose;
    return stream;
}

void JxrDecodeFixtureDestroyStream(void* stream) { free(stream); }
size_t JxrDecodeFixtureGetPosCalls(void) { return fixtureGetPosCalls; }
size_t JxrDecodeFixtureCloseCalls(void) { return fixtureCloseCalls; }
int32_t JxrDecodeFixtureCloseSlotMatches(void) {
    return fixtureCloseSlotMatched;
}
void* JxrDecodeFixtureStream(void* decoder) { return ((PKImageDecode*)decoder)->pStream; }
size_t JxrDecodeFixtureOffset(void* decoder) { return ((PKImageDecode*)decoder)->offStart; }
int32_t JxrDecodeFixtureInitializedFields(void* value) {
    PKImageDecode* decoder = (PKImageDecode*)value;
    const PKPixelFormatGUID expected = GUID_PKPixelFormatDontCare;
    return memcmp(&decoder->guidPixFormat, &expected, sizeof(expected)) == 0 &&
        decoder->fResX == 96 && decoder->fResY == 96 && decoder->cFrame == 1;
}
void JxrDecodeFixtureFillMisc(void* value, int byte) {
    memset(&((PKImageDecode*)value)->WMP.wmiDEMisc, byte, sizeof(((PKImageDecode*)value)->WMP.wmiDEMisc));
}
int32_t JxrDecodeFixtureMiscIs(void* value, int byte) {
    const uint8_t* bytes = (const uint8_t*)&((PKImageDecode*)value)->WMP.wmiDEMisc;
    for (size_t i = 0; i < sizeof(((PKImageDecode*)value)->WMP.wmiDEMisc); ++i)
        if (bytes[i] != (uint8_t)byte) return 0;
    return 1;
}
void JxrDecodeFixtureSetStreamOwner(void* value, int owner) {
    PKImageDecode* decoder = (PKImageDecode*)value;
    decoder->fStreamOwner = owner;
    fixtureExpectedCloseSlot = &decoder->pStream;
}
