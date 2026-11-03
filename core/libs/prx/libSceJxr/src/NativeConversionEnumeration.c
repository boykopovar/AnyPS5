#include "strcodec.h"
#include "JXRGlue.h"
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(GUID) == 16 && sizeof(U32) == 4, "conversion enumeration ABI");

int64_t JxrEnumerateConversions_nid_no_patch(const void* source, uint32_t index, const void** target) {
    GUID input;
    const PKPixelFormatGUID* output;
    ERR result;
    memcpy(&input, source, sizeof(input));
    result = PKFormatConverter_EnumConversions(&input, index, &output);
    *target = output;
    return result;
}
