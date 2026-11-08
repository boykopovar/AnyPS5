#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using LibreSsl3::DigestMethod;

constexpr int EvpLibrary = 6;
constexpr int RsaLibrary = 4;
constexpr int AnyFunction = 0xfff;
constexpr int AllocationFailure = 65;

constexpr int ReasonCommandNotSupported = 147;
constexpr int ReasonNoOperationSet = 149;
constexpr int ReasonInvalidOperation = 148;
constexpr int ReasonOperationNotSupported = 150;
constexpr int ReasonOperationNotInitialized = 151;
constexpr int ReasonBufferTooSmall = 155;
constexpr int ReasonUnsupportedAlgorithm = 156;

constexpr int ReasonKeySizeTooSmall = 120;
constexpr int ReasonBadExponentValue = 101;
constexpr int ReasonInvalidPaddingMode = 141;
constexpr int ReasonIllegalPaddingMode = 144;
constexpr int ReasonDigestNotAllowed = 145;
constexpr int ReasonInvalidSaltLength = 146;
constexpr int ReasonKeyTypeOperationNotSupported = 148;
constexpr int ReasonInvalidMaskDigest = 156;
constexpr int ReasonInvalidDigest = 157;
constexpr int ReasonSaltLengthTooSmall = 164;

constexpr int Asn1MethodFlags = 4;
constexpr int DefaultDigestNid = 672;
constexpr int DefaultKeyGenerationBits = 2048;
constexpr int NoSaltLengthLimit = -1;

constexpr int Md4Type = 257;
constexpr int Md5Type = 4;
constexpr int Md5Sha1Type = 114;
constexpr int Ripemd160Type = 117;
constexpr int Sha1Type = 64;
constexpr int Sha224Type = 675;
constexpr int Sha256Type = 672;
constexpr int Sha384Type = 673;
constexpr int Sha512Type = 674;

void QueueEvpError(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason);
}

void QueueRsaError(int reason) {
    LibreSslPutError_nid_no_patch(RsaLibrary, AnyFunction, reason);
}

[[noreturn]] void Reject(const std::string& message) {
    throw std::invalid_argument("libSceLibreSsl3 EVP_PKEY_CTX: " + message);
}

const LibreSsl3PkeyMethod* MethodOf(const LibreSsl3PkeyContext* context) {
    return static_cast<const LibreSsl3PkeyMethod*>(context->method);
}

LibreSsl3RsaKeyContext* RsaContextData(const LibreSsl3PkeyContext* context) {
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(context->data);
    if (!keyContext) NotImplemented_nid_no_patch("EVP_PKEY_CTX operation without RSA context data");
    return keyContext;
}

LibreSsl3Rsa* RsaKeyOf(const LibreSsl3PkeyContext* context) {
    const auto* key = static_cast<const LibreSsl3EvpPkey*>(context->key);
    auto* rsa = key ? static_cast<LibreSsl3Rsa*>(key->key) : nullptr;
    if (!rsa) NotImplemented_nid_no_patch("EVP_PKEY_CTX operation without a native RSA key");
    return rsa;
}

bool CheckPaddingDigest(const DigestMethod* digest, int padding) {
    if (!digest) return true;
    if (padding == LibreSsl3RsaPaddingNone) {
        QueueRsaError(ReasonInvalidPaddingMode);
        return false;
    }
    if (padding == LibreSsl3RsaPaddingX931)
        NotImplemented_nid_no_patch("check_padding_md for X9.31 padding");
    switch (digest->type) {
    case Md4Type:
    case Md5Type:
    case Md5Sha1Type:
    case Ripemd160Type:
    case Sha1Type:
    case Sha224Type:
    case Sha256Type:
    case Sha384Type:
    case Sha512Type:
        return true;
    default:
        QueueRsaError(ReasonInvalidDigest);
        return false;
    }
}

bool SetupTemporaryBuffer(LibreSsl3RsaKeyContext* keyContext, const LibreSsl3Rsa* rsa) {
    if (keyContext->temporaryBuffer) return true;
    const int size = LibreSsl3::RsaByteCount_nid_no_patch(rsa);
    if (size <= 0) return false;
    auto* buffer = static_cast<unsigned char*>(calloc_nid_postfix(1, static_cast<std::size_t>(size)));
    if (!buffer) {
        QueueRsaError(AllocationFailure);
        return false;
    }
    keyContext->temporaryBuffer = buffer;
    return true;
}

