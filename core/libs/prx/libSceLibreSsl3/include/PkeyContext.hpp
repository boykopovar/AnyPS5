#pragma once

#include <cstddef>
#include <cstdint>

struct LibreSsl3PkeyContext {
    const void* method;
    void* key;
    void* peerKey;
    std::int32_t operation;
    void* data;
    void* applicationData;
    void* generationCallback;
    void* generationInformation;
    std::int32_t generationInformationCount;
};

static_assert(sizeof(LibreSsl3PkeyContext) == 72);
static_assert(offsetof(LibreSsl3PkeyContext, method) == 0);
static_assert(offsetof(LibreSsl3PkeyContext, key) == 8);
static_assert(offsetof(LibreSsl3PkeyContext, peerKey) == 16);
static_assert(offsetof(LibreSsl3PkeyContext, operation) == 24);
static_assert(offsetof(LibreSsl3PkeyContext, data) == 32);
static_assert(offsetof(LibreSsl3PkeyContext, applicationData) == 40);
static_assert(offsetof(LibreSsl3PkeyContext, generationCallback) == 48);
static_assert(offsetof(LibreSsl3PkeyContext, generationInformation) == 56);
static_assert(offsetof(LibreSsl3PkeyContext, generationInformationCount) == 64);

inline constexpr std::int32_t LibreSsl3PkeyRsa = 6;
inline constexpr std::int32_t LibreSsl3PkeyRsaPss = 912;
inline constexpr std::int32_t LibreSsl3PkeyEc = 408;
inline constexpr std::int32_t LibreSsl3PkeyHmac = 855;

inline constexpr std::int32_t LibreSsl3PkeyOperationUndefined = 0;
inline constexpr std::int32_t LibreSsl3PkeyOperationParameterGeneration = 1 << 1;
inline constexpr std::int32_t LibreSsl3PkeyOperationKeyGeneration = 1 << 2;
inline constexpr std::int32_t LibreSsl3PkeyOperationSign = 1 << 3;
inline constexpr std::int32_t LibreSsl3PkeyOperationVerify = 1 << 4;
inline constexpr std::int32_t LibreSsl3PkeyOperationVerifyRecover = 1 << 5;
inline constexpr std::int32_t LibreSsl3PkeyOperationSignContext = 1 << 6;
inline constexpr std::int32_t LibreSsl3PkeyOperationVerifyContext = 1 << 7;
inline constexpr std::int32_t LibreSsl3PkeyOperationEncrypt = 1 << 8;
inline constexpr std::int32_t LibreSsl3PkeyOperationDecrypt = 1 << 9;
inline constexpr std::int32_t LibreSsl3PkeyOperationDerive = 1 << 10;

inline constexpr std::int32_t LibreSsl3PkeyOperationTypeSignature =
    LibreSsl3PkeyOperationSign | LibreSsl3PkeyOperationVerify | LibreSsl3PkeyOperationVerifyRecover |
    LibreSsl3PkeyOperationSignContext | LibreSsl3PkeyOperationVerifyContext;
inline constexpr std::int32_t LibreSsl3PkeyOperationTypeCrypt =
    LibreSsl3PkeyOperationEncrypt | LibreSsl3PkeyOperationDecrypt;

inline constexpr std::int32_t LibreSsl3PkeyMethodFlagAutomaticArgumentLength = 2;
inline constexpr std::int32_t LibreSsl3PkeyMethodFlagSignatureContextCustom = 4;

inline constexpr std::int32_t LibreSsl3PkeyControlMessageDigest = 1;
inline constexpr std::int32_t LibreSsl3PkeyControlPeerKey = 2;
inline constexpr std::int32_t LibreSsl3PkeyControlPkcs7Encrypt = 3;
inline constexpr std::int32_t LibreSsl3PkeyControlPkcs7Decrypt = 4;
inline constexpr std::int32_t LibreSsl3PkeyControlPkcs7Sign = 5;
inline constexpr std::int32_t LibreSsl3PkeyControlSetMacKey = 6;
inline constexpr std::int32_t LibreSsl3PkeyControlDigestInit = 7;
inline constexpr std::int32_t LibreSsl3PkeyControlCipher = 12;
inline constexpr std::int32_t LibreSsl3PkeyControlGetMessageDigest = 13;

inline constexpr std::int32_t LibreSsl3PkeyControlAlgorithm = 0x1000;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaPadding = LibreSsl3PkeyControlAlgorithm + 1;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaPssSaltLength = LibreSsl3PkeyControlAlgorithm + 2;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaKeyGenerationBits = LibreSsl3PkeyControlAlgorithm + 3;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaKeyGenerationPublicExponent = LibreSsl3PkeyControlAlgorithm + 4;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaMaskDigest = LibreSsl3PkeyControlAlgorithm + 5;
inline constexpr std::int32_t LibreSsl3PkeyControlGetRsaPadding = LibreSsl3PkeyControlAlgorithm + 6;
inline constexpr std::int32_t LibreSsl3PkeyControlGetRsaPssSaltLength = LibreSsl3PkeyControlAlgorithm + 7;
inline constexpr std::int32_t LibreSsl3PkeyControlGetRsaMaskDigest = LibreSsl3PkeyControlAlgorithm + 8;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaOaepDigest = LibreSsl3PkeyControlAlgorithm + 9;
inline constexpr std::int32_t LibreSsl3PkeyControlRsaOaepLabel = LibreSsl3PkeyControlAlgorithm + 10;
inline constexpr std::int32_t LibreSsl3PkeyControlGetRsaOaepDigest = LibreSsl3PkeyControlAlgorithm + 11;
inline constexpr std::int32_t LibreSsl3PkeyControlGetRsaOaepLabel = LibreSsl3PkeyControlAlgorithm + 12;

inline constexpr std::int32_t LibreSsl3Asn1PkeyControlDefaultDigestNid = 0x3;
