#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

struct EncodingView {
    unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

static_assert(sizeof(LibreSslX509Object) == 16);
static_assert(offsetof(LibreSslX509Object, data) == 8);
static_assert(sizeof(EncodingView) == sizeof(LibreSsl3CertificateInfo::encoding));
static_assert(offsetof(LibreSsl3CertificateInfo, encoding) == 80);

namespace {

constexpr const char* CertificateA = R"(-----BEGIN CERTIFICATE-----
MIIC6DCCAdCgAwIBAgIUeK9Zpva5JibdnjUZA7odWaOfRnwwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJVGVzdCBDQSBBMB4XDTI2MTExOTA4NTYxOVoXDTM2MTEx
NjA4NTYxOVowFDESMBAGA1UEAwwJVGVzdCBDQSBBMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEArKBNG5XW9j3heVjZO0YOVixlkfyvhLiqzpYYKcNB32k+
/tZ5uFgQC21fdObmLHvYr7v4SF1b7uM8q5DGCKPdDrOBK1yMdztKVIqYQB2Fa1QW
uo4s1Q/uBqr+j72h17+/NNEV6a6RS2OsWI091mVpHrLINItEGRGRs6D4f9vOjNFN
KsozZEqxg7Ar2PhnfaIHV2Rp0JazcERAk9Vflq950ISG9vOyV99n3KgEPKhxCwDz
d55so+U6T0pVDYHtEzSqjCxetu43bjlhHs1g5CKA+466UY5rKtrccoRk1/JkLUiU
bbJ6pdWLqHjo/TgUez++C8+thZW5PGq8ptogNzTmwwIDAQABozIwMDAdBgNVHQ4E
FgQU8wBut66F951+Hhw59y8uHKMZ4yAwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG
9w0BAQsFAAOCAQEAIKboUi8B4X4MSibT9VyQlXKU8QyPm4WCaReQkc17Lg5M2yBa
sYEjeqPMdyWPXVfms+MIb4f2y++ju5zvcsQrXnOLAo89HrsIcSHT564wAv2TghUM
Fx1USJz8zx1J12GRSAZ1ULQNq8Qd4UrXFIxu/wgS1MrwUQdVCPj0oyqE8KFrgocO
sI+nTe/lpLV++YmNHkCGMwTni3kLalZliQh20oH9vWx2BcFcSBNh7hml96vLPT0i
Vz2HDR5hw0Yd0wPjNn0TITHxILbGzmtjrjlNBW7rrcwtXKsTwcEeKL9ie0h3eavl
Ik0q4TE0C6m4ecQqZXyxa26U52y7kLa/YbXHUw==
-----END CERTIFICATE-----
)";

constexpr const char* CertificateB = R"(-----BEGIN CERTIFICATE-----
MIIC6DCCAdCgAwIBAgIUTTr1/kODYPsvpxduWjHc7noozEAwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJVGVzdCBDQSBCMB4XDTI2MTExOTA4NTYxOVoXDTM2MTEx
NjA4NTYxOVowFDESMBAGA1UEAwwJVGVzdCBDQSBCMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAgO8tQtPQKVAuv6DCe1YhHAaIaIoU4WS+Bc8oyJyl1Gfq
FSOiSb7eP8tj4Qg+MYFVTfh1CzX1a2Ya9RJmpc/1NdbO9h4TDsfWWgd+lVuVd1dM
9a7X5uG+SfMzNeBNxR/+PJluUO60gEjemaoMnkKSSKlKxtSYOLVxAG2iu3l4XTsf
DdJEnGUDPCUfEXlXvpH+SPywTBnK5tDxHoM1oJuaRc+3L4zInOTmmiFPPI9S33Zb
34ssBQHkNclqu3AL4D/YyqlgO5tAK0ouSivVGiXQgqLhIuf2brggiH6oRhM/HawT
N2p7VtRNlrTk2z9V1zLnoT9rOHEzYVMkSmBv+h0UHwIDAQABozIwMDAdBgNVHQ4E
FgQUf7hPIsjr0XdjoE1CqXrJgp6uvN0wDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG
9w0BAQsFAAOCAQEASJBn1kpV/F3UwqL1u0Rn7SXj6Ok3woBIrTogB0exmRJAEBrq
oG/ELtgXVCJSWPAZ4OKILjunekYknlZ0PyVDStDqNQO01zvJ+2Chf3zOE3Uu5f8S
rA+set2cj2+5QvkM+w2daqxLWozSmwkgzaDgmGtfQK/owEjJNuUK/9BPyh5o5kgw
5yBkGHYCDNmXCYpVe/CYpo/mxbATCDgn7/lQf3fGV1NchoM/S8VmltVKMv6S8vTe
bEmk9sVo0nLL/+iI7WWLB2aNxTfsG3ZF/hZDAcaxPnTkUOOSMaziVzMQX6dYtZi8
HUjYGQdeM1aWaa04GOwWioxt/yZiPQGDxfuNtA==
-----END CERTIFICATE-----
)";