int APS5_VABI RsaContextInitialize(LibreSsl3PkeyContext* context) {
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(calloc_nid_postfix(1, sizeof(LibreSsl3RsaKeyContext)));
    if (!keyContext) return 0;
    keyContext->keyBits = DefaultKeyGenerationBits;
    keyContext->padding = MethodOf(context)->keyType == LibreSsl3PkeyRsaPss ? LibreSsl3RsaPaddingPss
        : LibreSsl3RsaPaddingPkcs1;
    keyContext->saltLength = LibreSsl3RsaPssSaltLengthAuto;
    keyContext->minimumSaltLength = NoSaltLengthLimit;
    context->data = keyContext;
    context->generationInformation = keyContext->generationTemporary;
    context->generationInformationCount = 2;
    return 1;
}

int APS5_VABI RsaContextCopy(LibreSsl3PkeyContext* destination, LibreSsl3PkeyContext* source) {
    if (RsaContextInitialize(destination) <= 0) return 0;
    const auto* from = RsaContextData(source);
    auto* to = RsaContextData(destination);
    to->keyBits = from->keyBits;
    if (from->publicExponent) {
        to->publicExponent = LibreSsl3::DuplicateRsaBignum_nid_no_patch(from->publicExponent);
        if (!to->publicExponent) return 0;
    }
    to->padding = from->padding;
    to->digest = from->digest;
    to->maskDigest = from->maskDigest;
    if (from->oaepLabel) {
        to->oaepLabel = static_cast<unsigned char*>(calloc_nid_postfix(1, from->oaepLabelLength));
        if (!to->oaepLabel) return 0;
        std::memcpy(to->oaepLabel, from->oaepLabel, from->oaepLabelLength);
        to->oaepLabelLength = from->oaepLabelLength;
    }
    return 1;
}

void APS5_VABI RsaContextClear(LibreSsl3PkeyContext* context) {
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(context->data);
    if (!keyContext) return;
    if (keyContext->publicExponent) BN_clear_free_nid_postfix(keyContext->publicExponent);
    if (keyContext->temporaryBuffer) free_nid_postfix(keyContext->temporaryBuffer);
    if (keyContext->oaepLabel) free_nid_postfix(keyContext->oaepLabel);
    free_nid_postfix(keyContext);
    context->data = nullptr;
}

int APS5_VABI RsaEncryption(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
    const unsigned char* input, std::size_t inputLength) {
    auto* keyContext = RsaContextData(context);
    auto* rsa = RsaKeyOf(context);
    if (keyContext->padding == LibreSsl3RsaPaddingOaep) {
        const int keyLength = LibreSsl3::RsaByteCount_nid_no_patch(rsa);
        if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
        if (!LibreSsl3::RsaPaddingAddPkcs1OaepMgf1_nid_no_patch(keyContext->temporaryBuffer, keyLength, input,
                static_cast<int>(inputLength), keyContext->oaepLabel, static_cast<int>(keyContext->oaepLabelLength),
                keyContext->digest, keyContext->maskDigest))
            return -1;
        const int result = LibreSsl3::RsaPublicEncrypt_nid_no_patch(keyLength, keyContext->temporaryBuffer, output, rsa,
            LibreSsl3RsaPaddingNone);
        if (result < 0) return result;
        *outputLength = static_cast<std::size_t>(result);
        return 1;
    }
    const int result = LibreSsl3::RsaPublicEncrypt_nid_no_patch(static_cast<int>(inputLength), input, output, rsa,
        keyContext->padding);
    if (result < 0) return result;
    *outputLength = static_cast<std::size_t>(result);
    return 1;
}

