#include "strcodec.h"
#include "JXRGlue.h"

const void* JxrHashLookup_nid_no_patch(U8 hash) {
    return GetPixelFormatFromHash(hash);
}