constexpr const char* OtherCertificate = R"(-----BEGIN CERTIFICATE-----
MIIC6DCCAdCgAwIBAgIUTTr1/kODYPsvpxduWjHc7noozEAwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJVGVzdCBDQSBCMB4XDTI2MTExOTA4NTYxOVoXDTM2MTEx
NjA4NTYxOVowFDESMBAGA1UEAwwJVGVzdCBDQSBCMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAgO8tQtPQKVAuv6DCe1YhHAaIaIoU4WS+Bc8oyJyl1Gfq
FSOiSb7eP8tj4Qg+MYFVTfh1CzX1a2Ya9RJmpc/1NdbO9h4TDsfWWgd+lVuVd1dM
9a7X5uG+SfMzNeBNxR/+PJluUO60gEjemaoMnkKSSKlKxtSYOLVxAG2iu3l4XTsf
DdJEnGUDPCUfEXlXvpH+SPywTBnK5tDxHoM1oJuaRc+3L4zInOTmmiFPPI9S33Zb
34ssBQHkNclqu3AL4D/YyqlgO5tAK0ouSivVGiXQgqLhIuf2brggiH6oRhM/HawT
N2p7VtRNlrTk2z9V1zLnoT9rOHEzYVMkSmBv+h0UHwIDAQABozIwMDAdBgNVHQ4E
FgQUf7hPIsjr0XdjoE1CqXrJgp6uvN0wDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG
9w0BAQsFAAOCAQEASJBn1kpV/F3UwqL1u0Rn7SXj6Ok3woBIrTogB0exmRJAEBrq
oG/ELtgXVCJSWPAZ4OKILjunekYknlZ0PyVDStDqNQO01zvJ+2Chf3zOE3Uu5f8S
rA+set2cj2+5QvkM+w2daqxLWozSmwkgzaDgmGtfQK/owEjJNuUK/9BPyh5o5kgw
5yBkGHYCDNmXCYpVe/CYpo/mxbATCDgn7/lQf3fGV1NchoM/S8VmltVKMv6S8vTe
bEmk9sVo0nLL/+iI7WWLB2aNxTfsG3ZF/hZDAcaxPnTkUOOSMaziVzMQX6dYtZi8
HUjYGQdeM1aWaa04GOwWioxt/yZiPQGDxfuNtA==
-----END CERTIFICATE-----
)";

const unsigned char SubjectEncodingA[] = {0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x09, 't', 'e', 's', 't', ' ', 'c', 'a', ' ', 'a'};
const unsigned char SubjectEncodingB[] = {0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x09, 't', 'e', 's', 't', ' ', 'c', 'a', ' ', 'b'};
const unsigned char SubjectA[] = "Test CA A";
const unsigned char OidCommonName[] = {0x55, 0x04, 0x03};
constexpr const char* HashedNameA = "8deb836d.0";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_STORE_load_locations check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size);
    Require(result != nullptr);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

std::string WriteFile(const std::string& path, const std::string& contents) {
    FileStream* stream = fopen_nid_postfix(path.c_str(), "wb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix(contents.data(), 1, contents.size(), stream) == contents.size());
    Require(fclose_nid_postfix(stream) == 0);
    return path;
}

int ObjectCount(const LibreSslX509Store* store) {
    const auto* stack = static_cast<const LibreSslStack*>(store->objects);
    Require(stack != nullptr && stack->num >= 0);
    return stack->num;
}

int LookupCount(const LibreSslX509Store* store) {
    const auto* stack = static_cast<const LibreSslStack*>(store->lookupMethods);
    Require(stack != nullptr && stack->num >= 0);
    return stack->num;
}

LibreSslX509Store* NewStore() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    Require(ObjectCount(store) == 0);
    Require(LookupCount(store) == 0);
    return store;
}

const LibreSslX509Object* StoredObject(const LibreSslX509Store* store, int index) {
    const auto* stack = static_cast<const LibreSslStack*>(store->objects);
    Require(stack != nullptr && index < stack->num && stack->data != nullptr);
    return reinterpret_cast<const LibreSslX509Object*>(stack->data[index]);
}

void TestFileBundle() {
    const std::string path = WriteFile("libressl3-load-locations-bundle.pem", std::string(CertificateA) + CertificateB);
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
    Require(ObjectCount(store) == 2);
    Require(LookupCount(store) == 1);
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
    Require(ObjectCount(store) == 2);
    Require(LookupCount(store) == 1);
    for (int index = 0; index < 2; ++index) {
        const LibreSslX509Object* object = StoredObject(store, index);
        Require(object != nullptr && object->type == LibreSslX509LookupX509 && object->data != nullptr);
        const auto* certificate = static_cast<const LibreSsl3X509*>(object->data);
        Require(certificate->references == 1);
        Require(certificate->certInfo != nullptr);
        const auto* info = reinterpret_cast<const LibreSsl3CertificateInfo*>(certificate->certInfo);
        const auto* subject = static_cast<const LibreSslX509Name*>(info->subject);
        const unsigned char* const expectedSubject = index == 0 ? SubjectEncodingA : SubjectEncodingB;
        Require(subject != nullptr && subject->canonEnclen == static_cast<int>(sizeof(SubjectEncodingA)));
        Require(subject->canonEnclen == 20 && subject->canonEnc != nullptr);
        Require(std::memcmp(subject->canonEnc, expectedSubject, sizeof(SubjectEncodingA)) == 0);
        const auto* encoding = reinterpret_cast<const EncodingView*>(info->encoding);
        Require(encoding->length > 0 && encoding->bytes != nullptr && encoding->modified == 0);
        Require(encoding->bytes[0] == 0x30);
        Require(i2d_X509_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(object->data), nullptr) == 748);
        unsigned char* der = nullptr;
        Require(i2d_X509_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(object->data), &der) == 748);
        Require(der != nullptr && der[0] == 0x30 && der[1] == 0x82);
        unsigned char derDigest[20];
        Require(SHA1_nid_postfix(der, 748, derDigest) != nullptr);
        const char* expected = index == 0 ? "a235b4a5a3cc3122ee1fe86e5c5b623565cc77b4" : "0cdf2b753422b68659f724dece2f4bf3f3e56839";
        char text[41];
        for (std::size_t byte = 0; byte < sizeof(derDigest); ++byte)
            std::snprintf(text + byte * 2, 3, "%02x", derDigest[byte]);
        Require(std::string(text) == expected);
        free_nid_postfix(der);
    }
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    Require(remove_nid_postfix(path.c_str()) == 0);
}