int APS5_VABI RsaDecryption(LibreSsl3PkeyContext* context, unsigned char* output, std::size_t* outputLength,
    const unsigned char* input, std::size_t inputLength) {
    auto* keyContext = RsaContextData(context);
    auto* rsa = RsaKeyOf(context);
    if (keyContext->padding == LibreSsl3RsaPaddingOaep) {
        if (!SetupTemporaryBuffer(keyContext, rsa)) return -1;
        const int decrypted = LibreSsl3::RsaPrivateDecrypt_nid_no_patch(static_cast<int>(inputLength), input,
            keyContext->temporaryBuffer, rsa, LibreSsl3RsaPaddingNone);
        if (decrypted <= 0) return decrypted;
        const int recovered = LibreSsl3::RsaPaddingCheckPkcs1OaepMgf1_nid_no_patch(output, decrypted,
            keyContext->temporaryBuffer, decrypted, decrypted, keyContext->oaepLabel,
            static_cast<int>(keyContext->oaepLabelLength), keyContext->digest, keyContext->maskDigest);
        if (recovered < 0) return recovered;
        *outputLength = static_cast<std::size_t>(recovered);
        return 1;
    }
    const int recovered = LibreSsl3::RsaPrivateDecrypt_nid_no_patch(static_cast<int>(inputLength), input, output, rsa,
        keyContext->padding);
    if (recovered < 0) return recovered;
    *outputLength = static_cast<std::size_t>(recovered);
    return 1;
}

