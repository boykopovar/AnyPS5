#include "strcodec.h"
#include "JXRGlue.h"
#include "ConverterCallbacks.h"
#include <stdint.h>
#include <stddef.h>

typedef char JxrEncodeStreamOffset[(offsetof(PKImageEncode, pStream) == 120) ? 1 : -1];
typedef char JxrEncodeStartOffset[(offsetof(PKImageEncode, offStart) == 128) ? 1 : -1];
typedef char JxrEncodeGuidOffset[(offsetof(PKImageEncode, guidPixFormat) == 136) ? 1 : -1];
typedef char JxrEncodeWidthOffset[(offsetof(PKImageEncode, uWidth) == 152) ? 1 : -1];
typedef char JxrEncodeHeaderDoneOffset[(offsetof(PKImageEncode, fHeaderDone) == 176) ? 1 : -1];
typedef char JxrEncodePixelOffsetOffset[(offsetof(PKImageEncode, offPixel) == 184) ? 1 : -1];
typedef char JxrEncodePixelSizeOffset[(offsetof(PKImageEncode, cbPixel) == 192) ? 1 : -1];
typedef char JxrEncodeColorContextOffset[(offsetof(PKImageEncode, pbColorContext) == 200) ? 1 : -1];
typedef char JxrEncodeColorContextSizeOffset[(offsetof(PKImageEncode, cbColorContext) == 208) ? 1 : -1];
typedef char JxrEncodeExifDataOffset[(offsetof(PKImageEncode, pbEXIFMetadata) == 216) ? 1 : -1];
typedef char JxrEncodeExifSizeOffset[(offsetof(PKImageEncode, cbEXIFMetadataByteCount) == 224) ? 1 : -1];
typedef char JxrEncodeGpsDataOffset[(offsetof(PKImageEncode, pbGPSInfoMetadata) == 232) ? 1 : -1];
typedef char JxrEncodeGpsSizeOffset[(offsetof(PKImageEncode, cbGPSInfoMetadataByteCount) == 240) ? 1 : -1];
typedef char JxrEncodeIptcDataOffset[(offsetof(PKImageEncode, pbIPTCNAAMetadata) == 248) ? 1 : -1];
typedef char JxrEncodeIptcSizeOffset[(offsetof(PKImageEncode, cbIPTCNAAMetadataByteCount) == 256) ? 1 : -1];
typedef char JxrEncodeXmpDataOffset[(offsetof(PKImageEncode, pbXMPMetadata) == 264) ? 1 : -1];
typedef char JxrEncodeXmpSizeOffset[(offsetof(PKImageEncode, cbXMPMetadataByteCount) == 272) ? 1 : -1];
typedef char JxrEncodePhotoshopDataOffset[(offsetof(PKImageEncode, pbPhotoshopMetadata) == 280) ? 1 : -1];
typedef char JxrEncodePhotoshopSizeOffset[(offsetof(PKImageEncode, cbPhotoshopMetadataByteCount) == 288) ? 1 : -1];
#include <string.h>

const void* JxrHashLookup_nid_no_patch(U8 hash) {
    return GetPixelFormatFromHash(hash);
}

typedef ERR (*IidLookup)(const char*, const PKIID**);

static int64_t StoreIid(IidLookup lookup, const char* extension, uint32_t* value) {
    const PKIID* iid = NULL;
    const ERR result = lookup(extension, &iid);
    if (result >= 0 && iid) *value = *iid;
    return result;
}

int64_t JxrEncodeIid_nid_no_patch(const char* extension, uint32_t* value) {
    return StoreIid(GetImageEncodeIID, extension, value);
}

int64_t JxrDecodeIid_nid_no_patch(const char* extension, uint32_t* value) {
    return StoreIid(GetImageDecodeIID, extension, value);
}

int64_t JxrAllocAligned_nid_no_patch(void** memory, size_t size, size_t alignment) {
    return PKAllocAligned(memory, size, alignment);
}

int64_t JxrFreeAligned_nid_no_patch(void** memory) {
    return PKFreeAligned(memory);
}

int32_t JxrIdpEmpty_nid_no_patch(void* codec) {
    return IDPEmpty((CWMImageStrCodec*)codec);
}

#define JXR_DECLARE_CONVERTER(name) extern ERR name(PKFormatConverter*, const PKRect*, U8*, U32);
JXR_CONVERTER_CALLBACKS(JXR_DECLARE_CONVERTER)
#undef JXR_DECLARE_CONVERTER

int64_t JxrInitializeConvertNative_nid_no_patch(void* converter, const void* sourceFormat, const char* extension,
                                                const void* targetFormat, int32_t* callbackId) {
    PKFormatConverter* pFC = (PKFormatConverter*)converter;
    PKPixelFormatGUID enPFFrom;
    PKPixelFormatGUID enPFTo;
    unsigned char previous[sizeof(pFC->Convert)];
    memcpy(previous, &pFC->Convert, sizeof(previous));
    memcpy(&enPFFrom, sourceFormat, sizeof(enPFFrom));
    memcpy(&enPFTo, targetFormat, sizeof(enPFTo));
    const ERR result = PKFormatConverter_InitializeConvert(pFC, enPFFrom, (char*)extension, enPFTo);
    *callbackId = -1;
    if (memcmp(&pFC->Convert, previous, sizeof(previous)) != 0) {
        *callbackId = -2;
#define JXR_MATCH_CONVERTER(name) if (pFC->Convert == name) *callbackId = JXR_CONVERT_##name;
        JXR_CONVERTER_CALLBACKS(JXR_MATCH_CONVERTER)
#undef JXR_MATCH_CONVERTER
        memcpy(&pFC->Convert, previous, sizeof(previous));
    }
    return result;
}

int64_t JxrConvertNative_nid_no_patch(void* converter, const void* rectangle, uint8_t* pixels, uint32_t stride,
                                      int32_t callbackId) {
    switch (callbackId) {
#define JXR_CALL_CONVERTER(name) case JXR_CONVERT_##name: return name((PKFormatConverter*)converter, (const PKRect*)rectangle, pixels, stride);
        JXR_CONVERTER_CALLBACKS(JXR_CALL_CONVERTER)
#undef JXR_CALL_CONVERTER
    default:
        return WMP_errUnsupportedFormat;
    }
}
