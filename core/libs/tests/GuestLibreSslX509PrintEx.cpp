#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t X509FlagCompat = 0;
constexpr std::uint64_t X509FlagNoHeader = 1ULL << 0;
constexpr std::uint64_t X509FlagNoVersion = 1ULL << 1;
constexpr std::uint64_t X509FlagNoSerial = 1ULL << 2;
constexpr std::uint64_t X509FlagNoSigName = 1ULL << 3;
constexpr std::uint64_t X509FlagNoIssuer = 1ULL << 4;
constexpr std::uint64_t X509FlagNoValidity = 1ULL << 5;
constexpr std::uint64_t X509FlagNoSubject = 1ULL << 6;
constexpr std::uint64_t X509FlagNoPubkey = 1ULL << 7;
constexpr std::uint64_t X509FlagNoExtensions = 1ULL << 8;
constexpr std::uint64_t X509FlagNoSigDump = 1ULL << 9;
constexpr std::uint64_t X509FlagNoAux = 1ULL << 10;

constexpr std::uint64_t ExtErrorUnknown = 1ULL << 16;
constexpr std::uint64_t ExtDumpUnknown = 3ULL << 16;

constexpr std::uint64_t XnFlagSepCommaPlus = 1ULL << 16;
constexpr std::uint64_t XnFlagSepCplusSpc = 2ULL << 16;
constexpr std::uint64_t XnFlagSepMultiline = 4ULL << 16;
constexpr std::uint64_t XnFlagDnRev = 1ULL << 20;
constexpr std::uint64_t XnFlagFnLn = 1ULL << 21;
constexpr std::uint64_t XnFlagSpcEq = 1ULL << 23;
constexpr std::uint64_t XnFlagDumpUnknown = 1ULL << 24;
constexpr std::uint64_t XnFlagFnAlign = 1ULL << 25;
constexpr std::uint64_t XnFlagEsc2253 = 1;
constexpr std::uint64_t XnFlagEscCtrl = 2;
constexpr std::uint64_t XnFlagEscMsb = 4;
constexpr std::uint64_t XnFlagEscQuote = 8;
constexpr std::uint64_t XnFlagUtf8Convert = 0x10;
constexpr std::uint64_t XnFlagDumpUnknownAsn1 = 0x100;
constexpr std::uint64_t XnFlagDumpDer = 0x200;
constexpr std::uint64_t XnFlagRfc2253Strings = XnFlagEsc2253 | XnFlagEscCtrl | XnFlagEscMsb | XnFlagUtf8Convert | XnFlagDumpUnknownAsn1 | XnFlagDumpDer;
constexpr std::uint64_t XnFlagRfc2253 = XnFlagRfc2253Strings | XnFlagSepCommaPlus | XnFlagDnRev | XnFlagDumpUnknown;
constexpr std::uint64_t XnFlagOneline = XnFlagRfc2253Strings | XnFlagEscQuote | XnFlagSepCplusSpc | XnFlagSpcEq;
constexpr std::uint64_t XnFlagMultiline = XnFlagEscCtrl | XnFlagEscMsb | XnFlagSepMultiline | XnFlagSpcEq | XnFlagFnLn | XnFlagFnAlign;

struct TestAsn1Type {
    std::int32_t type;
    std::int32_t padding;
    const void* value;
};

struct TestX509Algor {
    const LibreSslAsn1Object* algorithm;
    const TestAsn1Type* parameter;
};

struct TestX509Validity {
    const Asn1String* notBefore;
    const Asn1String* notAfter;
};

struct TestX509Pubkey {
    const TestX509Algor* algor;
    const Asn1String* publicKey;
    const void* pkey;
};

struct TestEvpPkey {
    std::int32_t type;
    std::int32_t saveType;
    std::int32_t references;
    std::int32_t padding;
    const void* ameth;
    void* engine;
    void* key;
    std::int32_t saveParameters;
    std::int32_t padding2;
    void* attributes;
};

struct TestX509Extension {
    const LibreSslAsn1Object* object;
    std::int32_t critical;
    std::int32_t padding;
    const Asn1String* value;
};

struct TestBignum {
    std::uint64_t* d;
    std::int32_t top;
    std::int32_t dmax;
    std::int32_t neg;
    std::int32_t flags;
};