int APS5_VABI RsaControl(LibreSsl3PkeyContext* context, int type, int first, void* second) {
    auto* keyContext = RsaContextData(context);
    const auto* method = MethodOf(context);
    switch (type) {
    case LibreSsl3PkeyControlRsaPadding:
        if (first >= LibreSsl3RsaPaddingPkcs1 && first <= LibreSsl3RsaPaddingPss) {
            if (!CheckPaddingDigest(keyContext->digest, first)) return 0;
            if (first == LibreSsl3RsaPaddingPss) {
                if ((context->operation & (LibreSsl3PkeyOperationSign | LibreSsl3PkeyOperationVerify)) == 0)
                    goto badPadding;
                if (!keyContext->digest) keyContext->digest = EVP_sha1_nid_postfix();
            } else if (method->keyType == LibreSsl3PkeyRsaPss) {
                goto badPadding;
            }
            if (first == LibreSsl3RsaPaddingOaep) {
                if ((context->operation & LibreSsl3PkeyOperationTypeCrypt) == 0) goto badPadding;
                if (!keyContext->digest) keyContext->digest = EVP_sha1_nid_postfix();
            }
            keyContext->padding = first;
            return 1;
        }
badPadding:
        QueueRsaError(ReasonIllegalPaddingMode);
        return -2;

    case LibreSsl3PkeyControlGetRsaPadding:
        if (!second) Reject("EVP_PKEY_CTX_ctrl get RSA padding without an output");
        *static_cast<int*>(second) = keyContext->padding;
        return 1;

    case LibreSsl3PkeyControlRsaPssSaltLength:
    case LibreSsl3PkeyControlGetRsaPssSaltLength:
        if (keyContext->padding != LibreSsl3RsaPaddingPss) {
            QueueRsaError(ReasonInvalidSaltLength);
            return -2;
        }
        if (type == LibreSsl3PkeyControlGetRsaPssSaltLength) {
            if (!second) Reject("EVP_PKEY_CTX_ctrl get RSA PSS salt length without an output");
            *static_cast<int*>(second) = keyContext->saltLength;
        } else {
            if (first < LibreSsl3RsaPssSaltLengthMaximum) return -2;
            if (keyContext->minimumSaltLength != NoSaltLengthLimit) {
                if (first == LibreSsl3RsaPssSaltLengthAuto &&
                    context->operation == LibreSsl3PkeyOperationVerify) {
                    QueueRsaError(ReasonInvalidSaltLength);
                    return -2;
                }
                const int digestSize = keyContext->digest ? keyContext->digest->digestSize : 0;
                if ((first == LibreSsl3RsaPssSaltLengthDigest && keyContext->minimumSaltLength > digestSize) ||
                    (first >= 0 && first < keyContext->minimumSaltLength)) {
                    QueueRsaError(ReasonSaltLengthTooSmall);
                    return 0;
                }
            }
            keyContext->saltLength = first;
        }
        return 1;

    case LibreSsl3PkeyControlRsaKeyGenerationBits:
        if (first < LibreSsl3RsaMinimumModulusBits) {
            QueueRsaError(ReasonKeySizeTooSmall);
            return -2;
        }
        keyContext->keyBits = first;
        return 1;

    case LibreSsl3PkeyControlRsaKeyGenerationPublicExponent: {
        auto* exponent = static_cast<LibreSsl3Bignum*>(second);
        const bool odd = exponent != nullptr && exponent->top > 0 && exponent->data != nullptr &&
            (exponent->data[0] & 1U) != 0;
        const bool one = exponent != nullptr && exponent->top == 1 && exponent->data != nullptr &&
            exponent->data[0] == 1;
        if (!odd || one) {
            QueueRsaError(ReasonBadExponentValue);
            return -2;
        }
        if (keyContext->publicExponent) BN_clear_free_nid_postfix(keyContext->publicExponent);
        keyContext->publicExponent = exponent;
        return 1;
    }

    case LibreSsl3PkeyControlRsaOaepDigest:
    case LibreSsl3PkeyControlGetRsaOaepDigest:
        if (keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidPaddingMode);
            return -2;
        }
        if (type == LibreSsl3PkeyControlGetRsaOaepDigest) {
            if (!second) Reject("EVP_PKEY_CTX_ctrl get OAEP digest without an output");
            *static_cast<const DigestMethod**>(second) = keyContext->digest;
        } else {
            keyContext->digest = static_cast<const DigestMethod*>(second);
        }
        return 1;

    case LibreSsl3PkeyControlMessageDigest:
        if (!CheckPaddingDigest(static_cast<const DigestMethod*>(second), keyContext->padding)) return 0;
        if (keyContext->minimumSaltLength != NoSaltLengthLimit) {
            const int restricted = keyContext->digest ? keyContext->digest->type : -1;
            const int requested = second ? static_cast<const DigestMethod*>(second)->type : -1;
            if (restricted == requested) return 1;
            QueueRsaError(ReasonDigestNotAllowed);
            return 0;
        }
        keyContext->digest = static_cast<const DigestMethod*>(second);
        return 1;

    case LibreSsl3PkeyControlGetMessageDigest:
        if (!second) Reject("EVP_PKEY_CTX_ctrl get digest without an output");
        *static_cast<const DigestMethod**>(second) = keyContext->digest;
        return 1;

    case LibreSsl3PkeyControlRsaMaskDigest:
    case LibreSsl3PkeyControlGetRsaMaskDigest:
        if (keyContext->padding != LibreSsl3RsaPaddingPss && keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidMaskDigest);
            return -2;
        }
        if (type == LibreSsl3PkeyControlGetRsaMaskDigest) {
            if (!second) Reject("EVP_PKEY_CTX_ctrl get MGF1 digest without an output");
            *static_cast<const DigestMethod**>(second) = keyContext->maskDigest ? keyContext->maskDigest
                : keyContext->digest;
        } else {
            if (keyContext->minimumSaltLength != NoSaltLengthLimit) {
                const int restricted = keyContext->maskDigest ? keyContext->maskDigest->type : -1;
                const int requested = second ? static_cast<const DigestMethod*>(second)->type : -1;
                if (restricted == requested) return 1;
                QueueRsaError(ReasonInvalidMaskDigest);
                return 0;
            }
            keyContext->maskDigest = static_cast<const DigestMethod*>(second);
        }
        return 1;

    case LibreSsl3PkeyControlRsaOaepLabel:
        if (keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidPaddingMode);
            return -2;
        }
        if (keyContext->oaepLabel) free_nid_postfix(keyContext->oaepLabel);
        if (second && first > 0) {
            keyContext->oaepLabel = static_cast<unsigned char*>(second);
            keyContext->oaepLabelLength = static_cast<std::size_t>(first);
        } else {
            keyContext->oaepLabel = nullptr;
            keyContext->oaepLabelLength = 0;
        }
        return 1;

    case LibreSsl3PkeyControlGetRsaOaepLabel:
        if (keyContext->padding != LibreSsl3RsaPaddingOaep) {
            QueueRsaError(ReasonInvalidPaddingMode);
            return -2;
        }
        if (!second) Reject("EVP_PKEY_CTX_ctrl get OAEP label without an output");
        *static_cast<unsigned char**>(second) = keyContext->oaepLabel;
        return static_cast<int>(keyContext->oaepLabelLength);

    case LibreSsl3PkeyControlDigestInit:
    case LibreSsl3PkeyControlPkcs7Sign:
        return 1;

    case LibreSsl3PkeyControlPkcs7Encrypt:
    case LibreSsl3PkeyControlPkcs7Decrypt:
        if (method->keyType != LibreSsl3PkeyRsaPss) return 1;
        QueueRsaError(ReasonKeyTypeOperationNotSupported);
        return -2;

    case LibreSsl3PkeyControlPeerKey:
        QueueRsaError(ReasonKeyTypeOperationNotSupported);
        return -2;

    default:
        return -2;
    }
}

