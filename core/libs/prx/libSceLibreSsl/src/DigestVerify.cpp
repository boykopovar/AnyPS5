#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct PkeyHeader {
    std::int32_t type;
    std::int32_t saveType;
    std::int32_t references;
    const void* method;
};
static_assert(offsetof(PkeyHeader, references) == 8);
static_assert(offsetof(PkeyHeader, method) == 16);

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int CommandNotSupported = 147;
constexpr int InvalidOperation = 148;
constexpr int NoOperationSet = 149;
constexpr int OperationNotSupported = 150;
constexpr int OperationNotInitialized = 151;
constexpr int NoDefaultDigest = 158;

constexpr std::int32_t PkeyRsa = 6;
constexpr std::int32_t PkeyRsa2 = 19;
constexpr std::int32_t NidSha1 = 64;
constexpr std::int32_t NidSha1WithRsaEncryption = 65;
constexpr std::uint64_t FlagMethodSignature = 0x4;
constexpr std::uint64_t FlagDigestAlgorithmIdAbsent = 0x8;
constexpr int AutoArgLength = 2;
constexpr std::size_t MaxDigestSize = 64;
constexpr std::ptrdiff_t PointerSize = 8;
constexpr int DefaultKeyBits = 2048;
constexpr int LongestSalt = -2;

constexpr int PaddingPkcs1 = 1;
constexpr int PaddingSslV23 = 2;
constexpr int PaddingNone = 3;
constexpr int PaddingOaep = 4;
constexpr int PaddingX931 = 5;
constexpr int PaddingPss = 6;

constexpr int OperationUndefined = 0;
constexpr int OperationSign = 1 << 3;
constexpr int OperationVerify = 1 << 4;
constexpr int OperationVerifyRecover = 1 << 5;
constexpr int OperationSignContext = 1 << 6;
constexpr int OperationVerifyContext = 1 << 7;
constexpr int OperationTypeSign = OperationSign | OperationVerify | OperationVerifyRecover | OperationSignContext | OperationVerifyContext;

constexpr int ControlMd = 1;
constexpr int ControlDigestInit = 7;

int APS5_VABI EvpSha1Init(EvpMdContext* context) {
    return SHA1_Init_nid_postfix(static_cast<Sha1Context*>(context->mdData));
}

int APS5_VABI EvpSha1Update(EvpMdContext* context, const void* data, std::size_t length) {
    return SHA1_Update_nid_postfix(static_cast<Sha1Context*>(context->mdData), data, length);
}

int APS5_VABI EvpSha1Final(EvpMdContext* context, unsigned char* digest) {
    return SHA1_Final_nid_postfix(digest, static_cast<Sha1Context*>(context->mdData));
}

constexpr EvpMd Sha1 {
    NidSha1, NidSha1WithRsaEncryption, 20, FlagMethodSignature | FlagDigestAlgorithmIdAbsent, EvpSha1Init, EvpSha1Update, EvpSha1Final,
    nullptr, nullptr, nullptr, nullptr, {PkeyRsa, PkeyRsa2, 0, 0, 0}, 64, static_cast<std::int32_t>(PointerSize + sizeof(Sha1Context)), nullptr,
};

static_assert(sizeof(Sha1Context) == 96);
static_assert(offsetof(EvpMd, contextSize) == 104);

void QueueEvp(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason, __FILE__, __LINE__);
}

int APS5_VABI RsaContextInit(PkeyContext* context);
void APS5_VABI RsaContextCleanup(PkeyContext* context);

int APS5_VABI UnavailableSign(PkeyContext*, unsigned char* signature, std::size_t* signatureLength, const unsigned char*, std::size_t) {
    NotImplemented_nid_no_patch("EVP_PKEY_sign with a key context this library created");
    return -2;
}

int APS5_VABI UnavailableVerifyRecover(PkeyContext*, unsigned char* output, std::size_t* outputLength, const unsigned char*, std::size_t) {
    NotImplemented_nid_no_patch("EVP_PKEY_verify_recover with a key context this library created");
    return -2;
}

int APS5_VABI UnavailableEncrypt(PkeyContext*, unsigned char* output, std::size_t* outputLength, const unsigned char*, std::size_t) {
    NotImplemented_nid_no_patch("EVP_PKEY_encrypt with a key context this library created");
    return -2;
}

int APS5_VABI UnavailableDecrypt(PkeyContext*, unsigned char* output, std::size_t* outputLength, const unsigned char*, std::size_t) {
    NotImplemented_nid_no_patch("EVP_PKEY_decrypt with a key context this library created");
    return -2;
}

