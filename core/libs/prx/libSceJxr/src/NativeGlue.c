#include "strcodec.h"
#include "JXRGlue.h"
#include "prx/libc/include/general/VabiMacros.hpp"
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

_Static_assert(sizeof(void*) == 8, "JXR guest pointer width");
_Static_assert(offsetof(PKFormatConverter, GetPixelFormat) == 16, "converter target callback offset");
_Static_assert(offsetof(PKFormatConverter, GetSourcePixelFormat) == 24, "converter source callback offset");
_Static_assert(offsetof(PKFormatConverter, Copy) == 48, "converter copy callback offset");
_Static_assert(offsetof(PKImageEncode, WritePixels) == 56, "encoder write callback offset");
_Static_assert(offsetof(PKImageEncode, guidPixFormat) == 136, "encoder pixel format offset");
_Static_assert(offsetof(PKImageEncode, uWidth) == 152, "encoder width offset");
_Static_assert(sizeof(PKRect) == 16, "rectangle size");

size_t JxrWriteSourceRectWidthOffset_nid_no_patch(void) { return offsetof(PKRect, Width); }
size_t JxrWriteSourceRectHeightOffset_nid_no_patch(void) { return offsetof(PKRect, Height); }

size_t JxrWriteSourceSourceCallbackSize_nid_no_patch(void) {
    return offsetof(PKFormatConverter, GetSourcePixelFormat) + sizeof(((PKFormatConverter*)0)->GetSourcePixelFormat);
}

size_t JxrWriteSourceTargetCallbackSize_nid_no_patch(void) {
    return offsetof(PKFormatConverter, GetPixelFormat) + sizeof(((PKFormatConverter*)0)->GetPixelFormat);
}

size_t JxrWriteSourceCopyCallbackSize_nid_no_patch(void) {
    return offsetof(PKFormatConverter, Copy) + sizeof(((PKFormatConverter*)0)->Copy);
}

size_t JxrWriteSourceEncoderFormatSize_nid_no_patch(void) {
    return offsetof(PKImageEncode, guidPixFormat) + sizeof(((PKImageEncode*)0)->guidPixFormat);
}

size_t JxrWriteSourceEncoderWidthSize_nid_no_patch(void) {
    return offsetof(PKImageEncode, uWidth) + sizeof(((PKImageEncode*)0)->uWidth);
}

size_t JxrWriteSourceEncoderCallbackSize_nid_no_patch(void) {
    return offsetof(PKImageEncode, WritePixels) + sizeof(((PKImageEncode*)0)->WritePixels);
}

uintptr_t JxrWriteSourceSourceCallback_nid_no_patch(const void* record) {
    return (uintptr_t)((const PKFormatConverter*)record)->GetSourcePixelFormat;
}

uintptr_t JxrWriteSourceTargetCallback_nid_no_patch(const void* record) {
    return (uintptr_t)((const PKFormatConverter*)record)->GetPixelFormat;
}

uintptr_t JxrWriteSourceCopyCallback_nid_no_patch(const void* record) {
    return (uintptr_t)((const PKFormatConverter*)record)->Copy;
}

void JxrWriteSourceEncoderFormat_nid_no_patch(const void* record, void* format) {
    memcpy(format, &((const PKImageEncode*)record)->guidPixFormat, sizeof(PKPixelFormatGUID));
}

uint32_t JxrWriteSourceEncoderWidth_nid_no_patch(const void* record) {
    return ((const PKImageEncode*)record)->uWidth;
}

uintptr_t JxrWriteSourceEncoderCallback_nid_no_patch(const void* record) {
    return (uintptr_t)((const PKImageEncode*)record)->WritePixels;
}

int32_t JxrWriteSourceYuvStrideGuid_nid_no_patch(const void* guid) {
    return memcmp(guid, &GUID_PKPixelFormat12bppYUV420, sizeof(GUID)) == 0 ||
           memcmp(guid, &GUID_PKPixelFormat16bppYUV422, sizeof(GUID)) == 0;
}

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

typedef int64_t (APS5_VABI *JxrGuestGetPos)(struct WMPStream*, size_t*);
typedef int64_t (APS5_VABI *JxrGuestClose)(struct WMPStream**);

int32_t JxrDecodeStreamHasGetPos_nid_no_patch(void* stream) {
    return ((struct WMPStream*)stream)->GetPos != NULL;
}

int32_t JxrDecodeHasClose_nid_no_patch(void* decoder) {
    const PKImageDecode* pID = (const PKImageDecode*)decoder;
    return !pID->fStreamOwner || (pID->pStream != NULL && pID->pStream->Close != NULL);
}

int64_t JxrDecodeInitialize_nid_no_patch(void* decoder, void* stream) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    struct WMPStream* pStream = (struct WMPStream*)stream;

    pID->pStream = pStream;
    pID->guidPixFormat = GUID_PKPixelFormatDontCare;
    pID->fResX = 96;
    pID->fResY = 96;
    pID->cFrame = 1;

    if (!Failed(((JxrGuestGetPos)pStream->GetPos)(pStream, &pID->offStart)))
        memset(&pID->WMP.wmiDEMisc, 0, sizeof(pID->WMP.wmiDEMisc));
    return WMP_errSuccess;
}

int64_t JxrDecodeGetPixelFormat_nid_no_patch(void* decoder, void* format) {
    return PKImageDecode_GetPixelFormat((PKImageDecode*)decoder, (PKPixelFormatGUID*)format);
}

int64_t JxrDecodeGetSize_nid_no_patch(void* decoder, int32_t* width, int32_t* height) {
    return PKImageDecode_GetSize((PKImageDecode*)decoder, (I32*)width, (I32*)height);
}

int64_t JxrDecodeGetResolution_nid_no_patch(void* decoder, float* x, float* y) {
    return PKImageDecode_GetResolution((PKImageDecode*)decoder, x, y);
}

int64_t JxrDecodeGetFrameCount_nid_no_patch(void* decoder, uint32_t* count) {
    return PKImageDecode_GetFrameCount((PKImageDecode*)decoder, count);
}

int64_t JxrDecodeGetColorContext_nid_no_patch(void* decoder, uint8_t* buffer, uint32_t* size) {
    return PKImageDecode_GetColorContext((PKImageDecode*)decoder, buffer, size);
}

int64_t JxrDecodeGetDescriptiveMetadata_nid_no_patch(void* decoder, void* metadata) {
    return PKImageDecode_GetDescriptiveMetadata((PKImageDecode*)decoder, (DESCRIPTIVEMETADATA*)metadata);
}

int64_t JxrDecodeRelease_nid_no_patch(void** decoder) {
    PKImageDecode* pID = (PKImageDecode*)*decoder;
    if (pID->fStreamOwner) ((JxrGuestClose)pID->pStream->Close)(&pID->pStream);
    return PKFree(decoder);
}
