#include "strcodec.h"
#include "JXRGlue.h"
#include "GuestHeap.h"
#include "DecodeCallbacks.h"
#include <stdint.h>

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

int32_t JxrDecodeHasGetPosGuest_nid_no_patch(void* stream) {
    return ((struct WMPStream*)stream)->GetPos != NULL;
}

int32_t JxrDecodeHasCloseGuest_nid_no_patch(void* decoder) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    return !pID->fStreamOwner || (pID->pStream != NULL && pID->pStream->Close != NULL);
}

int64_t JxrDecodeInitializeNative_nid_no_patch(void* decoder, void* stream) {
    PKImageDecode* pID = (PKImageDecode*)decoder;
    struct WMPStream* pStream = (struct WMPStream*)stream;
    typedef int64_t (APS5_VABI *JxrGuestGetPos)(struct WMPStream*, size_t*);
    int64_t err = WMP_errSuccess;

    pID->pStream = pStream;
    pID->guidPixFormat = GUID_PKPixelFormatDontCare;
    pID->fResX = 96;
    pID->fResY = 96;
    pID->cFrame = 1;

    err = ((JxrGuestGetPos)pStream->GetPos)(pStream, &pID->offStart);
    if (!Failed(err)) memset(&pID->WMP.wmiDEMisc, 0, sizeof(pID->WMP.wmiDEMisc));
    return WMP_errSuccess;
}

int64_t JxrDecodeGetPixelFormatNative_nid_no_patch(void* decoder, void* format) {
    return PKImageDecode_GetPixelFormat((PKImageDecode*)decoder, (PKPixelFormatGUID*)format);
}

int64_t JxrDecodeGetSizeNative_nid_no_patch(void* decoder, int32_t* width, int32_t* height) {
    return PKImageDecode_GetSize((PKImageDecode*)decoder, (I32*)width, (I32*)height);
}

int64_t JxrDecodeGetResolutionNative_nid_no_patch(void* decoder, float* x, float* y) {
    return PKImageDecode_GetResolution((PKImageDecode*)decoder, x, y);
}

int64_t JxrDecodeGetColorContextNative_nid_no_patch(void* decoder, uint8_t* buffer, uint32_t* size) {
    return PKImageDecode_GetColorContext((PKImageDecode*)decoder, buffer, size);
}

int64_t JxrDecodeGetDescriptiveMetadataNative_nid_no_patch(void* decoder, void* metadata) {
    return PKImageDecode_GetDescriptiveMetadata((PKImageDecode*)decoder, (DESCRIPTIVEMETADATA*)metadata);
}

int64_t JxrDecodeCopyNative_nid_no_patch(void* decoder, const void* rectangle, uint8_t* buffer, uint32_t stride) {
    return PKImageDecode_Copy((PKImageDecode*)decoder, (const PKRect*)rectangle, buffer, stride);
}

int64_t JxrDecodeGetFrameCountNative_nid_no_patch(void* decoder, uint32_t* count) {
    return PKImageDecode_GetFrameCount((PKImageDecode*)decoder, count);
}

int64_t JxrDecodeSelectFrameNative_nid_no_patch(void* decoder, uint32_t frame) {
    return PKImageDecode_SelectFrame((PKImageDecode*)decoder, frame);
}

int64_t JxrDecodeReleaseNative_nid_no_patch(void** decoder) {
    PKImageDecode* pID = (PKImageDecode*)*decoder;
    if (pID->fStreamOwner) {
        typedef int64_t (APS5_VABI *JxrGuestClose)(struct WMPStream**);
        ((JxrGuestClose)pID->pStream->Close)(&pID->pStream);
    }
    return PKFree((void**)decoder);
}