int APS5_VABI UnavailableKeyGeneration(PkeyContext*, void*) {
    NotImplemented_nid_no_patch("EVP_PKEY_keygen with a key context this library created");
    return -2;
}

int APS5_VABI UnavailableControlString(PkeyContext*, const char*, const char*) {
    NotImplemented_nid_no_patch("EVP_PKEY_CTX_ctrl_str with a key context this library created");
    return -2;
}

const RsaPkeyContext& RequireRsaContextData(const PkeyContext& context, const char* function) {
    if (!context.data) throw std::invalid_argument(std::string(function) + ": an RSA key context without its method data");
    return *static_cast<const RsaPkeyContext*>(context.data);
}

RsaPkeyContext& RequireRsaContextData(PkeyContext& context, const char* function) {
    if (!context.data) throw std::invalid_argument(std::string(function) + ": an RSA key context without its method data");
    return *static_cast<RsaPkeyContext*>(context.data);
}

int APS5_VABI RsaContextCopy(PkeyContext* target, PkeyContext* source) {
    if (!RsaContextInit(target)) return 0;
    const auto& from = RequireRsaContextData(*source, "pkey_rsa_copy");
    auto& to = RequireRsaContextData(*target, "pkey_rsa_copy");
    if (from.publicExponent) NotImplemented_nid_no_patch("pkey_rsa_copy of a key context with a keygen public exponent");
    to.bits = from.bits;
    to.padding = from.padding;
    to.digest = from.digest;
    return 1;
}

LibreSslRsa* KeyOf(const PkeyContext& context, const char* function) {
    if (!context.key) throw std::invalid_argument(std::string(function) + ": a key context without a key");
    return EVP_PKEY_get1_RSA_nid_postfix(context.key);
}

int SetupTemporaryBuffer(RsaPkeyContext& data, const LibreSslRsa& rsa, const char* function) {
    if (data.buffer) return 1;
    const int size = RSA_size_nid_postfix(const_cast<LibreSslRsa*>(&rsa));
    if (size <= 0) throw std::invalid_argument(std::string(function) + ": an RSA key with a non-positive size");
    data.buffer = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(size)));
    return data.buffer ? 1 : 0;
}

int APS5_VABI RsaVerify(PkeyContext* context, const unsigned char* signature, std::size_t signatureLength, const unsigned char* data, std::size_t dataLength) {
    auto& method = RequireRsaContextData(*context, "pkey_rsa_verify");
    auto* rsa = KeyOf(*context, "pkey_rsa_verify");
    if (method.digest) {
        if (method.padding == PaddingPkcs1) {
            const int result = RSA_verify_nid_postfix(method.digest->type, data, static_cast<unsigned int>(dataLength), signature,
                static_cast<unsigned int>(signatureLength), rsa);
            RSA_free_nid_postfix(rsa);
            return result;
        }
        RSA_free_nid_postfix(rsa);
        if (method.padding == PaddingPss) NotImplemented_nid_no_patch("EVP_DigestVerifyFinal with RSA-PSS padding");
        if (method.padding != PaddingX931) return -1;
        NotImplemented_nid_no_patch("EVP_DigestVerifyFinal with RSA X9.31 padding");
    }
    if (!SetupTemporaryBuffer(method, *rsa, "pkey_rsa_verify")) {
        RSA_free_nid_postfix(rsa);
        return -1;
    }
    const int length = RSA_public_decrypt_nid_postfix(static_cast<int>(signatureLength), signature, method.buffer, rsa, method.padding);
    if (length == 0) {
        RSA_free_nid_postfix(rsa);
        return 0;
    }
    const bool same = static_cast<std::size_t>(length) == dataLength && std::memcmp(method.buffer, data, static_cast<std::size_t>(length)) == 0;
    RSA_free_nid_postfix(rsa);
    return same ? 1 : 0;
}

bool CheckPaddingDigest(const EvpMd* digest, int padding) {
    if (!digest) return true;
    if (padding != PaddingPkcs1) NotImplemented_nid_no_patch("pkey_rsa_ctrl with a padding mode other than PKCS#1 v1.5");
    return true;
}

int APS5_VABI RsaControl(PkeyContext* context, int command, int argument, void* pointer) {
    auto& data = RequireRsaContextData(*context, "pkey_rsa_ctrl");
    switch (command) {
    case ControlMd:
        if (!CheckPaddingDigest(static_cast<const EvpMd*>(pointer), data.padding)) return 0;
        data.digest = static_cast<const EvpMd*>(pointer);
        return 1;
    case ControlDigestInit:
        return 1;
    default:
        return -2;
    }
}

