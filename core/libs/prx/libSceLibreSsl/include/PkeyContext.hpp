#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_PKEYCONTEXT_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_PKEYCONTEXT_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libSceLibreSsl/include/Api.hpp"

struct PkeyContext;

struct PkeyMethod {
    std::int32_t pkeyId;
    std::int32_t flags;
    int (APS5_VABI* init)(PkeyContext* context);
    int (APS5_VABI* copy)(PkeyContext* target, PkeyContext* source);
    void (APS5_VABI* cleanup)(PkeyContext* context);
    int (APS5_VABI* parameterGenerationInit)(PkeyContext* context);
    int (APS5_VABI* parameterGeneration)(PkeyContext* context, void* key);
    int (APS5_VABI* keyGenerationInit)(PkeyContext* context);
    int (APS5_VABI* keyGeneration)(PkeyContext* context, void* key);
    int (APS5_VABI* signInit)(PkeyContext* context);
    int (APS5_VABI* sign)(PkeyContext* context, unsigned char* signature, std::size_t* signatureLength, const unsigned char* data,
        std::size_t dataLength);
    int (APS5_VABI* verifyInit)(PkeyContext* context);
    int (APS5_VABI* verify)(PkeyContext* context, const unsigned char* signature, std::size_t signatureLength, const unsigned char* data,
        std::size_t dataLength);
    int (APS5_VABI* verifyRecoverInit)(PkeyContext* context);
    int (APS5_VABI* verifyRecover)(PkeyContext* context, unsigned char* output, std::size_t* outputLength, const unsigned char* signature,
        std::size_t signatureLength);
    int (APS5_VABI* signContextInit)(PkeyContext* context, EvpMdContext* digestContext);
    int (APS5_VABI* signContext)(PkeyContext* context, unsigned char* signature, std::size_t* signatureLength, EvpMdContext* digestContext);
    int (APS5_VABI* verifyContextInit)(PkeyContext* context, EvpMdContext* digestContext);
    int (APS5_VABI* verifyContext)(PkeyContext* context, const unsigned char* signature, int signatureLength, EvpMdContext* digestContext);
    int (APS5_VABI* encryptInit)(PkeyContext* context);
    int (APS5_VABI* encrypt)(PkeyContext* context, unsigned char* output, std::size_t* outputLength, const unsigned char* input,
        std::size_t inputLength);
    int (APS5_VABI* decryptInit)(PkeyContext* context);
    int (APS5_VABI* decrypt)(PkeyContext* context, unsigned char* output, std::size_t* outputLength, const unsigned char* input,
        std::size_t inputLength);
    int (APS5_VABI* deriveInit)(PkeyContext* context);
    int (APS5_VABI* derive)(PkeyContext* context, unsigned char* key, std::size_t* keyLength);
    int (APS5_VABI* control)(PkeyContext* context, int command, int argument, void* pointer);
    int (APS5_VABI* controlString)(PkeyContext* context, const char* name, const char* value);
};

struct PkeyContext {
    const PkeyMethod* method;
    void* engine;
    void* key;
    void* peerKey;
    int operation;
    void* data;
    void* applicationData;
    void* generationCallback;
    int* generationInformation;
    int generationInformationCount;
};

struct RsaPkeyContext {
    int bits;
    void* publicExponent;
    int generationInformation[2];
    int padding;
    const EvpMd* digest;
    const EvpMd* maskDigest;
    int saltLength;
    unsigned char* buffer;
};

static_assert(sizeof(PkeyMethod) == 208);
static_assert(offsetof(PkeyMethod, pkeyId) == 0);
static_assert(offsetof(PkeyMethod, flags) == 4);
static_assert(offsetof(PkeyMethod, init) == 8);
static_assert(offsetof(PkeyMethod, sign) == 72);
static_assert(offsetof(PkeyMethod, verify) == 88);
static_assert(offsetof(PkeyMethod, signContext) == 120);
static_assert(offsetof(PkeyMethod, verifyContextInit) == 128);
static_assert(offsetof(PkeyMethod, verifyContext) == 136);
static_assert(offsetof(PkeyMethod, control) == 192);
static_assert(offsetof(PkeyMethod, controlString) == 200);
static_assert(sizeof(PkeyContext) == 80);
static_assert(offsetof(PkeyContext, method) == 0);
static_assert(offsetof(PkeyContext, engine) == 8);
static_assert(offsetof(PkeyContext, key) == 16);
static_assert(offsetof(PkeyContext, peerKey) == 24);
static_assert(offsetof(PkeyContext, operation) == 32);
static_assert(offsetof(PkeyContext, data) == 40);
static_assert(offsetof(PkeyContext, applicationData) == 48);
static_assert(offsetof(PkeyContext, generationCallback) == 56);
static_assert(offsetof(PkeyContext, generationInformation) == 64);
static_assert(offsetof(PkeyContext, generationInformationCount) == 72);
static_assert(sizeof(RsaPkeyContext) == 64);
static_assert(offsetof(RsaPkeyContext, bits) == 0);
static_assert(offsetof(RsaPkeyContext, publicExponent) == 8);
static_assert(offsetof(RsaPkeyContext, generationInformation) == 16);
static_assert(offsetof(RsaPkeyContext, padding) == 24);
static_assert(offsetof(RsaPkeyContext, digest) == 32);
static_assert(offsetof(RsaPkeyContext, maskDigest) == 40);
static_assert(offsetof(RsaPkeyContext, saltLength) == 48);
static_assert(offsetof(RsaPkeyContext, buffer) == 56);

extern "C" {

PkeyContext* PkeyContextNew_nid_no_patch(void* key, void* engine);
PkeyContext* PkeyContextDuplicate_nid_no_patch(PkeyContext* context);
void PkeyContextFree_nid_no_patch(PkeyContext* context);
int PkeyContextControl_nid_no_patch(PkeyContext* context, int keyType, int operationType, int command, int argument, void* pointer);

}

#endif
