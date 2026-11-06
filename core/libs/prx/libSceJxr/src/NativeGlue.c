#include "strcodec.h"
#include "JXRGlue.h"
#include "TranscodeBridge.h"
#include <stdint.h>
#include <stddef.h>
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