void TestDuplicateCertificate() {
    const std::string path = WriteFile("libressl3-load-locations-duplicate.pem", std::string(CertificateA) + CertificateA);
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
    Require(ObjectCount(store) == 1);
    Require(StoredObject(store, 0) != nullptr);
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
    Require(ObjectCount(store) == 1);
    Require(LookupCount(store) == 1);
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    Require(remove_nid_postfix(path.c_str()) == 0);
}

void TestNullArguments() {
    Require(X509_STORE_load_locations_nid_postfix(nullptr, nullptr, nullptr) == 0);
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, nullptr) == 0);
    Require(LookupCount(store) == 0);
    X509_STORE_free_nid_postfix(store);
    Require(Throws([] { X509_STORE_load_locations_nid_postfix(nullptr, "missing.pem", nullptr); }));
}

void TestMissingFile() {
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, "libressl3-load-locations-missing.pem", nullptr) == 0);
    Require(ObjectCount(store) == 0);
    X509_STORE_free_nid_postfix(store);
}

void TestEmptyAndMalformedFiles() {
    const std::string empty = WriteFile("libressl3-load-locations-empty.pem", "no certificates here\n");
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, empty.c_str(), nullptr) == 0);
    X509_STORE_free_nid_postfix(store);
    Require(remove_nid_postfix(empty.c_str()) == 0);

    const std::string malformed = WriteFile("libressl3-load-locations-malformed.pem",
        "-----BEGIN CERTIFICATE-----\nMIIB!!!\n-----END CERTIFICATE-----\n");
    store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, malformed.c_str(), nullptr) == 0);
    X509_STORE_free_nid_postfix(store);
    Require(remove_nid_postfix(malformed.c_str()) == 0);

    const std::string truncated = WriteFile("libressl3-load-locations-truncated.pem",
        std::string("-----BEGIN CERTIFICATE-----\n") + "MIIC6DCCAdCgAwIBAgIUeK9Zpva5JibdnjUZA7odWaOfRnw=\n");
    store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, truncated.c_str(), nullptr) == 0);
    X509_STORE_free_nid_postfix(store);
    Require(remove_nid_postfix(truncated.c_str()) == 0);

    const std::string notACertificate = WriteFile("libressl3-load-locations-key.pem", R"(-----BEGIN PRIVATE KEY-----
MIIB
-----END PRIVATE KEY-----
)");
    store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, notACertificate.c_str(), nullptr) == 0);
    X509_STORE_free_nid_postfix(store);
    Require(remove_nid_postfix(notACertificate.c_str()) == 0);
}

constexpr const char* RevocationList = R"(-----BEGIN X509 CRL-----
MIGuMFUCAQEwCgYIKoZIzj0EAwIwFjEUMBIGA1UEAwwLVGVzdCBDUkwgQ0EXDTI2
MTEyMTE1MjI1NVoXDTM2MTExODE1MjI1NVqgDjAMMAoGA1UdFAQDAgEBMAoGCCqG
SM49BAMCA0kAMEYCIQCP12Sr5s2pjAdUMvd6yUFCtgwdJWwkFWmJscniYJWVXAIh
ANO8Nc/Po/a1Gxr3TdbTUv0sRf/cj173cilVRS8CHN2l
-----END X509 CRL-----
)";

using Codes = std::vector<std::uint64_t>;

constexpr std::uint64_t Error(std::uint64_t library, std::uint64_t reason) {
    return library << 24 | 0xfffull << 12 | reason;
}

constexpr std::uint64_t EarlierError = Error(20, 67);
constexpr std::uint64_t PemNoStartLine = Error(9, 108);
constexpr std::uint64_t PemBadEndLine = Error(9, 102);
constexpr std::uint64_t PemBadBase64 = Error(9, 100);
constexpr std::uint64_t PemAsn1 = Error(9, 13);
constexpr std::uint64_t X509Pem = Error(11, 9);
constexpr std::uint64_t X509System = Error(11, 2);
constexpr std::uint64_t X509NothingFound = Error(11, 136);
constexpr std::uint64_t SystemNoEntry = Error(2, 2);
constexpr std::uint64_t SystemNotDirectory = Error(2, 20);
constexpr std::uint64_t BioNoSuchFile = Error(32, 128);
constexpr std::uint64_t BioSystem = Error(32, 2);

Codes Drain() {
    Codes codes;
    while (const std::uint64_t code = ERR_get_error_nid_postfix()) codes.push_back(code);
    return codes;
}