int APS5_VABI RsaContextInit(PkeyContext* context) {
    auto* data = static_cast<RsaPkeyContext*>(malloc_nid_postfix(sizeof(RsaPkeyContext)));
    if (!data) return 0;
    std::memset(data, 0, sizeof(*data));
    data->bits = DefaultKeyBits;
    data->padding = PaddingPkcs1;
    data->saltLength = LongestSalt;
    context->data = data;
    context->generationInformation = data->generationInformation;
    context->generationInformationCount = 2;
    return 1;
}

void APS5_VABI RsaContextCleanup(PkeyContext* context) {
    auto* data = static_cast<RsaPkeyContext*>(context->data);
    if (!data) return;
    if (data->publicExponent) NotImplemented_nid_no_patch("pkey_rsa_cleanup of a key context with a keygen public exponent");
    free_nid_postfix(data->buffer);
    free_nid_postfix(data);
    context->data = nullptr;
}

constexpr PkeyMethod RsaMethod {
    .pkeyId = PkeyRsa,
    .flags = AutoArgLength,
    .init = RsaContextInit,
    .copy = RsaContextCopy,
    .cleanup = RsaContextCleanup,
    .keyGeneration = UnavailableKeyGeneration,
    .sign = UnavailableSign,
    .verify = RsaVerify,
    .verifyRecover = UnavailableVerifyRecover,
    .encrypt = UnavailableEncrypt,
    .decrypt = UnavailableDecrypt,
    .control = RsaControl,
    .controlString = UnavailableControlString,
};
static_assert(RsaMethod.verifyContext == nullptr);
static_assert(RsaMethod.verifyContextInit == nullptr);
static_assert(RsaMethod.flags == 2);

void RequireOurMethod(const PkeyContext& context, const char* function) {
    if (context.method != &RsaMethod) NotImplemented_nid_no_patch((std::string(function) + ": a key context this library did not create").c_str());
}

PkeyContext* NewContext(void* key, void* engine) {
    if (!key) return nullptr;
    if (engine) NotImplemented_nid_no_patch("EVP_PKEY_CTX_new with an ENGINE");
    auto& header = *static_cast<PkeyHeader*>(key);
    if (!header.method) return nullptr;
    if (header.type != PkeyRsa) NotImplemented_nid_no_patch("EVP_PKEY_CTX_new for a key that is not RSA");
    auto* context = static_cast<PkeyContext*>(malloc_nid_postfix(sizeof(PkeyContext)));
    if (!context) {
        QueueEvp(MallocFailure);
        return nullptr;
    }
    std::memset(context, 0, sizeof(*context));
    context->method = &RsaMethod;
    context->key = key;
    std::atomic_ref<std::int32_t>(header.references).fetch_add(1, std::memory_order_relaxed);
    if (RsaMethod.init && RsaMethod.init(context) <= 0) {
        PkeyContextFree_nid_no_patch(context);
        return nullptr;
    }
    return context;
}

void FreeContext(PkeyContext* context) {
    if (!context) return;
    RequireOurMethod(*context, "EVP_PKEY_CTX_free");
    if (RsaMethod.cleanup) RsaMethod.cleanup(context);
    EVP_PKEY_free_nid_postfix(context->key);
    EVP_PKEY_free_nid_postfix(context->peerKey);
    free_nid_postfix(context);
}

PkeyContext* DuplicateContext(PkeyContext* context) {
    RequireOurMethod(*context, "EVP_PKEY_CTX_dup");
    auto* copy = static_cast<PkeyContext*>(malloc_nid_postfix(sizeof(PkeyContext)));
    if (!copy) return nullptr;
    std::memset(copy, 0, sizeof(*copy));
    copy->method = context->method;
    copy->engine = context->engine;
    if (context->key) std::atomic_ref<std::int32_t>(static_cast<PkeyHeader*>(context->key)->references).fetch_add(1, std::memory_order_relaxed);
    copy->key = context->key;
    copy->peerKey = context->peerKey;
    copy->operation = context->operation;
    if (context->method->copy(copy, context) > 0) return copy;
    FreeContext(copy);
    return nullptr;
}