void Require(bool condition, const char* what = nullptr, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "X509_print_ex check failed at line %u: %s\n", location.line(), what ? what : "");
    std::abort();
}

template <typename Fn>
void RequireThrows(Fn fn, const char* what = nullptr, std::source_location location = std::source_location::current()) {
    try {
        fn();
    } catch (const std::exception&) {
        return;
    }
    std::fprintf(stderr, "X509_print_ex expected exception at line %u: %s\n", location.line(), what ? what : "");
    std::abort();
}

int liveBlocks = 0;

void* APS5_VABI HeapAlloc(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI HeapFree(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) {
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI HeapRealloc(void* block, std::size_t bytes) {
    if (!block) ++liveBlocks;
    return std::realloc(block, bytes ? bytes : 1);
}

void* APS5_VABI HeapAligned(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI HeapRealigned(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI HeapPosixAligned(void**, std::size_t, std::size_t) { return 12; }

void RegisterGuestHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&HeapAlloc), reinterpret_cast<void*>(&HeapFree), reinterpret_cast<void*>(&HeapCalloc),
        reinterpret_cast<void*>(&HeapRealloc), reinterpret_cast<void*>(&HeapAligned), reinterpret_cast<void*>(&HeapRealigned),
        reinterpret_cast<void*>(&HeapPosixAligned),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

std::string printed;
int writesBeforeFailure = -1;

int APS5_VABI CaptureWrite(LibreSsl::Bio*, const char* data, int size) {
    if (writesBeforeFailure == 0) return -1;
    if (writesBeforeFailure > 0) --writesBeforeFailure;
    printed.append(data, static_cast<std::size_t>(size));
    return size;
}

struct CaptureBio {
    LibreSsl::BioMethod method{};
    LibreSsl::Bio bio{};

    CaptureBio() {
        method.write = &CaptureWrite;
        bio.method = &method;
        bio.initialized = 1;
    }
};

constexpr unsigned char OidSha256WithRsa[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
constexpr unsigned char OidRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
constexpr unsigned char OidEcPublicKey[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
constexpr unsigned char OidPrime256v1[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07};
constexpr unsigned char OidDsaWithSha1[] = {0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x03};
constexpr unsigned char OidRsaSsaPss[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a};
constexpr unsigned char OidOcspNonce[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x02};
constexpr unsigned char OidUnknownExt[] = {0x2a, 0x03, 0x04};
constexpr unsigned char OidCommonName[] = {0x55, 0x04, 0x03};
constexpr unsigned char OidCountry[] = {0x55, 0x04, 0x06};
constexpr unsigned char CommonNameBytes[] = "AnyPS5 Server";
constexpr unsigned char IssuerCommonNameBytes[] = "AnyPS5 CA";
constexpr unsigned char CountryBytes[] = "JP";
constexpr unsigned char NotBeforeBytes[] = "261117200104Z";
constexpr unsigned char NotAfterBytes[] = "361114200104Z";

constexpr unsigned char RsaSpkiDer[] = {
    0x30, 0x18,
    0x02, 0x11, 0x00, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18, 0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90,
    0x02, 0x03, 0x01, 0x00, 0x01,
};

constexpr unsigned char SignatureBytes[] = {
    0x30, 0x45, 0x02, 0x21, 0x00, 0x83, 0x08, 0xa4, 0xa1, 0x7a, 0x2e, 0x64, 0xc3, 0x69, 0x09, 0x5e, 0xfa, 0xfd, 0x74, 0x6f,
};

constexpr unsigned char OcspNonceBytes[] = {0xde, 0xad, 0xbe, 0xef};
constexpr unsigned char UnknownExtBytes[] = {'a', 'b', 'c', 0x01};

constexpr unsigned char EcPubPointBytes[] = {
    0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee,
    0xff, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0,
    0xe0, 0xf0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x0e, 0x0f, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
    0x1e, 0x1f, 0x21, 0x22, 0x23,
};

struct CertificateFixture {
    unsigned char versionBytes[16]{0x02};
    unsigned char serialBytes[24]{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    unsigned char signatureBytes[64]{};
    unsigned char notBeforeBytes[32]{};
    unsigned char notAfterBytes[32]{};
    unsigned char sigAlgOid[16]{};
    unsigned char pubKeyAlgOid[16]{};
    unsigned char curveOid[16]{};
    unsigned char pubKeyData[128]{};
    unsigned char ocspNonceData[16]{};
    unsigned char unknownExtData[16]{};

    Asn1String version{1, 2, versionBytes, 0};
    Asn1String serial{8, 2, serialBytes, 0};
    Asn1String signature{sizeof(SignatureBytes), 3, signatureBytes, 0};
    Asn1String notBefore{13, 23, notBeforeBytes, 0};
    Asn1String notAfter{13, 23, notAfterBytes, 0};
    Asn1String pubKeyBits{sizeof(RsaSpkiDer), 3, pubKeyData, 0};
    Asn1String ocspNonceValue{sizeof(OcspNonceBytes), 4, ocspNonceData, 0};
    Asn1String unknownExtValue{sizeof(UnknownExtBytes), 4, unknownExtData, 0};

    LibreSslAsn1Object sigAlgObj{nullptr, nullptr, 0, sizeof(OidSha256WithRsa), sigAlgOid, 0};
    LibreSslAsn1Object pubAlgObj{nullptr, nullptr, 0, sizeof(OidRsaEncryption), pubKeyAlgOid, 0};
    LibreSslAsn1Object curveObj{nullptr, nullptr, 0, sizeof(OidPrime256v1), curveOid, 0};
    LibreSslAsn1Object ocspNonceObj{nullptr, nullptr, 0, sizeof(OidOcspNonce), OidOcspNonce, 0};
    LibreSslAsn1Object unknownExtObj{nullptr, nullptr, 0, sizeof(OidUnknownExt), OidUnknownExt, 0};
    LibreSslAsn1Object commonNameObj{nullptr, nullptr, 13, sizeof(OidCommonName), OidCommonName, 0};
    LibreSslAsn1Object countryObj{nullptr, nullptr, 14, sizeof(OidCountry), OidCountry, 0};

    TestAsn1Type pubParam{6, 0, &curveObj};
    TestX509Algor sigAlg{&sigAlgObj, nullptr};
    TestX509Algor pubAlg{&pubAlgObj, nullptr};
    TestX509Pubkey pubkey{&pubAlg, &pubKeyBits, nullptr};
    TestX509Validity validity{&notBefore, &notAfter};

    Asn1String issuerCn{9, 12, const_cast<unsigned char*>(IssuerCommonNameBytes), 0};
    Asn1String subjectCn{13, 12, const_cast<unsigned char*>(CommonNameBytes), 0};
    Asn1String subjectCountry{2, 12, const_cast<unsigned char*>(CountryBytes), 0};
    LibreSslX509NameEntry issuerEntries[2]{};
    LibreSslX509NameEntry subjectEntries[2]{};
    void* issuerPtrs[2]{};
    void* subjectPtrs[2]{};
    LibreSslStack issuerStack{};
    LibreSslStack subjectStack{};
    LibreSslX509Name issuer{};
    LibreSslX509Name subject{};

    TestX509Extension extList[2]{};
    void* extPtrs[2]{};
    LibreSslStack extStack{};

    LibreSslX509CertInfo info{};
    LibreSslX509 cert{};

    CertificateFixture() {
        std::memcpy(signatureBytes, SignatureBytes, sizeof(SignatureBytes));
        std::memcpy(notBeforeBytes, NotBeforeBytes, 13);
        std::memcpy(notAfterBytes, NotAfterBytes, 13);
        std::memcpy(sigAlgOid, OidSha256WithRsa, sizeof(OidSha256WithRsa));
        std::memcpy(pubKeyAlgOid, OidRsaEncryption, sizeof(OidRsaEncryption));
        std::memcpy(curveOid, OidPrime256v1, sizeof(OidPrime256v1));
        std::memcpy(pubKeyData, RsaSpkiDer, sizeof(RsaSpkiDer));
        std::memcpy(ocspNonceData, OcspNonceBytes, sizeof(OcspNonceBytes));
        std::memcpy(unknownExtData, UnknownExtBytes, sizeof(UnknownExtBytes));
        Wire();
    }

    void Wire() {
        version.data = versionBytes;
        serial.data = serialBytes;
        signature.data = signatureBytes;
        notBefore.data = notBeforeBytes;
        notAfter.data = notAfterBytes;
        pubKeyBits.data = pubKeyData;
        ocspNonceValue.data = ocspNonceData;
        unknownExtValue.data = unknownExtData;
        sigAlgObj.data = sigAlgOid;
        pubAlgObj.data = pubKeyAlgOid;
        curveObj.data = curveOid;

        issuerEntries[0] = LibreSslX509NameEntry{&countryObj, &subjectCountry, 0, 1};
        issuerEntries[1] = LibreSslX509NameEntry{&commonNameObj, &issuerCn, 1, 1};
        subjectEntries[0] = LibreSslX509NameEntry{&countryObj, &subjectCountry, 0, 1};
        subjectEntries[1] = LibreSslX509NameEntry{&commonNameObj, &subjectCn, 1, 1};

        issuerPtrs[0] = &issuerEntries[0];
        issuerPtrs[1] = &issuerEntries[1];
        subjectPtrs[0] = &subjectEntries[0];
        subjectPtrs[1] = &subjectEntries[1];

        issuerStack = LibreSslStack{2, reinterpret_cast<char**>(issuerPtrs), 0, 2, nullptr};
        subjectStack = LibreSslStack{2, reinterpret_cast<char**>(subjectPtrs), 0, 2, nullptr};
        issuer = LibreSslX509Name{&issuerStack, 0, nullptr, nullptr, 0};
        subject = LibreSslX509Name{&subjectStack, 0, nullptr, nullptr, 0};

        extList[0] = TestX509Extension{&ocspNonceObj, 1, 0, &ocspNonceValue};
        extList[1] = TestX509Extension{&unknownExtObj, 0, 0, &unknownExtValue};
        extPtrs[0] = &extList[0];
        extPtrs[1] = &extList[1];
        extStack = LibreSslStack{2, reinterpret_cast<char**>(extPtrs), 0, 2, nullptr};

        sigAlg = TestX509Algor{&sigAlgObj, nullptr};
        pubAlg.algorithm = &pubAlgObj;
        pubkey.algor = &pubAlg;
        pubkey.publicKey = &pubKeyBits;
        validity = TestX509Validity{&notBefore, &notAfter};

        info = LibreSslX509CertInfo{
            &version, &serial, &sigAlg, &issuer, &validity, &subject, &pubkey,
            nullptr, nullptr, &extStack, {},
        };
        cert = LibreSslX509{&info, &sigAlg, &signature, 1, 1};
    }
};

std::string ExpectedRsaFull() {
    return
        "Certificate:\n"
        "    Data:\n"
        "        Version: 3 (0x2)\n"
        "        Serial Number: 81985529216486895 (0x123456789abcdef)\n"
        "    Signature Algorithm: sha256WithRSAEncryption\n"
        "        Issuer: C=JP, CN=AnyPS5 CA\n"
        "        Validity\n"
        "            Not Before: Nov 17 20:01:04 2026 GMT\n"
        "            Not After : Nov 14 20:01:04 2036 GMT\n"
        "        Subject: C=JP, CN=AnyPS5 Server\n"
        "        Subject Public Key Info:\n"
        "            Public Key Algorithm: rsaEncryption\n"
        "                Public-Key: (128 bit)\n"
        "                Modulus:\n"
        "                    00:a1:b2:c3:d4:e5:f6:07:18:29:3a:4b:5c:6d:7e:\n"
        "                    8f:90\n"
        "                Exponent: 65537 (0x10001)\n"
        "        X509v3 extensions:\n"
        "            OCSP Nonce: critical\n"
        "                DEADBEEF\n"
        "            1.2.3.4: \n"
        "                abc.\n"
        "    Signature Algorithm: sha256WithRSAEncryption\n"
        "         30:45:02:21:00:83:08:a4:a1:7a:2e:64:c3:69:09:5e:fa:fd:\n"
        "         74:6f\n";
}

void CheckFullRsaAndAttachedPkey() {
    CertificateFixture fixture;
    CaptureBio bio;
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, X509FlagCompat) == 1);
    Require(printed == ExpectedRsaFull(), "full RSA certificate literal mismatch");
    Require(liveBlocks == 0);

    std::uint64_t nWords[2]{0x293a4b5c6d7e8f90ULL, 0xa1b2c3d4e5f60718ULL};
    std::uint64_t eWords[1]{65537ULL};
    TestBignum nBn{nWords, 2, 2, 0, 1};
    TestBignum eBn{eWords, 1, 1, 0, 1};
    LibreSslRsa rsa{};
    rsa.n = &nBn;
    rsa.e = &eBn;
    TestEvpPkey pkey{6, 6, 1, 0, nullptr, nullptr, &rsa, 1, 0, nullptr};
    fixture.pubkey.publicKey = nullptr;
    fixture.pubkey.pkey = &pkey;
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, X509FlagCompat) == 1);
    Require(printed == ExpectedRsaFull(), "attached EVP_PKEY RSA certificate literal mismatch");
}

void CheckEcCertificate() {
    CertificateFixture fixture;
    std::memcpy(fixture.pubKeyAlgOid, OidEcPublicKey, sizeof(OidEcPublicKey));
    fixture.pubAlgObj.length = sizeof(OidEcPublicKey);
    fixture.pubAlg.parameter = &fixture.pubParam;
    std::memcpy(fixture.pubKeyData, EcPubPointBytes, sizeof(EcPubPointBytes));
    fixture.pubKeyBits.length = sizeof(EcPubPointBytes);
    fixture.info.extensions = nullptr;
    CaptureBio bio;
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, X509FlagCompat) == 1);
    const std::string expected =
        "Certificate:\n"
        "    Data:\n"
        "        Version: 3 (0x2)\n"
        "        Serial Number: 81985529216486895 (0x123456789abcdef)\n"
        "    Signature Algorithm: sha256WithRSAEncryption\n"
        "        Issuer: C=JP, CN=AnyPS5 CA\n"
        "        Validity\n"
        "            Not Before: Nov 17 20:01:04 2026 GMT\n"
        "            Not After : Nov 14 20:01:04 2036 GMT\n"
        "        Subject: C=JP, CN=AnyPS5 Server\n"
        "        Subject Public Key Info:\n"
        "            Public Key Algorithm: id-ecPublicKey\n"
        "                Public-Key: (256 bit)\n"
        "                pub: \n"
        "                    04:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:\n"
        "                    ff:00:10:20:30:40:50:60:70:80:90:a0:b0:c0:d0:\n"
        "                    e0:f0:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:\n"
        "                    0e:0f:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:\n"
        "                    1e:1f:21:22:23\n"
        "                ASN1 OID: prime256v1\n"
        "                NIST CURVE: P-256\n"
        "    Signature Algorithm: sha256WithRSAEncryption\n"
        "         30:45:02:21:00:83:08:a4:a1:7a:2e:64:c3:69:09:5e:fa:fd:\n"
        "         74:6f\n";
    Require(printed == expected, "EC P-256 certificate literal mismatch");
}