Codes ErrorsOf(const std::string& path, std::uint64_t earlier, int expectedResult, int expectedObjects) {
    LibreSslX509Store* store = NewStore();
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(static_cast<int>(earlier >> 24), 0xfff, static_cast<int>(earlier & 0xfff));
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == expectedResult);
    const Codes codes = Drain();
    Require(ObjectCount(store) == expectedObjects);
    Throws([store] { X509_STORE_free_nid_postfix(store); });
    return codes;
}

Codes ErrorsAfter(const std::string& contents, std::uint64_t earlier, int expectedResult, int expectedObjects) {
    const std::string path = WriteFile("libressl3-load-locations-errors.pem", contents);
    const Codes codes = ErrorsOf(path, earlier, expectedResult, expectedObjects);
    Require(remove_nid_postfix(path.c_str()) == 0);
    return codes;
}

void TestFileErrorQueue() {
    const std::string unclosed = "-----BEGIN CERTIFICATE-----\nMIIB\n";
    const std::string badBase64 = "-----BEGIN CERTIFICATE-----\nMIIB!!!\n-----END CERTIFICATE-----\n";
    const std::string badDer = "-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----\n";
    const std::string emptyRecord = "-----BEGIN CERTIFICATE-----\n-----END CERTIFICATE-----\n";
    const std::string otherRecord = "-----BEGIN PRIVATE KEY-----\nMIIB\n-----END PRIVATE KEY-----\n";

    Require(ErrorsAfter(std::string(CertificateA) + CertificateB, EarlierError, 1, 2).empty());
    Require(ErrorsAfter(std::string(CertificateA) + CertificateA, EarlierError, 1, 1).empty());
    Require(ErrorsAfter(std::string(CertificateA) + otherRecord + RevocationList, EarlierError, 1, 1).empty());

    Require(ErrorsAfter("", EarlierError, 0, 0) == Codes{X509NothingFound});
    Require(ErrorsAfter("no certificates here\n", EarlierError, 0, 0) == Codes{X509NothingFound});
    Require(ErrorsAfter(otherRecord, EarlierError, 0, 0) == Codes{X509NothingFound});
    Require(ErrorsAfter(RevocationList, EarlierError, 0, 0).empty());

    Require(ErrorsAfter(unclosed, EarlierError, 0, 0) == (Codes{EarlierError, PemBadEndLine, X509Pem}));
    Require(ErrorsAfter(std::string(CertificateA) + unclosed, EarlierError, 0, 0)
        == (Codes{EarlierError, PemBadEndLine, X509Pem}));
    Require(ErrorsAfter(badBase64, EarlierError, 0, 0) == (Codes{EarlierError, PemBadBase64, X509Pem}));
    Require(ErrorsAfter(std::string(CertificateA) + badBase64 + CertificateB, EarlierError, 0, 0)
        == (Codes{EarlierError, PemBadBase64, X509Pem}));

    const Codes decode = ErrorsAfter(std::string(CertificateA) + badDer, EarlierError, 0, 0);
    Require(decode.size() >= 4 && decode.front() == EarlierError);
    Require(decode[decode.size() - 2] == PemAsn1 && decode.back() == X509Pem);
    for (std::size_t index = 1; index + 2 < decode.size(); ++index) Require(decode[index] >> 24 == 13);

    Require(ErrorsAfter(emptyRecord, EarlierError, 0, 0) == (Codes{EarlierError, X509Pem}));
    Require(ErrorsAfter(std::string(CertificateA) + emptyRecord + CertificateB, EarlierError, 0, 0)
        == (Codes{EarlierError, X509Pem}));
    Require(ErrorsAfter(emptyRecord, PemNoStartLine, 0, 0) == Codes{X509NothingFound});
    Require(ErrorsAfter(emptyRecord, Error(20, 108), 0, 0) == Codes{X509NothingFound});
    Require(ErrorsAfter(emptyRecord, PemBadEndLine, 0, 0) == (Codes{PemBadEndLine, X509Pem}));
    Require(ErrorsAfter(std::string(CertificateA) + emptyRecord + CertificateB, PemNoStartLine, 1, 1).empty());
    Require(ErrorsAfter(emptyRecord + RevocationList, PemNoStartLine, 0, 0) == Codes{X509NothingFound});
    Require(ErrorsAfter(unclosed, PemNoStartLine, 0, 0) == (Codes{PemNoStartLine, PemBadEndLine, X509Pem}));

    Require(ErrorsOf("libressl3-load-locations-absent.pem", EarlierError, 0, 0)
        == (Codes{EarlierError, SystemNoEntry, BioNoSuchFile, X509System}));
    const std::filesystem::path plain = std::filesystem::temp_directory_path() / "anyps5-libressl3-load-locations-plain";
    std::FILE* hostFile = std::fopen(plain.string().c_str(), "wb");
    Require(hostFile != nullptr);
    Require(std::fclose(hostFile) == 0);
    AddPathAlias_nid_no_patch("/load-locations-plain", plain.string().c_str());
    Require(ErrorsOf("/load-locations-plain/inner.pem", EarlierError, 0, 0)
        == (Codes{EarlierError, SystemNotDirectory, BioSystem, X509System}));
    RemovePathAlias_nid_no_patch("/load-locations-plain");
    std::error_code ignored;
    std::filesystem::remove(plain, ignored);

    LibreSslX509Store* store = NewStore();
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    Require(X509_STORE_load_locations_nid_postfix(store, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(1)), nullptr) == 0);
    Require(Drain() == Codes{EarlierError});
    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, nullptr) == 0);
    Require(Drain().empty());
    X509_STORE_free_nid_postfix(store);
}

