#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceNKWebKit/include/PalCryptoDigestExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>

namespace PAL {

struct CryptoDigestContext {
    CryptoDigest::Algorithm algorithm;
    union {
        Sha1Context sha1;
        Sha256Context sha256;
        Sha512Context sha512;
    };
};

}  // namespace PAL

namespace {

using PAL::CryptoDigest;
using PAL::CryptoDigestContext;
using Algorithm = PAL::CryptoDigest::Algorithm;

void Require(int result, const char* function) {
    if (result != 1) throw std::runtime_error(std::string(function) + ": digest operation failed");
}

CryptoDigestContext& CheckedContext(CryptoDigest* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + ": null object");
    if (!self->m_context) throw std::invalid_argument(std::string(function) + ": null context");
    return *self->m_context;
}

bool IsKnown(Algorithm algorithm) {
    switch (algorithm) {
    case Algorithm::SHA_1:
    case Algorithm::SHA_224:
    case Algorithm::SHA_256:
    case Algorithm::SHA_384:
    case Algorithm::SHA_512:
        return true;
    }
    return false;
}

std::size_t DigestLength(Algorithm algorithm) {
    switch (algorithm) {
    case Algorithm::SHA_1:
        return 20;
    case Algorithm::SHA_224:
        return 28;
    case Algorithm::SHA_256:
        return 32;
    case Algorithm::SHA_384:
        return 48;
    case Algorithm::SHA_512:
        return 64;
    }
    throw std::invalid_argument("PAL::CryptoDigest: unknown algorithm");
}

void Init(CryptoDigestContext& context) {
    switch (context.algorithm) {
    case Algorithm::SHA_1:
        return Require(SHA1_Init_nid_postfix(&context.sha1), __func__);
    case Algorithm::SHA_224:
        return Require(SHA224_Init_nid_postfix(&context.sha256), __func__);
    case Algorithm::SHA_256:
        return Require(SHA256_Init_nid_postfix(&context.sha256), __func__);
    case Algorithm::SHA_384:
        return Require(SHA384_Init_nid_postfix(&context.sha512), __func__);
    case Algorithm::SHA_512:
        return Require(SHA512_Init_nid_postfix(&context.sha512), __func__);
    }
    throw std::invalid_argument("PAL::CryptoDigest: unknown algorithm");
}

void Update(CryptoDigestContext& context, const void* input, std::size_t length) {
    switch (context.algorithm) {
    case Algorithm::SHA_1:
        return Require(SHA1_Update_nid_postfix(&context.sha1, input, length), __func__);
    case Algorithm::SHA_224:
        return Require(SHA224_Update_nid_postfix(&context.sha256, input, length), __func__);
    case Algorithm::SHA_256:
        return Require(SHA256_Update_nid_postfix(&context.sha256, input, length), __func__);
    case Algorithm::SHA_384:
        return Require(SHA384_Update_nid_postfix(&context.sha512, input, length), __func__);
    case Algorithm::SHA_512:
        return Require(SHA512_Update_nid_postfix(&context.sha512, input, length), __func__);
    }
    throw std::invalid_argument("PAL::CryptoDigest: unknown algorithm");
}

void Final(CryptoDigestContext& context, std::uint8_t* digest) {
    switch (context.algorithm) {
    case Algorithm::SHA_1:
        return Require(SHA1_Final_nid_postfix(digest, &context.sha1), __func__);
    case Algorithm::SHA_224:
        return Require(SHA224_Final_nid_postfix(digest, &context.sha256), __func__);
    case Algorithm::SHA_256:
        return Require(SHA256_Final_nid_postfix(digest, &context.sha256), __func__);
    case Algorithm::SHA_384:
        return Require(SHA384_Final_nid_postfix(digest, &context.sha512), __func__);
    case Algorithm::SHA_512:
        return Require(SHA512_Final_nid_postfix(digest, &context.sha512), __func__);
    }
    throw std::invalid_argument("PAL::CryptoDigest: unknown algorithm");
}

void Destroy(CryptoDigest* self) {
    if (!self) throw std::invalid_argument("PAL::CryptoDigest::~CryptoDigest: null object");
    if (self->m_context) _ZN3WTF8fastFreeEPv(self->m_context);
    self->m_context = nullptr;
}

}  // namespace

extern "C" {

PAL::CryptoDigestPtr APS5_VABI _ZN3PAL12CryptoDigest6createENS0_9AlgorithmE(Algorithm algorithm) {
    if (!IsKnown(algorithm)) return nullptr;
    auto* context = static_cast<CryptoDigestContext*>(_ZN3WTF10fastMallocEm(sizeof(CryptoDigestContext)));
    context->algorithm = algorithm;
    void* storage;
    try {
        Init(*context);
        storage = _Znwm_nid_postfix(sizeof(CryptoDigest));
    } catch (...) {
        _ZN3WTF8fastFreeEPv(context);
        throw;
    }
    return PAL::CryptoDigestPtr(new (storage) CryptoDigest{context});
}

void APS5_VABI _ZN3PAL12CryptoDigest8addBytesEPKvm(CryptoDigest* self, const void* input, std::size_t length) {
    Update(CheckedContext(self, __func__), input, length);
}

NKWebKit::WtfAbi::Vector<std::uint8_t> APS5_VABI _ZN3PAL12CryptoDigest11computeHashEv(CryptoDigest* self) {
    auto& context = CheckedContext(self, __func__);
    const auto length = static_cast<unsigned>(DigestLength(context.algorithm));
    NKWebKit::WtfAbi::Vector<std::uint8_t> result(length);
    Final(context, result.data());
    result.size = length;
    return result;
}

void APS5_VABI _ZN3PAL12CryptoDigestD1Ev(CryptoDigest* self) {
    Destroy(self);
}

void APS5_VABI _ZN3PAL12CryptoDigestD2Ev(CryptoDigest* self) {
    Destroy(self);
}

}