void CheckDsaSignature() {
    CertificateFixture fixture;
    std::memcpy(fixture.sigAlgOid, OidDsaWithSha1, sizeof(OidDsaWithSha1));
    fixture.sigAlgObj.length = sizeof(OidDsaWithSha1);
    const unsigned char dsaSigDer[] = {
        0x30, 0x12,
        0x02, 0x02, 0x12, 0x34,
        0x02, 0x0c, 0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    };
    std::memcpy(fixture.signatureBytes, dsaSigDer, sizeof(dsaSigDer));
    fixture.signature.length = sizeof(dsaSigDer);
    CaptureBio bio;
    printed.clear();
    const std::uint64_t sigOnly =
        X509FlagNoHeader | X509FlagNoVersion | X509FlagNoSerial | X509FlagNoSigName |
        X509FlagNoIssuer | X509FlagNoValidity | X509FlagNoSubject | X509FlagNoPubkey |
        X509FlagNoExtensions | X509FlagNoAux;
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, sigOnly) == 1);
    const std::string expectedDsa =
        "    Signature Algorithm: dsaWithSHA1\n"
        "         r:    4660 (0x1234)\n"
        "         s:    (Negative)\n"
        "             7f:fe:fd:fc:fb:fa:f9:f8:f7:f6:f5:f5\n";
    Require(printed == expectedDsa, "DSA DER signature r/s literal mismatch");

    const unsigned char badDsaSig[] = {0x01, 0x02, 0x03};
    std::memcpy(fixture.signatureBytes, badDsaSig, sizeof(badDsaSig));
    fixture.signature.length = sizeof(badDsaSig);
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, sigOnly) == 1);
    const std::string expectedFallback =
        "    Signature Algorithm: dsaWithSHA1\n"
        "         01:02:03\n";
    Require(printed == expectedFallback, "malformed DSA signature fallback mismatch");
}