std::string WithLineEnding(const char* pem, const std::string& ending) {
    std::string result;
    for (const char* p = pem; *p; ++p) {
        if (*p == '\n') result += ending;
        else result.push_back(*p);
    }
    return result;
}

int LoadedFrom(const std::string& name, const std::string& contents, int expectedResult) {
    const std::string path = WriteFile(name, contents);
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == expectedResult);
    const int count = ObjectCount(store);
    if (count) Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    else X509_STORE_free_nid_postfix(store);
    Require(remove_nid_postfix(path.c_str()) == 0);
    return count;
}

void TestPemLines() {
    Require(LoadedFrom("libressl3-load-locations-spaces.pem", WithLineEnding(CertificateA, " \n"), 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-tabs.pem", WithLineEnding(CertificateA, " \t \r\n"), 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-crcrlf.pem", WithLineEnding(CertificateA, "\r\r\n"), 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-control.pem", WithLineEnding(CertificateA, "\x01\x1F\n"), 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-high.pem", WithLineEnding(CertificateA, "\xA0\xFF\n"), 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-two.pem", WithLineEnding(CertificateA, "  \n") + WithLineEnding(CertificateB, "\t\n"), 1) == 2);
    Require(LoadedFrom("libressl3-load-locations-inner.pem", std::string("-----BEGIN CERTIFICATE ----- \n") + CertificateA, 0) == 0);
    Require(LoadedFrom("libressl3-load-locations-open-begin.pem", std::string("-----BEGIN CERTIFICATE\n") + CertificateA, 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-short-begin.pem", std::string("-----BEGIN ----\n-----BEGIN CERTIFICATE---\n") + CertificateA, 1) == 1);
    Require(LoadedFrom("libressl3-load-locations-only-open.pem", "-----BEGIN CERTIFICATE\nMIIB\n", 0) == 0);
    const std::string broken = "-----BEGIN CERTIFICATE-----\nMIIB!!!\n-----END CERTIFICATE-----\n";
    Require(LoadedFrom("libressl3-load-locations-dropped.pem", std::string(CertificateA) + broken, 0) == 0);
    Require(LoadedFrom("libressl3-load-locations-dropped-two.pem", std::string(CertificateA) + CertificateB + broken + CertificateA, 0) == 0);
    Require(LoadedFrom("libressl3-load-locations-unclosed.pem", std::string(CertificateA) + "-----BEGIN CERTIFICATE-----\nMIIB\n", 0) == 0);
    Require(LoadedFrom("libressl3-load-locations-first-broken.pem", broken + CertificateA, 0) == 0);
    Require(LoadedFrom("libressl3-load-locations-end-label.pem", "-----BEGIN CERTIFICATE-----\nMIIB\n-----END X509 CRL-----\n", 0) == 0);
}

void TestUnreadablePath() {
    LibreSslX509Store* store = NewStore();
    const auto unreadable = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(1));
    Require(X509_STORE_load_locations_nid_postfix(store, unreadable, nullptr) == 0);
    X509_STORE_free_nid_postfix(store);
    store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, unreadable) == 0);
    Require(LookupCount(store) == 1);
    X509_STORE_free_nid_postfix(store);
}

std::string HashedFileName(const unsigned char* encoding, std::size_t length) {
    unsigned char digest[20];
    Require(SHA1_nid_postfix(encoding, length, digest) != nullptr);
    const unsigned long hash = static_cast<unsigned long>(digest[0]) |
        (static_cast<unsigned long>(digest[1]) << 8) | (static_cast<unsigned long>(digest[2]) << 16) |
        (static_cast<unsigned long>(digest[3]) << 24);
    char name[32];
    const int written = std::snprintf(name, sizeof(name), "%08lx.0", hash);
    Require(written > 0 && static_cast<std::size_t>(written) < sizeof(name));
    return name;
}

void TestHashedDirectory() {
    Require(std::string(HashedFileName(SubjectEncodingA, sizeof(SubjectEncodingA))) == HashedNameA);
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "anyps5-libressl3-hashed-ca";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    Require(std::filesystem::create_directories(directory));
    AddPathAlias_nid_no_patch("/ca-test", directory.string().c_str());
    const std::string guestDir = "/ca-test";
    WriteFile(guestDir + "/" + HashedNameA, CertificateA);

    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, guestDir.c_str()) == 1);
    Require(ObjectCount(store) == 0);
    Require(LookupCount(store) == 1);
    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, guestDir.c_str()) == 1);
    Require(LookupCount(store) == 1);

    const auto* lookups = static_cast<const LibreSslStack*>(store->lookupMethods);
    auto* lookup = reinterpret_cast<LibreSslX509Lookup*>(lookups->data[0]);
    Require(lookup != nullptr && lookup->storeContext == store && lookup->methodData != nullptr);
    Require(lookup->method != nullptr && lookup->method->getBySubject != nullptr);

    Asn1Object_nid_no_patch object{};
    object.length = static_cast<int>(sizeof(OidCommonName));
    object.data = OidCommonName;
    Asn1String value{};
    value.length = static_cast<int>(sizeof(SubjectA) - 1);
    value.type = 12;
    value.data = const_cast<unsigned char*>(SubjectA);
    LibreSslX509NameEntry entry{&object, &value, 0, 0};
    LibreSslStack* entries = sk_new_nid_postfix(nullptr);
    Require(entries != nullptr);
    Require(sk_push_nid_postfix(entries, &entry) > 0);
    LibreSslX509Name name{entries, 0, nullptr, nullptr, 0};
    LibreSslX509Object result{};
    const auto bySubject = reinterpret_cast<int (APS5_VABI*)(LibreSslX509Lookup*, int, LibreSslX509Name*, LibreSslX509Object*)>(
        lookup->method->getBySubject);
    ERR_clear_error_nid_postfix();
    Require(bySubject(lookup, LibreSslX509LookupX509, &name, &result) == 1);
    Require(Drain().empty());
    Require(result.type == LibreSslX509LookupX509 && result.data != nullptr);
    Require(ObjectCount(store) == 1);
    Require(result.data == StoredObject(store, 0)->data);
    Require(bySubject(lookup, LibreSslX509LookupX509, nullptr, &result) == 0);

    const std::filesystem::path partialDirectory = std::filesystem::temp_directory_path() / "anyps5-libressl3-hashed-ca-partial";
    std::filesystem::remove_all(partialDirectory, ignored);
    Require(std::filesystem::create_directories(partialDirectory));
    AddPathAlias_nid_no_patch("/ca-partial", partialDirectory.string().c_str());
    WriteFile(std::string("/ca-partial/") + HashedNameA, std::string(CertificateA) + "-----BEGIN CERTIFICATE-----\nMIIB\n");
    LibreSslX509Store* partial = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(partial, nullptr, "/ca-partial") == 1);
    auto* partialLookup = reinterpret_cast<LibreSslX509Lookup*>(static_cast<const LibreSslStack*>(partial->lookupMethods)->data[0]);
    LibreSslX509Object kept{};
    Require(bySubject(partialLookup, LibreSslX509LookupX509, &name, &kept) == 1);
    Require(ObjectCount(partial) == 1 && kept.data == StoredObject(partial, 0)->data);
    Require(Throws([partial] { X509_STORE_free_nid_postfix(partial); }));
    RemovePathAlias_nid_no_patch("/ca-partial");
    std::filesystem::remove_all(partialDirectory, ignored);

    unsigned char bmpSubject[] = {0x00, 'T', 0x00, 'E', 0x00, 'S', 0x00, 'T', 0x00, ' ', 0x00, '\t', 0x00, 'C', 0x00, 'a',
        0x00, ' ', 0x00, 'A', 0x00, ' '};
    value = {static_cast<int>(sizeof(bmpSubject)), 30, bmpSubject};
    result = {};
    Require(bySubject(lookup, LibreSslX509LookupX509, &name, &result) == 1);
    Require(result.data == StoredObject(store, 0)->data);
    value.length = 3;
    Require(Throws([&] { bySubject(lookup, LibreSslX509LookupX509, &name, &result); }));
    sk_free_nid_postfix(entries);

    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, "/nonexistent-anyps5-ca") == 1);
    LibreSslX509Store* empty = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(empty, nullptr, "") == 0);
    X509_STORE_free_nid_postfix(empty);
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    std::filesystem::remove_all(directory, ignored);
}

