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
