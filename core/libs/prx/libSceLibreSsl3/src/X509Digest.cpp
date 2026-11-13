#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

struct Encoding {
    unsigned char* data;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

static_assert(sizeof(Encoding) == 24);
static_assert(offsetof(LibreSsl3X509CertInfo, encModified) == 96);
static_assert(offsetof(Encoding, modified) == 16);
static_assert(offsetof(LibreSsl3X509CertInfo, encModified) == 80 + offsetof(Encoding, modified));

}

extern "C" int APS5_VABI X509_digest_nid_postfix(const LibreSsl3X509* certificate, const EvpMd* digest, unsigned char* output, std::uint32_t* length) {
    if (!certificate || !certificate->certInfo) throw std::invalid_argument("X509_digest requires a certificate");
    if (!digest || !output) throw std::invalid_argument("X509_digest requires a digest and output");
    const auto& encoding = *reinterpret_cast<const Encoding*>(reinterpret_cast<const unsigned char*>(certificate->certInfo) + 80);
    if (!encoding.data || encoding.length < 0 || encoding.modified)
        NotImplemented_nid_no_patch("X509_digest: certificate without a cached DER encoding");
    return EVP_Digest_nid_postfix(encoding.data, static_cast<std::size_t>(encoding.length), output, length, digest, nullptr);
}
