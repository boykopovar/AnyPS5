#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl3/include/X509.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int SignatureStringType = 3;
constexpr int NullType = 5;
constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr std::int64_t BitsLeftMask = BitsLeftFlag | 0x07;
constexpr int RsaKeyType = 6;
constexpr int RsaPssKeyType = 912;

struct Encoding {
    const unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

static_assert(sizeof(Encoding) == sizeof(LibreSsl3CertificateInfo::encoding));
static_assert(sizeof(LibreSsl3X509) <= sizeof(LibreSsl3Certificate));
static_assert(offsetof(LibreSsl3X509, certInfo) == offsetof(LibreSsl3Certificate, certInfo));
static_assert(offsetof(LibreSsl3X509, sigAlg) == offsetof(LibreSsl3Certificate, sigAlg));

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename Value>
Value CopyIn(const void* source, const char* caller) {
    PreservedErrno guard;
    Value value {};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename Value>
void CopyOut(void* destination, const Value& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(value))) NotImplemented_nid_no_patch(caller);
}

int SignatureNid(int digestNid, int keyType) {
    for (int nid = 1; nid < LibreSsl3::ObjectCount; ++nid) {
        int digest = 0;
        int key = 0;
        if (OBJ_find_sigid_algs_nid_postfix(nid, &digest, &key) == 0) continue;
        if (digest == digestNid && key == keyType) return nid;
    }
    return 0;
}

std::vector<unsigned char> CertificateInfoEncoding(const Encoding& encoding) {
    std::vector<unsigned char> tbs(static_cast<std::size_t>(encoding.length));
    PreservedErrno guard;
    if (!GuestCopyIn_nid_no_patch(tbs.data(), encoding.bytes, tbs.size()))
        NotImplemented_nid_no_patch("X509_sign: certificate info without a current cached encoding");
    return tbs;
}

std::vector<unsigned char> SignBytes(const std::vector<unsigned char>& tbs, LibreSsl3EvpPkey* key,
    const LibreSsl3::DigestMethod* digest) {
    const int keySize = EVP_PKEY_size_nid_postfix(key);
    if (keySize <= 0) NotImplemented_nid_no_patch("X509_sign: digest signing through EVP");
    auto* context = EVP_MD_CTX_new_nid_postfix();
    if (!context) NotImplemented_nid_no_patch("X509_sign: digest signing through EVP");
    std::vector<unsigned char> signature(static_cast<std::size_t>(keySize));
    unsigned int length = 0;
    int signedOk = 0;
    try {
        if (EVP_DigestInit_nid_postfix(context, digest) == 1 && context->update
            && context->update(context, tbs.data(), tbs.size()) == 1)
            signedOk = EVP_SignFinal_nid_postfix(context, signature.data(), &length, key);
    } catch (...) {
        EVP_MD_CTX_free_nid_postfix(context);
        throw;
    }
    EVP_MD_CTX_free_nid_postfix(context);
    if (signedOk != 1 || length == 0 || length > static_cast<unsigned int>(keySize))
        NotImplemented_nid_no_patch("X509_sign: digest signing through EVP");
    signature.resize(length);
    return signature;
}

void SetAlgorithm(LibreSsl3X509Algor* algor, const void* object, LibreSsl3Asn1Type* parameter) {
    CopyOut(&algor->algorithm, object, "X509_sign");
    CopyOut(&algor->parameter, parameter, "X509_sign");
}

bool StoreSignature(const LibreSsl3X509& certificate, LibreSsl3Certificate* guestCertificate,
    const std::vector<unsigned char>& signature) {
    LibreSsl3Asn1String bits {};
    void* target = certificate.signature;
    if (target) {
        bits = CopyIn<LibreSsl3Asn1String>(target, "X509_sign");
    } else {
        auto* allocated = static_cast<LibreSsl3Asn1String*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1String)));
        if (!allocated) return false;
        CopyOut(reinterpret_cast<unsigned char*>(guestCertificate) + offsetof(LibreSsl3X509, signature),
            static_cast<void*>(allocated), "X509_sign");
        target = allocated;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(signature.size()));
    if (!data) return false;
    std::memcpy(data, signature.data(), signature.size());
    void* const previous = bits.data;
    bits.data = data;
    bits.length = static_cast<int>(signature.size());
    bits.type = SignatureStringType;
    bits.flags = (bits.flags & ~BitsLeftMask) | BitsLeftFlag;
    CopyOut(target, bits, "X509_sign");
    if (previous) free_nid_postfix(previous);
    return true;
}

}

extern "C" int APS5_VABI X509_sign_nid_postfix(LibreSsl3Certificate* certificate, LibreSsl3EvpPkey* key,
    const EvpMd* digest) {
    if (!certificate || !key || !digest) return 0;
    const auto fields = CopyIn<LibreSsl3Certificate>(certificate, "X509_sign");
    const auto view = CopyIn<LibreSsl3X509>(certificate, "X509_sign");
    if (!fields.certInfo || !fields.sigAlg) return 0;
    const auto info = CopyIn<LibreSsl3CertificateInfo>(fields.certInfo, "X509_sign");
    if (!info.signature) return 0;
    const auto encoding = CopyIn<Encoding>(info.encoding, "X509_sign");
    if (encoding.modified != 0 || !encoding.bytes || encoding.length <= 0)
        NotImplemented_nid_no_patch("X509_sign: certificate info without a current cached encoding");

    const auto* const digestMethod = reinterpret_cast<const LibreSsl3::DigestMethod*>(digest);
    const int digestNid = digestMethod->type;
    const int keyType = EVP_PKEY_base_id_nid_postfix(key);
    const int signatureNid = SignatureNid(digestNid, keyType);
    if (signatureNid == 0) NotImplemented_nid_no_patch("X509_sign: digest and key type without a signature algorithm");
    const auto* const object = LibreSsl3::ObjectByNid(signatureNid);
    if (!object) NotImplemented_nid_no_patch("X509_sign: digest and key type without a signature algorithm");

    const auto inner = CopyIn<LibreSsl3X509Algor>(info.signature, "X509_sign");
    if (LibreSsl3::ObjectNid(static_cast<const LibreSslAsn1Object*>(inner.algorithm)) != signatureNid)
        NotImplemented_nid_no_patch("X509_sign: certificate info signed with another signature algorithm");

    LibreSsl3Asn1Type* parameter = nullptr;
    if (keyType == RsaKeyType || keyType == RsaPssKeyType) {
        parameter = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
        if (!parameter) return 0;
        parameter->type = NullType;
        parameter->value = nullptr;
    }

    const auto tbs = CertificateInfoEncoding(encoding);
    const auto signature = SignBytes(tbs, key, digestMethod);
    SetAlgorithm(const_cast<LibreSsl3X509Algor*>(fields.sigAlg), object, parameter);
    SetAlgorithm(static_cast<LibreSsl3X509Algor*>(info.signature), object, parameter);
    if (!StoreSignature(view, certificate, signature)) return 0;
    return static_cast<int>(signature.size());
}
