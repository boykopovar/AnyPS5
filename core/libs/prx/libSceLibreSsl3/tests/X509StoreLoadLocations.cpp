#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <system_error>

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
    Require(bySubject(lookup, LibreSslX509LookupX509, &name, &result) == 1);
    Require(result.type == LibreSslX509LookupX509 && result.data != nullptr);
    Require(ObjectCount(store) == 1);
    Require(result.data == StoredObject(store, 0)->data);
    Require(bySubject(lookup, LibreSslX509LookupX509, nullptr, &result) == 0);
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
    TestAddCertificate();
    TestUnreadablePath();
    TestHashedDirectory();
    TestNegativeControl();
    std::puts("libSceLibreSsl3 X509_STORE_load_locations tests passed");
    return 0;
}