void CheckFlagsAndNameFormats() {
    CertificateFixture fixture;
    CaptureBio bio;

    const std::array<std::uint64_t, 10> flags{
        X509FlagNoHeader, X509FlagNoVersion, X509FlagNoSerial, X509FlagNoSigName,
        X509FlagNoIssuer, X509FlagNoValidity, X509FlagNoSubject, X509FlagNoPubkey,
        X509FlagNoExtensions, X509FlagNoSigDump,
    };
    const std::string full = ExpectedRsaFull();
    for (const std::uint64_t flag : flags) {
        printed.clear();
        Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, flag) == 1);
        Require(printed.size() < full.size(), "individual NO_* flag should omit output");
    }

    const std::uint64_t allOff =
        X509FlagNoHeader | X509FlagNoVersion | X509FlagNoSerial | X509FlagNoSigName |
        X509FlagNoIssuer | X509FlagNoValidity | X509FlagNoSubject | X509FlagNoPubkey |
        X509FlagNoExtensions | X509FlagNoSigDump | X509FlagNoAux;
    printed.clear();
    Require(X509_print_ex_nid_postfix(nullptr, &fixture.cert, X509FlagCompat, allOff) == 1);
    Require(printed.empty());

    const std::uint64_t issuerSubjectOnly = allOff & ~(X509FlagNoIssuer | X509FlagNoSubject);
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, XnFlagRfc2253, issuerSubjectOnly) == 1);
    Require(printed == "        Issuer: CN=AnyPS5 CA,C=JP\n        Subject: CN=AnyPS5 Server,C=JP\n");

    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, XnFlagOneline, issuerSubjectOnly) == 1);
    Require(printed == "        Issuer: C = JP, CN = AnyPS5 CA\n        Subject: C = JP, CN = AnyPS5 Server\n");

    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, XnFlagMultiline, issuerSubjectOnly) == 1);
    const std::string expectedMultiline =
        "        Issuer:\n"
        "            countryName               = JP\n"
        "            commonName                = AnyPS5 CA\n"
        "        Subject:\n"
        "            countryName               = JP\n"
        "            commonName                = AnyPS5 Server\n";
    Require(printed == expectedMultiline, "multiline XN_FLAG_MULTILINE literal mismatch");

    const std::uint64_t extOnly = allOff & ~X509FlagNoExtensions;
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, extOnly | ExtErrorUnknown) == 1);
    Require(printed.find("<Not Supported>") != std::string::npos);

    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, extOnly | ExtDumpUnknown) == 1);
    Require(printed.find("0000 - 61 62 63 01") != std::string::npos);
}