void TestAddCertificate() {
    const std::string path = WriteFile("libressl3-load-locations-add.pem", CertificateA);
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
    Require(ObjectCount(store) == 1);
    auto* certificate = const_cast<LibreSslX509*>(static_cast<const LibreSslX509*>(StoredObject(store, 0)->data));
    Require(X509_STORE_add_cert_nid_postfix(store, certificate) == 0);
    Require(ObjectCount(store) == 1);
    Require(certificate->references == 1);
    Require(X509_STORE_add_cert_nid_postfix(store, nullptr) == 0);
    Require(Throws([certificate] { X509_STORE_add_cert_nid_postfix(nullptr, certificate); }));
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    Require(remove_nid_postfix(path.c_str()) == 0);
}

bool HoldsCertificates(LibreSslX509Store* store, int expected) {
    return ObjectCount(store) == expected;
}

void TestNegativeControl() {
    const std::string path = WriteFile("libressl3-load-locations-control.pem", std::string(CertificateA) + CertificateB);
    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
    Require(HoldsCertificates(store, 2));
    auto* stack = const_cast<LibreSslStack*>(static_cast<const LibreSslStack*>(store->objects));
    stack->num -= 1;
    Require(!HoldsCertificates(store, 2));
    stack->num += 1;
    Require(HoldsCertificates(store, 2));
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    Require(remove_nid_postfix(path.c_str()) == 0);
}

using Bytes = std::vector<unsigned char>;

constexpr char UtcStart[] = "260101000000Z";
constexpr char UtcEnd[] = "360101000000Z";

Bytes Element(unsigned char tag, const Bytes& content) {
    Bytes output{tag};
    const std::size_t size = content.size();
    Require(size < 0x10000);
    if (size >= 0x100) output.insert(output.end(), {0x82, static_cast<unsigned char>(size >> 8)});
    if (size >= 0x80 && size < 0x100) output.push_back(0x81);
    output.push_back(static_cast<unsigned char>(size));
    output.insert(output.end(), content.begin(), content.end());
    return output;
}

Bytes Concat(std::initializer_list<Bytes> parts) {
    Bytes output;
    for (const Bytes& part : parts) output.insert(output.end(), part.begin(), part.end());
    return output;
}

