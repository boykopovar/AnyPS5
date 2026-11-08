#include "strcodec.h"
#include "JXRGlue.h"
#include "GuestHeap.h"
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

int32_t JxrAllocateTileInfo_nid_no_patch(void* codec) {
    CWMImageStrCodec* pSC = (CWMImageStrCodec*)codec;
    size_t i;

    if(pSC->WMISCP.cNumOfSliceMinus1V >= MAX_TILES)
        return ICERR_ERROR;
    pSC->pTile = (CWMITile*)JxrGuestMalloc_nid_no_patch((pSC->WMISCP.cNumOfSliceMinus1V + 1) * sizeof(CWMITile));
    if(pSC->pTile == NULL)
        return ICERR_ERROR;
    memset(pSC->pTile, 0, (pSC->WMISCP.cNumOfSliceMinus1V + 1) * sizeof(CWMITile));

    for(i = 0; i <= pSC->WMISCP.cNumOfSliceMinus1V; i ++)
        pSC->pTile[i].cNumQPHP = pSC->pTile[i].cNumQPLP = 1, pSC->pTile[i].cBitsHP = pSC->pTile[i].cBitsLP = 0;

    return ICERR_OK;
}

static void JxrFreeTileQuantizer(CWMIQuantizer* quantizers[MAX_CHANNELS]) {
    if(quantizers[0] != NULL)
        JxrGuestFree_nid_no_patch(quantizers[0]);
}

int32_t JxrFreeTileInfo_nid_no_patch(void* codec)
{
    CWMImageStrCodec* pSC = (CWMImageStrCodec*)codec;
    size_t iTile;
    if(!pSC->pTile) return 0;

    if((pSC->m_param.uQPMode & 1) != 0)
        for(iTile = 0; iTile <= pSC->WMISCP.cNumOfSliceMinus1V; iTile ++)
            JxrFreeTileQuantizer(pSC->pTile[iTile].pQuantizerDC);
    else
        JxrFreeTileQuantizer(pSC->pTile[0].pQuantizerDC);

    if(pSC->WMISCP.sbSubband != SB_DC_ONLY)
        if((pSC->m_param.uQPMode & 2) != 0)
            for(iTile = 0; iTile <= pSC->WMISCP.cNumOfSliceMinus1V; iTile ++)
                JxrFreeTileQuantizer(pSC->pTile[iTile].pQuantizerLP);
        else
            JxrFreeTileQuantizer(pSC->pTile[0].pQuantizerLP);

    if(pSC->WMISCP.sbSubband != SB_DC_ONLY && pSC->WMISCP.sbSubband != SB_NO_HIGHPASS)
        if((pSC->m_param.uQPMode & 4) != 0)
            for(iTile = 0; iTile <= pSC->WMISCP.cNumOfSliceMinus1V; iTile ++)
                JxrFreeTileQuantizer(pSC->pTile[iTile].pQuantizerHP);
        else
            JxrFreeTileQuantizer(pSC->pTile[0].pQuantizerHP);

    if(pSC->pTile != NULL)
        free(pSC->pTile);
    return 1;
}

int32_t JxrAllocateBitIOInfo_nid_no_patch(void* codec) {
    CWMImageStrCodec* pSC = (CWMImageStrCodec*)codec;
    U32 cNumBitIO;
    SUBBAND sbSubband = pSC->WMISCP.sbSubband;

    pSC->cSB = (sbSubband == SB_DC_ONLY ? 1 : (sbSubband == SB_NO_HIGHPASS ? 2 : (sbSubband == SB_NO_FLEXBITS ? 3 : 4)));

    if (!pSC->m_param.bIndexTable) {
        assert(pSC->WMISCP.bfBitstreamFormat == SPATIAL && pSC->WMISCP.cNumOfSliceMinus1H + pSC->WMISCP.cNumOfSliceMinus1V == 0);
        cNumBitIO = 0;
    }
    else if (pSC->WMISCP.bfBitstreamFormat == SPATIAL)
        cNumBitIO = pSC->WMISCP.cNumOfSliceMinus1V + 1;
    else
        cNumBitIO = (pSC->WMISCP.cNumOfSliceMinus1V + 1) * pSC->cSB;

    if (cNumBitIO > MAX_TILES * 4)
        return ICERR_ERROR;

    if (cNumBitIO > 0) {
        U32 i = 0;
        size_t cb = sizeof(BitIOInfo) * cNumBitIO + (PACKETLENGTH * 4 - 1) + PACKETLENGTH * 4 * cNumBitIO;
        U8* pb = (U8*)JxrGuestMalloc_nid_no_patch(cb);

        if (NULL == pb) return ICERR_ERROR;
        memset(pb, 0, cb);

        pSC->m_ppBitIO = (BitIOInfo**)pb;
        pb += sizeof(BitIOInfo) * cNumBitIO;

        pb = (U8*)ALIGNUP(pb, PACKETLENGTH * 4) + PACKETLENGTH * 2;
        for (i = 0; i < cNumBitIO; ++i) {
            pSC->m_ppBitIO[i] = (BitIOInfo*)pb;
            pb += PACKETLENGTH * 4;
        }

        if (cNumBitIO > MAX_TILES * 4 || pSC->WMISCP.cNumOfSliceMinus1H >= MAX_TILES)
            return ICERR_ERROR;
        pSC->pIndexTable = JxrGuestMalloc_nid_no_patch(cNumBitIO * (pSC->WMISCP.cNumOfSliceMinus1H + 1) * sizeof(size_t));
        if (NULL == pSC->pIndexTable) return ICERR_ERROR;
    }

    pSC->cNumBitIO = cNumBitIO;
    return ICERR_OK;
}

int32_t JxrSetBitIOPointers_nid_no_patch(void* codec) {
    CWMImageStrCodec* pSC = (CWMImageStrCodec*)codec;
    if(pSC->cNumBitIO > 0){
        U32 i;

        for(i = 0; i <= pSC->WMISCP.cNumOfSliceMinus1V; i ++){
            CCodingContext * pContext = &pSC->m_pCodingContext[i];
            if(pSC->WMISCP.bfBitstreamFormat == SPATIAL){
                pContext->m_pIODC = pContext->m_pIOLP = pContext->m_pIOAC = pContext->m_pIOFL = pSC->m_ppBitIO[i];
            }
            else{
                U32 j = pSC->cSB;

                pContext->m_pIODC = pSC->m_ppBitIO[i * j];
                if(j > 1)
                    pContext->m_pIOLP = pSC->m_ppBitIO[i * j + 1];
                if(j > 2)
                    pContext->m_pIOAC = pSC->m_ppBitIO[i * j + 2];
                if(j > 3)
                    pContext->m_pIOFL = pSC->m_ppBitIO[i * j + 3];
            }
        }
    }
    else{
        CCodingContext * pContext = &pSC->m_pCodingContext[0];
        pContext->m_pIODC = pContext->m_pIOLP = pContext->m_pIOAC = pContext->m_pIOFL = pSC->pIOHeader;
    }

    return ICERR_OK;
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
