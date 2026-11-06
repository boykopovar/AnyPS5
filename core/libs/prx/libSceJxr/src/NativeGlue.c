#include "strcodec.h"
#include "JXRGlue.h"
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
