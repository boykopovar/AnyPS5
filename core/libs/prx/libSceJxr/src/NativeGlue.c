#include "strcodec.h"
#include "JXRGlue.h"
#include "GuestHeap.h"
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
