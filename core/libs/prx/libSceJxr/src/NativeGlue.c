#include "strcodec.h"
#include "JXRGlue.h"
#include "TranscodeBridge.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "GuestHeap.h"
#include "DecodeCallbacks.h"
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

_Static_assert(sizeof(uintptr_t) == sizeof(void*), "transcode pointer width");
_Static_assert(offsetof(PKImageEncode, Transcode) == 12 * sizeof(void*), "encoder Transcode offset");
_Static_assert(offsetof(PKImageEncode, guidPixFormat) == 17 * sizeof(void*), "encoder pixel format offset");
_Static_assert(offsetof(PKImageEncode, uWidth) == 19 * sizeof(void*), "encoder width offset");
_Static_assert(offsetof(PKImageEncode, bWMP) == 520, "encoder WMP flag offset");
_Static_assert(offsetof(PKFormatConverter, pDecoder) == 9 * sizeof(void*), "converter decoder offset");
_Static_assert(offsetof(PKImageDecode, WMP) == 19 * sizeof(void*), "decoder WMP offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiI.cROILeftX) == 288, "decoder ROI left offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiI.cROITopY) == 304, "decoder ROI top offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiI.cROIWidth) == 296, "decoder ROI width offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiI.cROIHeight) == 312, "decoder ROI height offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiI.oOrientation) == 344, "decoder orientation offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiSCP.bfBitstreamFormat) == 388, "decoder bitstream offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiSCP.uAlphaMode) == 400, "decoder alpha offset");
_Static_assert(offsetof(PKImageDecode, WMP.wmiSCP.sbSubband) == 404, "decoder subband offset");
_Static_assert(sizeof(((PKImageEncode*)0)->WritePixels) == sizeof(uintptr_t), "encoder callback width");
_Static_assert(sizeof(((PKImageEncode*)0)->Transcode) == sizeof(uintptr_t), "transcode callback width");
_Static_assert(sizeof(((PKFormatConverter*)0)->GetPixelFormat) == sizeof(uintptr_t), "converter callback width");
_Static_assert(sizeof(CWMTranscodingParam) == 56, "transcode parameter size");
_Static_assert(offsetof(CWMTranscodingParam, bfBitstreamFormat) == 32, "transcode bitstream offset");
_Static_assert(offsetof(CWMTranscodingParam, uAlphaMode) == 36, "transcode alpha offset");
_Static_assert(offsetof(CWMTranscodingParam, sbSubband) == 40, "transcode subband offset");
_Static_assert(offsetof(CWMTranscodingParam, oOrientation) == 44, "transcode orientation offset");
_Static_assert(offsetof(CWMTranscodingParam, bIgnoreOverlap) == 48, "transcode overlap offset");
_Static_assert(sizeof(Bool) == 4, "guest Bool width");

typedef struct JxrGuestDecoderTailLayout {
    int32_t hasAlpha;
    int64_t imageOffset;
    int64_t imageByteCount;
    int64_t alphaOffset;
    int64_t alphaByteCount;
    int32_t ignoreOverlap;
    uint64_t decoderCurrMBRow;
} JxrGuestDecoderTailLayout;

_Static_assert(offsetof(JxrGuestDecoderTailLayout, imageOffset) == 8, "guest decoder Long alignment");
_Static_assert(offsetof(JxrGuestDecoderTailLayout, ignoreOverlap) == 40, "guest decoder overlap offset");
_Static_assert(offsetof(JxrGuestDecoderTailLayout, decoderCurrMBRow) == 48, "guest decoder size alignment");

size_t JxrTranscodeEncoderSize_nid_no_patch(void) {
    return offsetof(PKImageEncode, bWMP) + sizeof(Bool);
}

size_t JxrTranscodeConverterSize_nid_no_patch(void) {
    return sizeof(PKFormatConverter);
}

size_t JxrTranscodeDecoderSize_nid_no_patch(void) {
    return offsetof(PKImageDecode, WMP.bHasAlpha) + offsetof(JxrGuestDecoderTailLayout, ignoreOverlap) +
           sizeof(((JxrGuestDecoderTailLayout*)0)->ignoreOverlap);
}

void JxrReadTranscodeEncoder_nid_no_patch(const void* record, JxrTranscodeEncoderFields* fields) {
    const PKImageEncode* encoder = (const PKImageEncode*)record;
    fields->writePixels = (uintptr_t)encoder->WritePixels;
    fields->transcode = (uintptr_t)encoder->Transcode;
    memcpy(fields->pixelFormat, &encoder->guidPixFormat, sizeof(fields->pixelFormat));
    fields->width = encoder->uWidth;
    fields->isWmp = encoder->bWMP;
}

