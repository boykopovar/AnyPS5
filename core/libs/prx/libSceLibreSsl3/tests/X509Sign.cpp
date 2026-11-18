#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_sign check failed at line %u\n", location.line());
    std::exit(1);
}

template <typename Function>
void AllowUnsupported(Function function) {
    try {
        function();
    } catch (const std::exception&) {
    }
}

using Bytes = std::vector<unsigned char>;

constexpr int P256 = 415;
constexpr int Sha256WithRsaEncryption = 668;
constexpr int EcdsaWithSha256 = 794;
constexpr int Sha1WithRsaEncryption = 65;
constexpr int BitStringType = 3;
constexpr int NullType = 5;

const Bytes Sha256WithRsa {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
const Bytes Sha1WithRsa {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05};
const Bytes EcdsaWithSha256Oid {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};

constexpr char RsaPrivateKey[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpQIBAAKCAQEAnSLmvsiTtm1KhSwaFmn9kbS7r8t2OakfpDVlVsA3/7Jxae/2\n"
    "tHdP3RBXtZstk/mguYOBEVEqZC4W0UZmDOurhu3JeqtVCMjhRMbB/e8VqwoVPVac\n"
    "GCWSYou5Kxg2cguwt83bE+AiDXk9dUyQlqUnAHgZiaKC7vp5DdxnpkucjERjppeL\n"
    "L2/ca694m3D2zsusS10ODTy+d1t6bj9zBH8Ap0bny38wskhER6GGR4LDX81iKHrM\n"
    "Dg7fzcLJB2g0pu0McxIq5VqHsPwpmqMgIEQRHIveWBU24fwKpMEI8fTzjDD0XcA2\n"
    "DFz9c5SfWd77MogCStgl6h9uj78BnFavGLZObQIDAQABAoIBAATZufOzr1OXRhCc\n"
    "TuXYObqQsKZ5ilm3TcG84DbwUgozkOqrPJtPf4icY4i4lIyXzQ1M8OP+9GcJQS0O\n"
    "lkHZ2H6ewnRl64cM5F9CH2TeR3XzJrqw/LqGFmGyXHHnGm8kyzyxPwz7AzIVRv9i\n"
    "8Kely3FpQ6iK+q2zz57M6eopNVjjCyQhYHLKeTUmU/sr2FHku6qEhjYAyL4ZKrtT\n"
    "cy50CTDsrCpYgpgAISSQuO6jlqO1qWFqmcyTWH0RUji353tGmj62CI0Yh1DLfWv+\n"
    "Ip3tDoQl1YFVNItur3XBvVE2wtq33Q4XdHWjBquowiO1dXrxMniBtorsRNs45Wk9\n"
    "all+LGUCgYEA1bvnBN8HddXDcW9XOQmyylpx9PYFR2c4qg79KuffLsPcv0b+eIhb\n"
    "tNDoLUiiyaXYhkBfQUvxuJxclmZaL/T2HyW0jIsbYeZR+IcU/anQouX0LTsv7b6z\n"
    "qnqo+5le8wL6dxLYubcxub8M6ILoRw4DZVEK9/+KxchEQvcUQ3gatQsCgYEAvDXK\n"
    "pWxB7mpzCOF2DoneGFLXAwFTNehJLbMilM9njW7B+Axz8i7k8xfVH+DRYzobsmPe\n"
    "5lq1YNhV9qRUObHTzuRMYBN/gb+uFlB6sjWziGHTl2c2y8chCKqC+RrSe1mq8HQ4\n"
    "RywbaFEC/ogM854Kssdp1eMjbgATh9WfGmWyxWcCgYEArKYDQBkUwqexH5Fr6KQf\n"
    "63tR8ClHLF2CFUTUVnNFAlaum0gyyJSFV7MkyWeSL8cq949KIsnA8XpFBCybj3+V\n"
    "zKU9igXYUaWl81bSqvEgmmWcb2kPF0HjicNZextx6mUugz8TGDmWDk/4kl0kWjlE\n"
    "nvPvI1Ml+wwBsAI8OR/smu8CgYEAoQFjWiBCRrLJtc8qL0XBuQ0k4m0/wd30vCOC\n"
    "ccr7ywH07T258nQrZ8n7iEnK1hffmbvV8zml8DMhVt09MB3wFA05SPdlHJndUzmF\n"
    "vO4zp3gIinzBY5H1sHQtluxbIyqSESFbA4e36oVjCltYuQ96F3aAE3k71d461vjo\n"
    "IqzdQgECgYEArHreTGO4wKEl4AfYmOOJsybL8xW2x557EEL8SOk/mopGYP3VNn5B\n"
    "8BfAK+eqgbjop6fE4PZDp2n37a402b4Upc7JQrInGpZwIlE/9bDTXFQWkS6Zny0A\n"
    "ncQjOJpY2aYydxnb/ujlEuAcSTyGQQIGamn93QrMtvB351dBVJaUJIM=\n"
    "-----END RSA PRIVATE KEY-----\n";

const Bytes Certificate {
    0x30, 0x82, 0x03, 0x00, 0x30, 0x82, 0x01, 0xe8, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x14, 0x1b,
    0x9f, 0xab, 0x43, 0x19, 0xd7, 0x22, 0xf8, 0xaf, 0xb8, 0x43, 0xd1, 0xf1, 0xa8, 0x92, 0xd4, 0x99,
    0xb8, 0x48, 0x84, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
    0x05, 0x00, 0x30, 0x20, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x15, 0x41,
    0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x58, 0x35, 0x30, 0x39, 0x5f, 0x73, 0x69, 0x67, 0x6e, 0x20,
    0x74, 0x65, 0x73, 0x74, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x36, 0x31, 0x31, 0x31, 0x38, 0x31, 0x32,
    0x35, 0x36, 0x32, 0x30, 0x5a, 0x17, 0x0d, 0x33, 0x36, 0x31, 0x31, 0x31, 0x35, 0x31, 0x32, 0x35,
    0x36, 0x32, 0x30, 0x5a, 0x30, 0x20, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x15, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x58, 0x35, 0x30, 0x39, 0x5f, 0x73, 0x69, 0x67,
    0x6e, 0x20, 0x74, 0x65, 0x73, 0x74, 0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82,
    0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0x9d, 0x22, 0xe6, 0xbe, 0xc8, 0x93, 0xb6, 0x6d, 0x4a,
    0x85, 0x2c, 0x1a, 0x16, 0x69, 0xfd, 0x91, 0xb4, 0xbb, 0xaf, 0xcb, 0x76, 0x39, 0xa9, 0x1f, 0xa4,
    0x35, 0x65, 0x56, 0xc0, 0x37, 0xff, 0xb2, 0x71, 0x69, 0xef, 0xf6, 0xb4, 0x77, 0x4f, 0xdd, 0x10,
    0x57, 0xb5, 0x9b, 0x2d, 0x93, 0xf9, 0xa0, 0xb9, 0x83, 0x81, 0x11, 0x51, 0x2a, 0x64, 0x2e, 0x16,
    0xd1, 0x46, 0x66, 0x0c, 0xeb, 0xab, 0x86, 0xed, 0xc9, 0x7a, 0xab, 0x55, 0x08, 0xc8, 0xe1, 0x44,
    0xc6, 0xc1, 0xfd, 0xef, 0x15, 0xab, 0x0a, 0x15, 0x3d, 0x56, 0x9c, 0x18, 0x25, 0x92, 0x62, 0x8b,
    0xb9, 0x2b, 0x18, 0x36, 0x72, 0x0b, 0xb0, 0xb7, 0xcd, 0xdb, 0x13, 0xe0, 0x22, 0x0d, 0x79, 0x3d,
    0x75, 0x4c, 0x90, 0x96, 0xa5, 0x27, 0x00, 0x78, 0x19, 0x89, 0xa2, 0x82, 0xee, 0xfa, 0x79, 0x0d,
    0xdc, 0x67, 0xa6, 0x4b, 0x9c, 0x8c, 0x44, 0x63, 0xa6, 0x97, 0x8b, 0x2f, 0x6f, 0xdc, 0x6b, 0xaf,
    0x78, 0x9b, 0x70, 0xf6, 0xce, 0xcb, 0xac, 0x4b, 0x5d, 0x0e, 0x0d, 0x3c, 0xbe, 0x77, 0x5b, 0x7a,
    0x6e, 0x3f, 0x73, 0x04, 0x7f, 0x00, 0xa7, 0x46, 0xe7, 0xcb, 0x7f, 0x30, 0xb2, 0x48, 0x44, 0x47,
    0xa1, 0x86, 0x47, 0x82, 0xc3, 0x5f, 0xcd, 0x62, 0x28, 0x7a, 0xcc, 0x0e, 0x0e, 0xdf, 0xcd, 0xc2,
    0xc9, 0x07, 0x68, 0x34, 0xa6, 0xed, 0x0c, 0x73, 0x12, 0x2a, 0xe5, 0x5a, 0x87, 0xb0, 0xfc, 0x29,
    0x9a, 0xa3, 0x20, 0x20, 0x44, 0x11, 0x1c, 0x8b, 0xde, 0x58, 0x15, 0x36, 0xe1, 0xfc, 0x0a, 0xa4,
    0xc1, 0x08, 0xf1, 0xf4, 0xf3, 0x8c, 0x30, 0xf4, 0x5d, 0xc0, 0x36, 0x0c, 0x5c, 0xfd, 0x73, 0x94,
    0x9f, 0x59, 0xde, 0xfb, 0x32, 0x88, 0x02, 0x4a, 0xd8, 0x25, 0xea, 0x1f, 0x6e, 0x8f, 0xbf, 0x01,
    0x9c, 0x56, 0xaf, 0x18, 0xb6, 0x4e, 0x6d, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x32, 0x30, 0x30,
    0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0xbc, 0xfd, 0x3d, 0x47, 0xed,
    0xce, 0xaf, 0x53, 0x3f, 0x2a, 0x96, 0xef, 0xaf, 0x6f, 0xd7, 0x65, 0x92, 0xf7, 0xfa, 0x91, 0x30,
    0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff,
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03,
    0x82, 0x01, 0x01, 0x00, 0x3c, 0xab, 0xa6, 0x98, 0x32, 0x53, 0x8a, 0x2f, 0x6b, 0x75, 0x7f, 0x3d,
    0x46, 0x42, 0x50, 0x85, 0x4b, 0x8e, 0xa3, 0x81, 0x0a, 0x3f, 0xb8, 0x48, 0xa2, 0xf0, 0xad, 0x71,
    0x8a, 0x6f, 0xb7, 0xce, 0xe0, 0x69, 0xa1, 0x6d, 0x29, 0x19, 0xad, 0x52, 0xe2, 0xca, 0x45, 0xab,
    0xe4, 0x35, 0x35, 0xe5, 0x57, 0x30, 0x95, 0x12, 0x87, 0x2f, 0x95, 0x66, 0xd2, 0x7f, 0x03, 0x1a,
    0xaa, 0x1c, 0x19, 0x0c, 0xa1, 0x55, 0xea, 0xdf, 0x6f, 0xfb, 0x8d, 0xa4, 0x77, 0x4a, 0x39, 0xea,
    0xe3, 0xf8, 0x2c, 0x0e, 0x24, 0x70, 0x24, 0x6b, 0x53, 0x27, 0x3a, 0xc7, 0x47, 0xb1, 0xd8, 0x8b,
    0x87, 0x5d, 0x9b, 0x68, 0x49, 0xb4, 0xba, 0xe7, 0x8e, 0x48, 0x98, 0xeb, 0xec, 0x67, 0x00, 0x80,
    0xcf, 0x84, 0x9c, 0xb6, 0x64, 0x16, 0x3e, 0x87, 0x51, 0xfa, 0x06, 0xc5, 0x5f, 0x73, 0x52, 0x93,
    0x3f, 0x32, 0x08, 0x2e, 0x23, 0xe9, 0x44, 0x1a, 0x58, 0x0d, 0xd0, 0x3d, 0xfb, 0x6e, 0xe3, 0xc6,
    0x20, 0x4a, 0xac, 0xfd, 0x05, 0x70, 0xa4, 0x82, 0x37, 0xba, 0x97, 0xa3, 0xec, 0x99, 0xd5, 0x2c,
    0x96, 0x4f, 0x94, 0xe9, 0xde, 0x41, 0x93, 0x35, 0x63, 0x19, 0xff, 0x8c, 0x20, 0x36, 0x5d, 0xa9,
    0x84, 0x11, 0xfd, 0x4b, 0x9c, 0xda, 0x00, 0x07, 0xa1, 0x7b, 0xc3, 0x10, 0x59, 0x02, 0xb7, 0x78,
    0x3e, 0xa7, 0x5b, 0x71, 0x91, 0xd8, 0x32, 0x88, 0x5d, 0x9f, 0x60, 0x9a, 0xf0, 0xda, 0xd4, 0x3d,
    0x53, 0xea, 0x79, 0xd2, 0x19, 0xd2, 0x51, 0x99, 0x44, 0x18, 0xe9, 0x32, 0xd3, 0x96, 0xd2, 0x00,
    0x5d, 0xb8, 0x09, 0x6d, 0xf9, 0x67, 0x28, 0xb5, 0xe8, 0x1f, 0x09, 0x85, 0xa7, 0x9d, 0x88, 0xab,
    0x22, 0x8a, 0x6a, 0x6f, 0x46, 0x28, 0x78, 0xeb, 0xea, 0x25, 0x0b, 0xd6, 0x2f, 0x53, 0xba, 0x2c,
    0x29, 0xb5, 0x54, 0xce
};

void* APS5_VABI Allocate(std::size_t bytes) { return std::malloc(bytes ? bytes : 1); }
void APS5_VABI Release(void* block) { std::free(block); }
void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}
void* APS5_VABI Reallocate(void* block, std::size_t bytes) { return std::realloc(block, bytes ? bytes : 1); }
void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

struct Span {
    const unsigned char* data;
    std::size_t size;
};

Span Element(const unsigned char*& cursor, const unsigned char* end, unsigned char tag) {
    Require(end - cursor >= 2 && cursor[0] == tag);
    std::size_t length = cursor[1];
    cursor += 2;
    if ((length & 0x80) != 0) {
        const std::size_t count = length & 0x7f;
        Require(count != 0 && count <= sizeof(std::size_t) && static_cast<std::size_t>(end - cursor) >= count);
        length = 0;
        for (std::size_t index = 0; index < count; ++index) length = (length << 8) | *cursor++;
    }
    Require(static_cast<std::size_t>(end - cursor) >= length);
    const Span element {cursor, length};
    cursor += length;
    return element;
}

struct CertificateParts {
    Span tbs;
    Span algorithm;
    Span signature;
};

CertificateParts Parse(const Bytes& encoding) {
    const unsigned char* cursor = encoding.data();
    const unsigned char* const end = cursor + encoding.size();
    const Span sequence = Element(cursor, end, 0x30);
    Require(cursor == end && sequence.data + sequence.size == end);
    const unsigned char* inner = sequence.data;
    const unsigned char* const innerEnd = inner + sequence.size;
    CertificateParts parts;
    const unsigned char* const tbsStart = inner;
    Element(inner, innerEnd, 0x30);
    parts.tbs = Span {tbsStart, static_cast<std::size_t>(inner - tbsStart)};
    parts.algorithm = Element(inner, innerEnd, 0x30);
    parts.signature = Element(inner, innerEnd, 0x03);
    Require(inner == innerEnd);
    Require(parts.signature.size > 1 && parts.signature.data[0] == 0);
    parts.signature.data += 1;
    parts.signature.size -= 1;
    return parts;
}

struct Encoding {
    const unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

struct Fixture {
    LibreSsl3CertificateInfo info {};
    LibreSsl3X509Algor inner {};
    LibreSsl3X509Algor outer {};
    LibreSslAsn1Object object {};
    LibreSsl3Asn1String signature {};
    LibreSsl3Certificate certificate {};

    Fixture() {
        std::memset(&info, 0, sizeof(info));
        std::memset(&certificate, 0, sizeof(certificate));
        certificate.certInfo = &info;
        certificate.sigAlg = &outer;
        info.signature = &inner;
        const LibreSsl3Asn1String* bits = &signature;
        std::memcpy(certificate.rest, &bits, sizeof(bits));
    }

    void SetEncoding(const Span& tbs, std::int32_t modified) {
        const Encoding encoding {tbs.data, static_cast<std::int64_t>(tbs.size), modified, 0};
        std::memcpy(info.encoding, &encoding, sizeof(encoding));
    }

    void SetAlgorithm(int nid, const Bytes& oid) {
        object = LibreSslAsn1Object {nullptr, nullptr, nid, static_cast<std::int32_t>(oid.size()), oid.data(), 0};
        outer = LibreSsl3X509Algor {&object, nullptr};
        inner = LibreSsl3X509Algor {&object, nullptr};
    }
};

LibreSsl3EvpPkey* ReadPrivateKey() {
    auto* bio = BIO_new_mem_buf_nid_postfix(RsaPrivateKey, static_cast<int>(std::strlen(RsaPrivateKey)));
    Require(bio != nullptr);
    auto* key = PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bio, nullptr, nullptr, nullptr);
    BIO_free_nid_postfix(bio);
    Require(key != nullptr);
    return key;
}

bool Verify(const Bytes& data, const Bytes& signature, LibreSsl3EvpPkey* key) {
    auto* context = EVP_MD_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(EVP_DigestInit_nid_postfix(context,
        reinterpret_cast<const LibreSsl3::DigestMethod*>(EVP_sha256_nid_postfix())) == 1);
    Require(context->update(context, data.data(), data.size()) == 1);
    const int result = EVP_VerifyFinal_nid_postfix(context, signature.data(),
        static_cast<unsigned int>(signature.size()), key);
    EVP_MD_CTX_free_nid_postfix(context);
    return result == 1;
}

Bytes Encode(const LibreSsl3Certificate* certificate) {
    unsigned char* buffer = nullptr;
    const int length = i2d_X509_nid_postfix(certificate, &buffer);
    Require(length > 0 && buffer != nullptr);
    const Bytes result(buffer, buffer + length);
    free_nid_postfix(buffer);
    return result;
}

Bytes Signed(const Fixture& fixture) {
    return Bytes(fixture.signature.data, fixture.signature.data + fixture.signature.length);
}

void CheckSignedCertificate() {
    auto* key = ReadPrivateKey();
    const CertificateParts parts = Parse(Certificate);
    const Bytes tbs(parts.tbs.data, parts.tbs.data + parts.tbs.size);
    const Bytes original(parts.signature.data, parts.signature.data + parts.signature.size);
    Require(original.size() == 256);

    Fixture fixture;
    fixture.SetEncoding(parts.tbs, 0);
    fixture.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);

    Require(X509_sign_nid_postfix(&fixture.certificate, key, EVP_sha256_nid_postfix()) == 256);
    Require(fixture.signature.length == 256);
    Require(fixture.signature.type == BitStringType);
    Require((fixture.signature.flags & 0x0f) == 0x08);
    const Bytes signature = Signed(fixture);
    Require(signature == original);

    const auto* innerObject = static_cast<const LibreSslAsn1Object*>(fixture.inner.algorithm);
    const auto* outerObject = static_cast<const LibreSslAsn1Object*>(fixture.outer.algorithm);
    Require(innerObject != nullptr && outerObject == innerObject);
    Require(innerObject->nid == Sha256WithRsaEncryption);
    Require(Bytes(innerObject->data, innerObject->data + innerObject->length) == Sha256WithRsa);
    Require(fixture.inner.parameter != nullptr && fixture.inner.parameter->type == NullType
        && fixture.inner.parameter->value == nullptr);
    Require(fixture.outer.parameter != nullptr && fixture.outer.parameter != fixture.inner.parameter);
    Require(fixture.outer.parameter->type == NullType && fixture.outer.parameter->value == nullptr);
    Require(X509_get_signature_nid_nid_postfix(&fixture.certificate) == Sha256WithRsaEncryption);

    Require(Verify(tbs, signature, key));
    Bytes corrupted = signature;
    corrupted[64] ^= 0x01;
    Require(!Verify(tbs, corrupted, key));

    const Bytes reencoded = Encode(&fixture.certificate);
    Require(reencoded == Certificate);
    const CertificateParts after = Parse(reencoded);
    Require(Bytes(after.tbs.data, after.tbs.data + after.tbs.size) == tbs);
    Require(Bytes(after.algorithm.data, after.algorithm.data + after.algorithm.size)
        == Bytes(parts.algorithm.data, parts.algorithm.data + parts.algorithm.size));
    Require(Bytes(after.signature.data, after.signature.data + after.signature.size) == signature);

    Require(X509_sign_nid_postfix(&fixture.certificate, key, EVP_sha256_nid_postfix()) == 256);
    Require(Signed(fixture) == signature);
    Require(Verify(tbs, Signed(fixture), key));
    Require(Encode(&fixture.certificate) == Certificate);

    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void CheckNullArguments() {
    auto* key = ReadPrivateKey();
    const CertificateParts parts = Parse(Certificate);
    Fixture fixture;
    fixture.SetEncoding(parts.tbs, 0);
    fixture.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);

    Require(X509_sign_nid_postfix(nullptr, key, EVP_sha256_nid_postfix()) == 0);
    Require(X509_sign_nid_postfix(&fixture.certificate, nullptr, EVP_sha256_nid_postfix()) == 0);
    Require(X509_sign_nid_postfix(&fixture.certificate, key, nullptr) == 0);
    Require(fixture.signature.length == 0 && fixture.signature.data == nullptr);

    Fixture noInfo;
    noInfo.certificate.certInfo = nullptr;
    Require(X509_sign_nid_postfix(&noInfo.certificate, key, EVP_sha256_nid_postfix()) == 0);

    Fixture noAlgorithm;
    noAlgorithm.SetEncoding(parts.tbs, 0);
    noAlgorithm.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);
    noAlgorithm.certificate.sigAlg = nullptr;
    Require(X509_sign_nid_postfix(&noAlgorithm.certificate, key, EVP_sha256_nid_postfix()) == 0);

    Fixture noSignature;
    noSignature.SetEncoding(parts.tbs, 0);
    noSignature.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);
    noSignature.info.signature = nullptr;
    Require(X509_sign_nid_postfix(&noSignature.certificate, key, EVP_sha256_nid_postfix()) == 0);

    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