Bytes Attribute(unsigned char type, unsigned char tag, const Bytes& value) {
    return Element(0x30, Concat({Element(0x06, {0x55, 0x04, type}), Element(tag, value)}));
}

std::string CertificatePem(const Bytes& name) {
    const Bytes algorithm = Element(0x30, Element(0x06, {0x2b, 0x65, 0x70}));
    const Bytes validity = Element(0x30, Concat({Element(0x17, Bytes(UtcStart, UtcStart + 13)),
        Element(0x17, Bytes(UtcEnd, UtcEnd + 13))}));
    const Bytes key = Element(0x30, Concat({algorithm, Element(0x03, Concat({{0x00}, Bytes(32, 0x11)}))}));
    const Bytes info = Element(0x30, Concat({Element(0xa0, Element(0x02, {0x02})), Element(0x02, {0x01}), algorithm,
        name, validity, name, key}));
    const Bytes der = Element(0x30, Concat({info, algorithm, Element(0x03, Concat({{0x00}, Bytes(64, 0x22)}))}));
    static constexpr char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text = "-----BEGIN CERTIFICATE-----\n";
    for (std::size_t index = 0; index < der.size(); index += 3) {
        const std::size_t count = std::min<std::size_t>(3, der.size() - index);
        std::uint32_t group = 0;
        for (std::size_t byte = 0; byte < 3; ++byte) group = (group << 8) | (byte < count ? der[index + byte] : 0);
        for (std::size_t digit = 0; digit < 4; ++digit)
            text.push_back(digit <= count ? Alphabet[(group >> (18 - 6 * digit)) & 0x3f] : '=');
        if ((index / 3 + 1) % 16 == 0) text.push_back('\n');
    }
    if (text.back() != '\n') text.push_back('\n');
    return text + "-----END CERTIFICATE-----\n";
}

const LibreSslX509Name* LoadedName(const LibreSslX509Store* store, bool subject) {
    const auto* certificate = static_cast<const LibreSsl3X509*>(StoredObject(store, 0)->data);
    const auto* info = reinterpret_cast<const LibreSsl3CertificateInfo*>(certificate->certInfo);
    return static_cast<const LibreSslX509Name*>(subject ? info->subject : info->issuer);
}

void TestConvertedNameStrings() {
    const Bytes mixed = Element(0x30, Concat({
        Element(0x31, Attribute(0x03, 30, {0x00, 0x20, 0x00, 0x20, 0x00, 0x45, 0x00, 0xc9, 0x00, 0x74, 0x00, 0xe9, 0x00,
            0x09, 0x00, 0x09, 0x00, 0x58, 0x03, 0xa9, 0x4e, 0x2d, 0x00, 0x20})),
        Element(0x31, Attribute(0x0a, 20, {' ', '\t', ' ', 'A', 'C', 'M', 'E', 0xe9, 0xff, ' ', ' ', 'L', 'a', 'b', ' '})),
        Element(0x31, Concat({Attribute(0x0b, 28, {0, 0, 0, 0x55, 0, 0, 0, 0xdc, 0, 0x01, 0xf6, 0x00, 0, 0, 0, 0x20, 0, 0,
            0, 0x20, 0, 0, 0, 0x5a}), Attribute(0x07, 19, {'C', 'a', 'f', 0xe9})}))}));
    const Bytes canonicalMixed = Concat({
        Element(0x31, Attribute(0x03, 12, {'e', 0xc3, 0x89, 't', 0xc3, 0xa9, ' ', 'x', 0xce, 0xa9, 0xe4, 0xb8, 0xad})),
        Element(0x31, Attribute(0x0a, 12, {'a', 'c', 'm', 'e', 0xc3, 0xa9, 0xc3, 0xbf, ' ', 'l', 'a', 'b'})),
        Element(0x31, Concat({Attribute(0x07, 12, {'c', 'a', 'f', 0xc3, 0xa9}),
            Attribute(0x0b, 12, {'u', 0xc3, 0x9c, 0xf0, 0x9f, 0x98, 0x80, ' ', 'z'})}))});
    const Bytes utf8 = Element(0x30, Element(0x31, Attribute(0x03, 12, {' ', ' ', 0xc3, 0x87, 'a', ' ', ' ', 'V', 'A', ' ',
        0xf0, 0x9f, 0x98, 0x80, ' '})));
    const Bytes canonicalUtf8 = Element(0x31, Attribute(0x03, 12, {0xc3, 0x87, 'a', ' ', 'v', 'a', ' ', 0xf0, 0x9f, 0x98,
        0x80}));
    const Bytes ia5 = Element(0x30, Element(0x31, Attribute(0x03, 22, {'A', 0xe9, 0x80})));
    const Bytes canonicalIa5 = Element(0x31, Attribute(0x03, 12, {'a', 0xc3, 0xa9, 0xc2, 0x80}));
    const struct {
        const Bytes& name;
        const Bytes& canonical;
        const char* hashedName;
    } cases[] = {{mixed, canonicalMixed, "e5631d8b.0"}, {utf8, canonicalUtf8, "70ea2405.0"},
        {ia5, canonicalIa5, "00481cab.0"}};
    for (const auto& test : cases) {
        const std::string path = WriteFile("libressl3-load-locations-strings.pem", CertificatePem(test.name));
        LibreSslX509Store* store = NewStore();
        Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 1);
        Require(ObjectCount(store) == 1);
        for (const bool subject : {true, false}) {
            const LibreSslX509Name* name = LoadedName(store, subject);
            Require(name->canonEnclen == static_cast<int>(test.canonical.size()) && name->canonEnc != nullptr);
            Require(std::memcmp(name->canonEnc, test.canonical.data(), test.canonical.size()) == 0);
            Require(HashedFileName(name->canonEnc, test.canonical.size()) == test.hashedName);
        }
        Throws([store] { X509_STORE_free_nid_postfix(store); });
        Require(remove_nid_postfix(path.c_str()) == 0);
    }
}