void CheckSerialVersionTimeAndUnloadableKey() {
    CertificateFixture fixture;
    CaptureBio bio;
    const std::uint64_t verSerValOnly =
        X509FlagNoHeader | X509FlagNoSigName | X509FlagNoIssuer |
        X509FlagNoSubject | X509FlagNoPubkey | X509FlagNoExtensions |
        X509FlagNoSigDump | X509FlagNoAux;

    fixture.info.version = nullptr;
    fixture.serial = Asn1String{1, 0x102, fixture.serialBytes, 0};
    fixture.serialBytes[0] = 2;
    std::memcpy(fixture.notBeforeBytes, "20261117200104Z", 15);
    fixture.notBefore = Asn1String{15, 24, fixture.notBeforeBytes, 0};
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, verSerValOnly) == 1);
    const std::string expected1 =
        "        Version: 1 (0x0)\n"
        "        Serial Number: -2 (-0x2)\n"
        "        Validity\n"
        "            Not Before: Nov 17 20:01:04 2026 GMT\n"
        "            Not After : Nov 14 20:01:04 2036 GMT\n";
    Require(printed == expected1);

    fixture.serial = Asn1String{1, 0x102, fixture.serialBytes, 0};
    fixture.serialBytes[0] = 1;
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, verSerValOnly | X509FlagNoVersion | X509FlagNoValidity) == 1);
    Require(printed == "        Serial Number:\n             (Negative)01\n");

    const unsigned char msbSerial[8] = {0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    std::memcpy(fixture.serialBytes, msbSerial, 8);
    fixture.serial = Asn1String{8, 2, fixture.serialBytes, 0};
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, verSerValOnly | X509FlagNoVersion | X509FlagNoValidity) == 1);
    Require(printed == "        Serial Number:\n            80:01:02:03:04:05:06:07\n");

    fixture.pubKeyData[0] = 0x05;
    fixture.pubKeyBits.length = 1;
    printed.clear();
    const std::uint64_t pubOnly = X509FlagNoHeader | X509FlagNoVersion | X509FlagNoSerial |
        X509FlagNoSigName | X509FlagNoIssuer | X509FlagNoValidity | X509FlagNoSubject |
        X509FlagNoExtensions | X509FlagNoSigDump | X509FlagNoAux;
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, pubOnly) == 1);
    Require(printed ==
        "        Subject Public Key Info:\n"
        "            Public Key Algorithm: rsaEncryption\n"
        "            Unable to load Public Key\n");

    fixture.notAfter.length = 2;
    printed.clear();
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, verSerValOnly | X509FlagNoVersion | X509FlagNoSerial) == 0);
    Require(printed.find("Bad time value") != std::string::npos);
}