int APS5_VABI RsaKeySize(const LibreSsl3EvpPkey* key) {
    return LibreSsl3::RsaByteCount_nid_no_patch(static_cast<const LibreSsl3Rsa*>(key->key));
}

int APS5_VABI RsaKeyBits(const LibreSsl3EvpPkey* key) {
    return LibreSsl3::RsaBitCount_nid_no_patch(static_cast<const LibreSsl3Rsa*>(key->key));
}

void APS5_VABI RsaKeyFree(LibreSsl3EvpPkey* key) {
    LibreSsl3::FreeRsa_nid_no_patch(static_cast<LibreSsl3Rsa*>(key->key));
}

int APS5_VABI RsaKeyControl(LibreSsl3EvpPkey* key, int operation, long first, void* second) {
    (void) first;
    if (operation != LibreSsl3Asn1PkeyControlDefaultDigestNid) return -2;
    const auto* rsa = static_cast<const LibreSsl3Rsa*>(key->key);
    if (!rsa) NotImplemented_nid_no_patch("EVP_PKEY_asn1 method control without a native RSA key");
    if (rsa->pss) NotImplemented_nid_no_patch("EVP_PKEY_asn1 method control of a key with PSS restrictions");
    if (!second) Reject("EVP_PKEY_asn1 method control without a digest identifier");
    *static_cast<int*>(second) = DefaultDigestNid;
    return 1;
}

const LibreSsl3PkeyMethod RsaMethod {
    LibreSsl3PkeyRsa,
    LibreSsl3PkeyMethodFlagAutomaticArgumentLength,
    &RsaContextInitialize,
    &RsaContextCopy,
    &RsaContextClear,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaEncryption,
    &RsaDecryption,
    nullptr,
    nullptr,
    &RsaControl,
    nullptr,
    nullptr,
    nullptr,
};

const LibreSsl3PkeyMethod RsaPssMethod {
    LibreSsl3PkeyRsaPss,
    LibreSsl3PkeyMethodFlagAutomaticArgumentLength,
    &RsaContextInitialize,
    &RsaContextCopy,
    &RsaContextClear,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaControl,
    nullptr,
    nullptr,
    nullptr,
};

const LibreSsl3PkeyAsn1Method RsaAsn1Method {
    &RsaAsn1Method,
    LibreSsl3PkeyRsa,
    Asn1MethodFlags,
    "RSA",
    "OpenSSL RSA method",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeySize,
    &RsaKeyBits,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeyFree,
    &RsaKeyControl,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

const LibreSsl3PkeyAsn1Method RsaPssAsn1Method {
    &RsaPssAsn1Method,
    LibreSsl3PkeyRsaPss,
    Asn1MethodFlags,
    "RSA-PSS",
    "OpenSSL RSA-PSS method",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeySize,
    &RsaKeyBits,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &RsaKeyFree,
    &RsaKeyControl,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

} // namespace

const LibreSsl3PkeyMethod* FindNativePkeyMethod_nid_no_patch(std::int32_t keyType) {
    if (keyType == LibreSsl3PkeyRsa) return &RsaMethod;
    if (keyType == LibreSsl3PkeyRsaPss) return &RsaPssMethod;
    return nullptr;
}

const LibreSsl3PkeyAsn1Method* FindNativePkeyAsn1Method_nid_no_patch(std::int32_t keyType) {
    if (keyType == LibreSsl3PkeyRsa) return &RsaAsn1Method;
    if (keyType == LibreSsl3PkeyRsaPss) return &RsaPssAsn1Method;
    return nullptr;
}

bool IsNativePkeyMethod_nid_no_patch(const void* method) {
    return method == &RsaAsn1Method || method == &RsaPssAsn1Method;
}

extern "C" {

LibreSsl3PkeyContext* APS5_VABI EVP_PKEY_CTX_new_nid_postfix(LibreSsl3EvpPkey* key, void* engine) {
    (void) engine;
    if (!key || !key->method) return nullptr;
    const auto* asn1Method = static_cast<const LibreSsl3PkeyAsn1Method*>(key->method);
    const auto* method = FindNativePkeyMethod_nid_no_patch(asn1Method->keyType);
    if (!method) {
        QueueEvpError(ReasonUnsupportedAlgorithm);
        return nullptr;
    }
    auto* context = static_cast<LibreSsl3PkeyContext*>(calloc_nid_postfix(1, sizeof(LibreSsl3PkeyContext)));
    if (!context) {
        QueueEvpError(AllocationFailure);
        return nullptr;
    }
    context->method = method;
    context->operation = LibreSsl3PkeyOperationUndefined;
    if ((context->key = key) != nullptr) EVP_PKEY_up_ref_nid_postfix(key);
    if (method->initialize != nullptr && method->initialize(context) <= 0) {
        EVP_PKEY_CTX_free_nid_postfix(context);
        return nullptr;
    }
    return context;
}

void APS5_VABI EVP_PKEY_CTX_free_nid_postfix(LibreSsl3PkeyContext* context) {
    if (!context) return;
    const auto* method = MethodOf(context);
    if (method != nullptr && method->clear != nullptr) method->clear(context);
    EVP_PKEY_free_nid_disambig1_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->key));
    EVP_PKEY_free_nid_disambig1_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->peerKey));
    free_nid_postfix(context);
}

