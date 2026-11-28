#include "strcodec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

Void FreeCodingContextDec(CWMImageStrCodec* pSC);

size_t JxrContextDecCodecBytes_nid_no_patch(void) {
    return sizeof(CWMImageStrCodec);
}

void* JxrContextDecMake_nid_no_patch(uint32_t seed, int32_t format) {
    CWMImageStrCodec* codec = (CWMImageStrCodec*)malloc(sizeof(CWMImageStrCodec));
    unsigned char* bytes = (unsigned char*)codec;
    size_t i;
    if (codec == NULL) return NULL;
    for (i = 0; i < sizeof(CWMImageStrCodec); ++i) {
        seed = seed * 1664525u + 1013904223u;
        bytes[i] = (unsigned char)(seed >> 8);
    }
    codec->m_param.cfColorFormat = (COLORFORMAT)format;
    codec->m_pCodingContext = NULL;
    codec->cNumCodingContext = 0;
    return codec;
}

void* JxrContextDecClone_nid_no_patch(const void* source) {
    void* codec = malloc(sizeof(CWMImageStrCodec));
    if (codec != NULL) memcpy(codec, source, sizeof(CWMImageStrCodec));
    return codec;
}

void JxrContextDecRelease_nid_no_patch(void* codec) {
    if (codec == NULL) return;
    FreeCodingContextDec((CWMImageStrCodec*)codec);
    free(codec);
}

int32_t JxrContextDecReference_nid_no_patch(void* codec, int32_t count) {
    return AllocateCodingContextDec((CWMImageStrCodec*)codec, count);
}

size_t JxrContextDecCount_nid_no_patch(const void* codec) {
    return ((const CWMImageStrCodec*)codec)->cNumCodingContext;
}

static int SameTable(const void* left, const void* right) {
    return (left == NULL) == (right == NULL);
}

static int SameHuffman(const CAdaptiveHuffman* left, const CAdaptiveHuffman* right) {
    if (left == NULL || right == NULL || left == right) return 0;
    return left->m_iNSymbols == right->m_iNSymbols && left->m_iTableIndex == right->m_iTableIndex &&
           left->m_bInitialize == right->m_bInitialize && left->m_iDiscriminant == right->m_iDiscriminant &&
           left->m_iDiscriminant1 == right->m_iDiscriminant1 && left->m_iUpperBound == right->m_iUpperBound &&
           left->m_iLowerBound == right->m_iLowerBound && SameTable(left->m_pTable, right->m_pTable) &&
           SameTable(left->m_pDelta, right->m_pDelta) && SameTable(left->m_pDelta1, right->m_pDelta1) &&
           SameTable(left->m_hufDecTable, right->m_hufDecTable);
}

static int SameContext(const CCodingContext* left, const CCodingContext* right) {
    CCodingContext copy;
    size_t k;
    if (!SameHuffman(left->m_pAdaptHuffCBPCY, right->m_pAdaptHuffCBPCY)) return 0;
    if (!SameHuffman(left->m_pAdaptHuffCBPCY1, right->m_pAdaptHuffCBPCY1)) return 0;
    for (k = 0; k < NUMVLCTABLES; ++k)
        if (!SameHuffman(left->m_pAHexpt[k], right->m_pAHexpt[k])) return 0;
    memcpy(&copy, left, sizeof(CCodingContext));
    copy.m_pAdaptHuffCBPCY = right->m_pAdaptHuffCBPCY;
    copy.m_pAdaptHuffCBPCY1 = right->m_pAdaptHuffCBPCY1;
    for (k = 0; k < NUMVLCTABLES; ++k) copy.m_pAHexpt[k] = right->m_pAHexpt[k];
    return memcmp(&copy, right, sizeof(CCodingContext)) == 0;
}

int32_t JxrContextDecEqual_nid_no_patch(const void* guest, const void* native) {
    const CWMImageStrCodec* left = (const CWMImageStrCodec*)guest;
    const CWMImageStrCodec* right = (const CWMImageStrCodec*)native;
    CWMImageStrCodec* copy;
    size_t i;
    int32_t same;
    if ((left->m_pCodingContext == NULL) != (right->m_pCodingContext == NULL)) return 0;
    if (left->m_pCodingContext != NULL && left->m_pCodingContext == right->m_pCodingContext) return 0;
    copy = (CWMImageStrCodec*)malloc(sizeof(CWMImageStrCodec));
    if (copy == NULL) return 0;
    memcpy(copy, left, sizeof(CWMImageStrCodec));
    copy->m_pCodingContext = right->m_pCodingContext;
    same = memcmp(copy, right, sizeof(CWMImageStrCodec)) == 0;
    free(copy);
    if (left->m_pCodingContext == NULL) return same;
    for (i = 0; same && i < left->cNumCodingContext; ++i)
        same = SameContext(&left->m_pCodingContext[i], &right->m_pCodingContext[i]);
    return same;
}