void TestRejectedNameStrings() {
    const struct {
        unsigned char tag;
        Bytes value;
        std::uint64_t error;
    } cases[] = {
        {30, {0x00, 0x41, 0x00}, 0x0dfff0d6},
        {30, {0xd8, 0x3d, 0xde, 0x00}, 0x0dfff07c},
        {30, {0x00, 0x41, 0xdc, 0x00}, 0x0dfff07c},
        {28, {0x00, 0x00, 0x00, 0x41, 0x00, 0x00}, 0x0dfff0d7},
        {28, {0x00, 0x11, 0x00, 0x00}, 0x0dfff07c},
        {28, {0x00, 0x00, 0xd8, 0x00}, 0x0dfff07c},
        {12, {0xc0, 0x80}, 0x0dfff086},
        {12, {0xed, 0xa0, 0x80}, 0x0dfff086},
        {12, {'A', 0xe2, 0x82}, 0x0dfff086},
        {12, {0xe0, 0x80, 0x80}, 0x0dfff086},
        {12, {0xc3, 0x41}, 0x0dfff086},
        {12, {0xf4, 0x90, 0x80, 0x80}, 0x0dfff086},
    };
    for (const auto& test : cases) {
        const Bytes name = Element(0x30, Element(0x31, Attribute(0x03, test.tag, test.value)));
        const std::string path = WriteFile("libressl3-load-locations-rejected.pem", CertificatePem(name));
        LibreSslX509Store* store = NewStore();
        ERR_clear_error_nid_postfix();
        Require(X509_STORE_load_locations_nid_postfix(store, path.c_str(), nullptr) == 0);
        Require(ObjectCount(store) == 0);
        Require(ERR_get_error_nid_postfix() == test.error);
        ERR_clear_error_nid_postfix();
        X509_STORE_free_nid_postfix(store);
        Require(remove_nid_postfix(path.c_str()) == 0);
    }
}

void TestLatin1LookupName() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "anyps5-libressl3-latin1-ca";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    Require(std::filesystem::create_directories(directory));
    AddPathAlias_nid_no_patch("/ca-latin1", directory.string().c_str());
    const Bytes stored = Element(0x30, Element(0x31, Attribute(0x03, 19, {'C', 'a', 'f', 0xe9})));
    WriteFile("/ca-latin1/3008ee67.0", CertificatePem(stored));

    LibreSslX509Store* store = NewStore();
    Require(X509_STORE_load_locations_nid_postfix(store, nullptr, "/ca-latin1") == 1);
    const auto* lookups = static_cast<const LibreSslStack*>(store->lookupMethods);
    auto* lookup = reinterpret_cast<LibreSslX509Lookup*>(lookups->data[0]);
    const auto bySubject = reinterpret_cast<int (APS5_VABI*)(LibreSslX509Lookup*, int, LibreSslX509Name*, LibreSslX509Object*)>(
        lookup->method->getBySubject);

    Asn1Object_nid_no_patch object{};
    object.length = static_cast<int>(sizeof(OidCommonName));
    object.data = OidCommonName;
    unsigned char visible[] = {'C', 'a', 'f', 0xe9};
    Asn1String value{static_cast<int>(sizeof(visible)), 26, visible};
    LibreSslX509NameEntry entry{&object, &value, 0, 0};
    LibreSslStack* entries = sk_new_nid_postfix(nullptr);
    Require(entries != nullptr);
    Require(sk_push_nid_postfix(entries, &entry) > 0);
    LibreSslX509Name name{entries, 0, nullptr, nullptr, 0};
    LibreSslX509Object result{};
    Require(bySubject(lookup, LibreSslX509LookupX509, &name, &result) == 1);
    Require(ObjectCount(store) == 1);
    Require(result.data == StoredObject(store, 0)->data);
    unsigned char invalidUtf8[] = {0xc0, 0x80};
    value = {static_cast<int>(sizeof(invalidUtf8)), 12, invalidUtf8};
    result = {};
    Require(Throws([&] { bySubject(lookup, LibreSslX509LookupX509, &name, &result); }));
    sk_free_nid_postfix(entries);
    Throws([store] { X509_STORE_free_nid_postfix(store); });
    std::filesystem::remove_all(directory, ignored);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestFileBundle();
    TestDuplicateCertificate();
    TestNullArguments();
    TestMissingFile();
    TestEmptyAndMalformedFiles();
    TestFileErrorQueue();
    TestPemLines();
    TestAddCertificate();
    TestUnreadablePath();
    TestHashedDirectory();
    TestNegativeControl();
    TestConvertedNameStrings();
    TestRejectedNameStrings();
    TestLatin1LookupName();
    std::puts("libSceLibreSsl3 X509_STORE_load_locations tests passed");
    return 0;
}
