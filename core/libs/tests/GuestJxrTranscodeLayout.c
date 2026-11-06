#include "strcodec.h"
#include "JXRGlue.h"
#include <stddef.h>
#include <stdint.h>

struct GuestDecoderLongFields {
    int32_t hasAlpha;
    int64_t imageOffset;
    int64_t imageSize;
    int64_t alphaOffset;
    int64_t alphaSize;
    int32_t ignoreOverlap;
};

size_t JxrTranscodeFixtureOverlapOffset(void) {
    if (sizeof(Long) == 8) return offsetof(PKImageDecode, WMP.bIgnoreOverlap);
    return offsetof(PKImageDecode, WMP.bHasAlpha) + offsetof(struct GuestDecoderLongFields, ignoreOverlap);
}