void CheckErrorsAndGuardPages() {
    CertificateFixture fixture;
    CaptureBio bio;

    printed.clear();
    Require(X509_print_ex_nid_postfix(nullptr, &fixture.cert, X509FlagCompat, X509FlagCompat) == 0);
    Require(printed.empty());

    writesBeforeFailure = 0;
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, X509FlagCompat) == 0);
    Require(printed.empty());

    writesBeforeFailure = 12;
    Require(X509_print_ex_nid_postfix(&bio.bio, &fixture.cert, X509FlagCompat, X509FlagCompat) == 0);
    Require(!printed.empty() && printed != ExpectedRsaFull());
    writesBeforeFailure = -1;

    RequireThrows([] { X509_print_ex_nid_postfix(nullptr, nullptr, X509FlagCompat, X509FlagCompat); }, "null cert");

    {
        CertificateFixture bad;
        bad.cert.certificateInfo = nullptr;
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "null info");
    }
    {
        CertificateFixture bad;
        bad.info.serialNumber = nullptr;
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "null serial");
    }
    {
        CertificateFixture bad;
        bad.info.validity = nullptr;
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "null validity");
    }
    {
        CertificateFixture bad;
        bad.info.key = nullptr;
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "null pubkey");
    }
    {
        CertificateFixture bad;
        std::memcpy(bad.sigAlgOid, OidRsaSsaPss, sizeof(OidRsaSsaPss));
        bad.sigAlgObj.length = sizeof(OidRsaSsaPss);
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "RSA-PSS");
    }

