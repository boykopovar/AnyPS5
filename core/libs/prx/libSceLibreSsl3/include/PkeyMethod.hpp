#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl3/include/EvpDigest.hpp"
#include "prx/libSceLibreSsl3/include/Key.hpp"
#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"

struct LibreSsl3PkeyMethod {
    std::int32_t keyType;
    std::int32_t flags;
    int (APS5_VABI* initialize)(LibreSsl3PkeyContext* context);
    int (APS5_VABI* copy)(LibreSsl3PkeyContext* destination, LibreSsl3PkeyContext* source);
    void (APS5_VABI* clear)(LibreSsl3PkeyContext* context);
    int (APS5_VABI* parameterGeneration)(LibreSsl3PkeyContext* context, LibreSsl3EvpPkey* key);
    int (APS5_VABI* keyGeneration)(LibreSsl3PkeyContext* context, LibreSsl3EvpPkey* key);
    int (APS5_VABI* signatureInitialize)(LibreSsl3PkeyContext* context);
    int (APS5_VABI* signature)(LibreSsl3PkeyContext* context, unsigned char* signature, std::size_t* signatureLength,
        const unsigned char* content, std::size_t contentLength);
    int (APS5_VABI* verificationInitialize)(LibreSsl3PkeyContext* context);
    int (APS5_VABI* verification)(LibreSsl3PkeyContext* context, const unsigned char* signature, std::size_t signatureLength,
        const unsigned char* content, std::size_t contentLength);
    int (APS5_VABI* verificationRecovery)(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
        const unsigned char* signature, std::size_t signatureLength);
    int (APS5_VABI* signatureContextInitialize)(LibreSsl3PkeyContext* context, LibreSsl3::DigestContext* digestContext);
    int (APS5_VABI* signatureContext)(LibreSsl3PkeyContext* context, unsigned char* signature, std::size_t* signatureLength,
        LibreSsl3::DigestContext* digestContext);
    int (APS5_VABI* encryption)(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
        const unsigned char* input, std::size_t inputLength);
    int (APS5_VABI* decryption)(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
        const unsigned char* input, std::size_t inputLength);
    int (APS5_VABI* derivationInitialize)(LibreSsl3PkeyContext* context);
    int (APS5_VABI* derivation)(LibreSsl3PkeyContext* context, unsigned char* key, std::size_t* keyLength);
    int (APS5_VABI* control)(LibreSsl3PkeyContext* context, int type, int first, void* second);
    int (APS5_VABI* controlString)(LibreSsl3PkeyContext* context, const char* name, const char* value);
    int (APS5_VABI* digestSignature)(LibreSsl3::DigestContext* digestContext, unsigned char* signature,
        std::size_t* signatureLength, const unsigned char* content, std::size_t contentLength);
    int (APS5_VABI* digestVerification)(LibreSsl3::DigestContext* digestContext, const unsigned char* signature,
        std::size_t signatureLength, const unsigned char* content, std::size_t contentLength);
};

static_assert(sizeof(LibreSsl3PkeyMethod) == 168);
static_assert(offsetof(LibreSsl3PkeyMethod, keyType) == 0);
static_assert(offsetof(LibreSsl3PkeyMethod, flags) == 4);
static_assert(offsetof(LibreSsl3PkeyMethod, initialize) == 8);
static_assert(offsetof(LibreSsl3PkeyMethod, copy) == 16);
static_assert(offsetof(LibreSsl3PkeyMethod, clear) == 24);
static_assert(offsetof(LibreSsl3PkeyMethod, parameterGeneration) == 32);
static_assert(offsetof(LibreSsl3PkeyMethod, keyGeneration) == 40);
static_assert(offsetof(LibreSsl3PkeyMethod, signatureInitialize) == 48);
static_assert(offsetof(LibreSsl3PkeyMethod, signature) == 56);
static_assert(offsetof(LibreSsl3PkeyMethod, verificationInitialize) == 64);
static_assert(offsetof(LibreSsl3PkeyMethod, verification) == 72);
static_assert(offsetof(LibreSsl3PkeyMethod, verificationRecovery) == 80);
static_assert(offsetof(LibreSsl3PkeyMethod, signatureContextInitialize) == 88);
static_assert(offsetof(LibreSsl3PkeyMethod, signatureContext) == 96);
static_assert(offsetof(LibreSsl3PkeyMethod, encryption) == 104);
static_assert(offsetof(LibreSsl3PkeyMethod, decryption) == 112);
static_assert(offsetof(LibreSsl3PkeyMethod, derivationInitialize) == 120);
static_assert(offsetof(LibreSsl3PkeyMethod, derivation) == 128);
static_assert(offsetof(LibreSsl3PkeyMethod, control) == 136);
static_assert(offsetof(LibreSsl3PkeyMethod, controlString) == 144);
static_assert(offsetof(LibreSsl3PkeyMethod, digestSignature) == 152);
static_assert(offsetof(LibreSsl3PkeyMethod, digestVerification) == 160);

struct LibreSsl3PkeyAsn1Method {
    const LibreSsl3PkeyAsn1Method* baseMethod;
    std::int32_t keyType;
    std::uint64_t flags;
    const char* pemString;
    const char* information;
    int (APS5_VABI* publicKeyDecode)(LibreSsl3EvpPkey* key, LibreSsl3X509Pubkey* publicKey);
    void* publicKeyEncode;
    int (APS5_VABI* publicKeyCompare)(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second);
    void* publicKeyPrint;
    int (APS5_VABI* privateKeyDecode)(LibreSsl3EvpPkey* key, const LibreSsl3Pkcs8PrivKeyInfo* information);
    void* privateKeyEncode;
    void* privateKeyPrint;
    int (APS5_VABI* keySize)(const LibreSsl3EvpPkey* key);
    int (APS5_VABI* keyBits)(const LibreSsl3EvpPkey* key);
    void* keySecurityBits;
    void* signatureInformation;
    void* parameterDecode;
    void* parameterEncode;
    void* parameterMissing;
    void* parameterCopy;
    int (APS5_VABI* parameterCompare)(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second);
    void* parameterPrint;
    void* signaturePrint;
    void (APS5_VABI* keyFree)(LibreSsl3EvpPkey* key);
    int (APS5_VABI* keyControl)(LibreSsl3EvpPkey* key, int operation, long first, void* second);
    int (APS5_VABI* oldPrivateKeyDecode)(LibreSsl3EvpPkey* key, const unsigned char** data, int length);
    void* oldPrivateKeyEncode;
    void* itemVerify;
    void* itemSign;
    void* setPrivateKey;
    void* setPublicKey;
    void* getPrivateKey;
    void* getPublicKey;
};

static_assert(sizeof(LibreSsl3PkeyAsn1Method) == 264);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, baseMethod) == 0);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, keyType) == 8);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, flags) == 16);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, pemString) == 24);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, information) == 32);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, publicKeyDecode) == 40);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, publicKeyCompare) == 56);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, privateKeyDecode) == 72);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, keySize) == 96);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, keyBits) == 104);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, keySecurityBits) == 112);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, parameterCompare) == 160);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, keyFree) == 184);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, keyControl) == 192);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, oldPrivateKeyDecode) == 200);
static_assert(offsetof(LibreSsl3PkeyAsn1Method, getPublicKey) == 256);

const LibreSsl3PkeyMethod* FindNativePkeyMethod_nid_no_patch(std::int32_t keyType);
const LibreSsl3PkeyAsn1Method* FindNativePkeyAsn1Method_nid_no_patch(std::int32_t keyType);