int ControlContext(PkeyContext* context, int keyType, int operationType, int command, int argument, void* pointer) {
    if (!context || !context->method || !context->method->control) {
        QueueEvp(CommandNotSupported);
        return -2;
    }
    if (keyType != -1 && context->method->pkeyId != keyType) return -1;
    if (context->operation == OperationUndefined) {
        QueueEvp(NoOperationSet);
        return -1;
    }
    if (operationType != -1 && !(context->operation & operationType)) {
        QueueEvp(InvalidOperation);
        return -1;
    }
    const int result = context->method->control(context, command, argument, pointer);
    if (result == -2) QueueEvp(CommandNotSupported);
    return result;
}

int VerifyWithKey(PkeyContext* context, const unsigned char* signature, std::size_t signatureLength, const unsigned char* data, std::size_t dataLength) {
    if (!context || !context->method || !context->method->verify) {
        QueueEvp(OperationNotSupported);
        return -2;
    }
    if (context->operation != OperationVerify) {
        QueueEvp(OperationNotInitialized);
        return -1;
    }
    return context->method->verify(context, signature, signatureLength, data, dataLength);
}

const EvpMd* DefaultDigest(const PkeyContext& context) {
    if (!context.key) return nullptr;
    if (static_cast<const PkeyHeader*>(context.key)->type != PkeyRsa) return nullptr;
    return &Sha1;
}

} // namespace

extern "C" {

PkeyContext* PkeyContextNew_nid_no_patch(void* key, void* engine) {
    return NewContext(key, engine);
}

PkeyContext* PkeyContextDuplicate_nid_no_patch(PkeyContext* context) {
    return DuplicateContext(context);
}

void PkeyContextFree_nid_no_patch(PkeyContext* context) {
    FreeContext(context);
}

int PkeyContextControl_nid_no_patch(PkeyContext* context, int keyType, int operationType, int command, int argument, void* pointer) {
    return ControlContext(context, keyType, operationType, command, argument, pointer);
}

int APS5_VABI EVP_DigestVerifyInit_nid_postfix(EvpMdContext* context, void** pctx, const EvpMd* type, void* engine, void* pkey) {
    if (!context) throw std::invalid_argument(std::string(__func__) + ": null digest context");
    if (!context->pkeyContext && !pkey) return 0;
    if (engine) NotImplemented_nid_no_patch((std::string(__func__) + ": ENGINE").c_str());
    if (!context->pkeyContext) {
        context->pkeyContext = NewContext(pkey, nullptr);
        if (!context->pkeyContext) return 0;
    }
    auto* pkeyContext = static_cast<PkeyContext*>(context->pkeyContext);
    RequireOurMethod(*pkeyContext, __func__);
    const auto* method = pkeyContext->method;
    const EvpMd* digest = type;
    if (!digest) {
        digest = DefaultDigest(*pkeyContext);
        if (!digest) {
            QueueEvp(NoDefaultDigest);
            return 0;
        }
    }
    if (method->verifyContextInit) {
        if (method->verifyContextInit(pkeyContext, context) <= 0) return 0;
        pkeyContext->operation = OperationVerifyContext;
    } else {
        if (!method->verify) {
            QueueEvp(OperationNotSupported);
            return 0;
        }
        pkeyContext->operation = OperationVerify;
        if (method->verifyInit && method->verifyInit(pkeyContext) <= 0) {
            pkeyContext->operation = OperationUndefined;
            return 0;
        }
    }
    if (ControlContext(pkeyContext, -1, OperationTypeSign, ControlMd, 0, const_cast<EvpMd*>(digest)) <= 0) return 0;
    if (pctx) *pctx = pkeyContext;
    if (!EVP_DigestInit_ex_nid_postfix(context, digest, nullptr)) return 0;
    const int digestInit = ControlContext(pkeyContext, -1, OperationTypeSign, ControlDigestInit, 0, context);
    if (digestInit <= 0 && digestInit != -2) return 0;
    return 1;
}

int APS5_VABI EVP_DigestVerifyFinal_nid_postfix(EvpMdContext* context, unsigned char* signature, std::size_t signatureLength) {
    if (!context || !context->pkeyContext) NotImplemented_nid_no_patch(__func__);
    auto* pkeyContext = static_cast<PkeyContext*>(context->pkeyContext);
    RequireOurMethod(*pkeyContext, __func__);
    EvpMdContext copy {};
    if (!CopyContext_nid_no_patch(copy, *context, __func__)) return -1;
    unsigned char digest[MaxDigestSize];
    std::uint32_t digestLength = 0;
    const int finalized = EVP_DigestFinal_ex_nid_postfix(&copy, digest, &digestLength);
    EVP_MD_CTX_cleanup_nid_postfix(&copy);
    if (!finalized) return finalized;
    return VerifyWithKey(pkeyContext, signature, signatureLength, digest, digestLength);
}

}