#ifdef _WIN32
    void* guardPage = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
    Require(guardPage != nullptr);
    const auto freeGuard = [guardPage] { VirtualFree(guardPage, 0, MEM_RELEASE); };
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    void* guardPage = mmap(nullptr, static_cast<std::size_t>(pageSize), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(guardPage != MAP_FAILED);
    const auto freeGuard = [guardPage, pageSize] { munmap(guardPage, static_cast<std::size_t>(pageSize)); };
#endif

    errno = 42;
    RequireThrows([&] {
        X509_print_ex_nid_postfix(&bio.bio, static_cast<const LibreSslX509*>(guardPage), X509FlagCompat, X509FlagCompat);
    }, "unmapped cert");
    Require(errno == 42, "errno preserved on guard page cert");

    {
        CertificateFixture bad;
        bad.info.issuer = guardPage;
        errno = 77;
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "unmapped issuer");
        Require(errno == 77, "errno preserved on guard page issuer");
    }
    {
        CertificateFixture bad;
        bad.pubkey.publicKey = static_cast<const Asn1String*>(guardPage);
        errno = 99;
        RequireThrows([&] { X509_print_ex_nid_postfix(&bio.bio, &bad.cert, X509FlagCompat, X509FlagCompat); }, "unmapped spk");
        Require(errno == 99, "errno preserved on guard page publicKey");
    }
    freeGuard();
}

} // namespace

int main() {
    RegisterGuestHeap();
    CheckFullRsaAndAttachedPkey();
    CheckEcCertificate();
    CheckDsaSignature();
    CheckFlagsAndNameFormats();
    CheckSerialVersionTimeAndUnloadableKey();
    CheckErrorsAndGuardPages();
    Require(liveBlocks == 0, "guest heap leak detected");
    std::printf("X509_print_ex checks passed\n");
    return 0;
}