void CheckUnsupportedKeys() {
    const CertificateParts parts = Parse(Certificate);
    Fixture fixture;

    EcKey* ecKey = EC_KEY_new_by_curve_name_nid_postfix(P256);
    Require(ecKey != nullptr && EC_KEY_generate_key_nid_postfix(ecKey) == 1);
    LibreSsl3EvpPkey* ec = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(ec != nullptr);
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(ec, reinterpret_cast<LibreSsl3EcKey*>(ecKey)) == 1);
    EC_KEY_free_nid_postfix(ecKey);
    fixture.SetEncoding(parts.tbs, 0);
    fixture.SetAlgorithm(EcdsaWithSha256, EcdsaWithSha256Oid);
    AllowUnsupported([&] { X509_sign_nid_postfix(&fixture.certificate, ec, EVP_sha256_nid_postfix()); });
    Require(fixture.signature.length == 0);
    fixture.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);
    AllowUnsupported([&] { X509_sign_nid_postfix(&fixture.certificate, ec, EVP_sha256_nid_postfix()); });
    Require(fixture.signature.length == 0);
    EVP_PKEY_free_nid_disambig1_nid_postfix(ec);

    const unsigned char secret[3] {0x01, 0x02, 0x03};
    LibreSsl3EvpPkey* mac = EVP_PKEY_new_mac_key_nid_postfix(LibreSsl3PkeyHmac, nullptr, secret, 3);
    Require(mac != nullptr);
    AllowUnsupported([&] { X509_sign_nid_postfix(&fixture.certificate, mac, EVP_sha256_nid_postfix()); });
    Require(fixture.signature.length == 0);
    EVP_PKEY_free_nid_disambig1_nid_postfix(mac);
}

