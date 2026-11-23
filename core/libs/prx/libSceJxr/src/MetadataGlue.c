#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

static size_t CaptionUnits(const uint16_t* caption) {
    const uint16_t* end = caption;
    while (*end != 0) ++end;
    return (size_t)(end - caption);
}

#define wcslen(value) CaptionUnits((const uint16_t*)(value))
#define PKFree JxrMetadataFree_nid_no_patch
#define FreeDescMetadata JxrMetadataRelease_nid_no_patch
#include "JXRGlueJxr.c"