int APS5_VABI EVP_PKEY_CTX_ctrl_nid_postfix(LibreSsl3PkeyContext* context, int keyType, int operationType, int command,
    int first, void* second) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->control) {
        QueueEvpError(ReasonCommandNotSupported);
        return -2;
    }
    if (keyType != -1 && method->keyType != keyType) return -1;
    if (context->operation == LibreSsl3PkeyOperationUndefined) {
        QueueEvpError(ReasonNoOperationSet);
        return -1;
    }
    if (operationType != -1 && (context->operation & operationType) == 0) {
        QueueEvpError(ReasonInvalidOperation);
        return -1;
    }
    const int result = method->control(context, command, first, second);
    if (result == -2) QueueEvpError(ReasonCommandNotSupported);
    return result;
}

int APS5_VABI EVP_PKEY_encrypt_init_nid_postfix(LibreSsl3PkeyContext* context) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->encryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    context->operation = LibreSsl3PkeyOperationEncrypt;
    return 1;
}

int APS5_VABI EVP_PKEY_encrypt_nid_postfix(LibreSsl3PkeyContext* context, unsigned char* output,
    std::size_t* outputLength, const unsigned char* input, std::size_t inputLength) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->encryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    if (context->operation != LibreSsl3PkeyOperationEncrypt) {
        QueueEvpError(ReasonOperationNotInitialized);
        return -1;
    }
    if (!outputLength) Reject("EVP_PKEY_encrypt without an output length");
    if ((method->flags & LibreSsl3PkeyMethodFlagAutomaticArgumentLength) != 0) {
        const auto keySize = static_cast<std::size_t>(EVP_PKEY_size_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->key)));
        if (!output) {
            *outputLength = keySize;
            return 1;
        }
        if (*outputLength < keySize) {
            QueueEvpError(ReasonBufferTooSmall);
            return 0;
        }
    }
    return method->encryption(context, output, outputLength, input, inputLength);
}

int APS5_VABI EVP_PKEY_decrypt_init_nid_postfix(LibreSsl3PkeyContext* context) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->decryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    context->operation = LibreSsl3PkeyOperationDecrypt;
    return 1;
}

int APS5_VABI EVP_PKEY_decrypt_nid_postfix(LibreSsl3PkeyContext* context, unsigned char* output,
    std::size_t* outputLength, const unsigned char* input, std::size_t inputLength) {
    const auto* method = context ? MethodOf(context) : nullptr;
    if (!context || !method || !method->decryption) {
        QueueEvpError(ReasonOperationNotSupported);
        return -2;
    }
    if (context->operation != LibreSsl3PkeyOperationDecrypt) {
        QueueEvpError(ReasonOperationNotInitialized);
        return -1;
    }
    if (!outputLength) Reject("EVP_PKEY_decrypt without an output length");
    if ((method->flags & LibreSsl3PkeyMethodFlagAutomaticArgumentLength) != 0) {
        const auto keySize = static_cast<std::size_t>(EVP_PKEY_size_nid_postfix(static_cast<LibreSsl3EvpPkey*>(context->key)));
        if (!output) {
            *outputLength = keySize;
            return 1;
        }
        if (*outputLength < keySize) {
            QueueEvpError(ReasonBufferTooSmall);
            return 0;
        }
    }
    return method->decryption(context, output, outputLength, input, inputLength);
}

}