void CheckCertificateInfoGaps() {
    auto* key = ReadPrivateKey();
    const CertificateParts parts = Parse(Certificate);

    Fixture modified;
    modified.SetEncoding(parts.tbs, 1);
    modified.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);
    AllowUnsupported([&] { X509_sign_nid_postfix(&modified.certificate, key, EVP_sha256_nid_postfix()); });

    Fixture uncached;
    uncached.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);
    AllowUnsupported([&] { X509_sign_nid_postfix(&uncached.certificate, key, EVP_sha256_nid_postfix()); });

    Fixture signedInstead;
    signedInstead.SetEncoding(parts.tbs, 0);
    signedInstead.SetAlgorithm(Sha1WithRsaEncryption, Sha1WithRsa);
    AllowUnsupported([&] { X509_sign_nid_postfix(&signedInstead.certificate, key, EVP_sha256_nid_postfix()); });
    Require(signedInstead.signature.length == 0);

    Fixture unreadable;
    unreadable.SetEncoding(parts.tbs, 0);
    unreadable.SetAlgorithm(Sha256WithRsaEncryption, Sha256WithRsa);
    AllowUnsupported([&] {
        X509_sign_nid_postfix(reinterpret_cast<LibreSsl3Certificate*>(std::uintptr_t {0x10}), key,
            EVP_sha256_nid_postfix());
    });

    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

}

int main() {
    InitializeHeap();
    CheckSignedCertificate();
    CheckNullArguments();
    CheckUnsupportedKeys();
    CheckCertificateInfoGaps();
    std::puts("libSceLibreSsl3 X509_sign tests passed");
    return 0;
}