void JxrReadTranscodeConverter_nid_no_patch(const void* record, JxrTranscodeConverterFields* fields) {
    const PKFormatConverter* converter = (const PKFormatConverter*)record;
    fields->getPixelFormat = (uintptr_t)converter->GetPixelFormat;
    fields->getSourcePixelFormat = (uintptr_t)converter->GetSourcePixelFormat;
    fields->copy = (uintptr_t)converter->Copy;
    fields->decoder = (uintptr_t)converter->pDecoder;
}

void JxrReadTranscodeDecoder_nid_no_patch(const void* record, JxrTranscodeDecoderFields* fields) {
    const PKImageDecode* decoder = (const PKImageDecode*)record;
    fields->roiLeft = decoder->WMP.wmiI.cROILeftX;
    fields->roiTop = decoder->WMP.wmiI.cROITopY;
    fields->roiWidth = decoder->WMP.wmiI.cROIWidth;
    fields->roiHeight = decoder->WMP.wmiI.cROIHeight;
    fields->orientation = decoder->WMP.wmiI.oOrientation;
    fields->bitstreamFormat = decoder->WMP.wmiSCP.bfBitstreamFormat;
    fields->alphaMode = decoder->WMP.wmiSCP.uAlphaMode;
    fields->subband = decoder->WMP.wmiSCP.sbSubband;
    const size_t overlapOffset = offsetof(PKImageDecode, WMP.bHasAlpha) +
                                 offsetof(JxrGuestDecoderTailLayout, ignoreOverlap);
    memcpy(&fields->ignoreOverlap, (const uint8_t*)record + overlapOffset, sizeof(fields->ignoreOverlap));
}

int32_t JxrIsTranscodeYuvStrideGuid_nid_no_patch(const void* guid) {
    return memcmp(guid, &GUID_PKPixelFormat12bppYUV420, sizeof(GUID)) == 0 ||
           memcmp(guid, &GUID_PKPixelFormat16bppYUV422, sizeof(GUID)) == 0;
}

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

static size_t JxrDecodeGuestSize(void) {
    const size_t longStart = (offsetof(PKImageDecode, WMP.bHasAlpha) + sizeof(Bool) + 7) & ~(size_t)7;
    const size_t rowStart = (longStart + 4 * sizeof(int64_t) + sizeof(Bool) + 7) & ~(size_t)7;
    return sizeof(PKImageDecode) + rowStart - offsetof(PKImageDecode, WMP.DecoderCurrMBRow);
}

int64_t JxrDecodeCreateGuest_nid_no_patch(void** decoder, const JxrDecodeCallbacks* callbacks) {
    const ERR result = PKAlloc(decoder, JxrDecodeGuestSize());
    if (Failed(result)) return result;

    PKImageDecode* pID = (PKImageDecode*)*decoder;
    pID->Initialize = (ERR (*)(PKImageDecode*, struct WMPStream*))callbacks->initialize;
    pID->GetPixelFormat = (ERR (*)(PKImageDecode*, PKPixelFormatGUID*))callbacks->getPixelFormat;
    pID->GetSize = (ERR (*)(PKImageDecode*, I32*, I32*))callbacks->getSize;
    pID->GetResolution = (ERR (*)(PKImageDecode*, Float*, Float*))callbacks->getResolution;
    pID->GetColorContext = (ERR (*)(PKImageDecode*, U8*, U32*))callbacks->getColorContext;
    pID->GetDescriptiveMetadata = (ERR (*)(PKImageDecode*, DESCRIPTIVEMETADATA*))callbacks->getDescriptiveMetadata;
    pID->Copy = (ERR (*)(PKImageDecode*, const PKRect*, U8*, U32))callbacks->copy;
    pID->GetFrameCount = (ERR (*)(PKImageDecode*, U32*))callbacks->getFrameCount;
    pID->SelectFrame = (ERR (*)(PKImageDecode*, U32))callbacks->selectFrame;
    pID->Release = (ERR (*)(PKImageDecode**))callbacks->release;
    return result;
}

int64_t JxrDecodeCopy_nid_no_patch(void* decoder, const void* rectangle, uint8_t* buffer, uint32_t stride) {
    return PKImageDecode_Copy((PKImageDecode*)decoder, (const PKRect*)rectangle, buffer, stride);
}

int64_t JxrDecodeSelectFrame_nid_no_patch(void* decoder, uint32_t frame) {
    return PKImageDecode_SelectFrame((PKImageDecode*)decoder, frame);
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
