#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

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

const unsigned char SubjectEncodingA[] = {0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x09, 't', 'e', 's', 't', ' ', 'c', 'a', ' ', 'a'};
const unsigned char SubjectEncodingB[] = {0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x09, 't', 'e', 's', 't', ' ', 'c', 'a', ' ', 'b'};
const unsigned char SubjectA[] = "Test CA A";
const unsigned char OidCommonName[] = {0x55, 0x04, 0x03};
constexpr const char* HashedNameA = "8deb836d.0";

constexpr const char* CertificateFileCandidates[] = {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/cert.pem",
    "/opt/homebrew/etc/openssl@3/cert.pem",
    "/usr/local/etc/openssl@3/cert.pem",
    "/usr/local/etc/openssl/cert.pem",
    "/usr/local/etc/ssl/cert.pem"
};
constexpr const char* CertificateDirectoryCandidate = "/etc/ssl/certs";
constexpr const char* MissingBundle = "/anyps5-libressl3-default-verify-paths-missing.pem";
constexpr const char* CertificateFileVariable = "SSL_CERT_FILE";
constexpr const char* CertificateDirectoryVariable = "SSL_CERT_DIR";
constexpr const char* TemporaryMount = "/anyps5-libressl3-default-verify-paths-tmp";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_CTX_set_default_verify_paths check failed at line %u\n", location.line());
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

const std::filesystem::path& TemporaryRoot() {
    static const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "anyps5-libressl3-default-verify-paths";
    return root;
}

std::string HostPath(const char* name) {
    const std::filesystem::path host = TemporaryRoot() / name;
    std::error_code ignored;
    std::filesystem::create_directories(host.parent_path(), ignored);
    return host.string();
}

std::string WriteGuestFile(const char* name, const std::string& contents) {
    HostPath(name);
    const std::string guestPath = std::string(TemporaryMount) + "/" + name;
    FileStream* stream = fopen_nid_postfix(guestPath.c_str(), "wb");
    Require(stream != nullptr);
    Require(fwrite_nid_postfix(contents.data(), 1, contents.size(), stream) == contents.size());
    Require(fclose_nid_postfix(stream) == 0);
    return guestPath;
}

const char T61Certificate[] = R"PEM(
-----BEGIN CERTIFICATE-----
MIGXMIGBoAMCAQICAhI0MA0GCSqGSIb3DQEBCwUAMBYxFDASBgNVBAMUC1Rlc3Qg
Q0EgVDYxMB4XDTI0MDEwMTAwMDAwMFoXDTM0MDEwMTAwMDAwMFowFjEUMBIGA1UE
AxQLVGVzdCBDQSBUNjEwFzANBgkqhkiG9w0BAQEFAAMGADADAQABMA0GCSqGSIb3
DQEBCwUAAwIAAQ==
-----END CERTIFICATE-----
)PEM";

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

bool HoldsCertificates(LibreSslX509Store* store, int expected) {
    return ObjectCount(store) == expected;
}

LibreSslX509Store* NewStore() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    Require(ObjectCount(store) == 0);
    Require(LookupCount(store) == 0);
    return store;
}

LibreSsl3SslCtx ContextFor(LibreSslX509Store* store) {
    LibreSsl3SslCtx context{};
    context.certStore = store;
    return context;
}

const LibreSslX509Object* StoredObject(const LibreSslX509Store* store, int index) {
    const auto* stack = static_cast<const LibreSslStack*>(store->objects);
    Require(stack != nullptr && index < stack->num && stack->data != nullptr);
    return reinterpret_cast<const LibreSslX509Object*>(stack->data[index]);
}

const unsigned char* StoredSubject(const LibreSslX509Store* store, int index, int& length) {
    const LibreSslX509Object* object = StoredObject(store, index);
    Require(object != nullptr && object->type == LibreSslX509LookupX509 && object->data != nullptr);
    const auto* certificate = static_cast<const LibreSsl3X509*>(object->data);
    Require(certificate->certInfo != nullptr);
    const auto* info = reinterpret_cast<const LibreSsl3CertificateInfo*>(certificate->certInfo);
    const auto* subject = static_cast<const LibreSslX509Name*>(info->subject);
    Require(subject != nullptr && subject->canonEnc != nullptr && subject->canonEnclen > 0);
    length = subject->canonEnclen;
    return subject->canonEnc;
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

void BlockCandidates() {
    RemovePathAlias_nid_no_patch("/system/common/cert/CA_LIST.cer");
    for (const char* candidate : CertificateFileCandidates) BlockPathAlias_nid_no_patch(candidate);
    BlockPathAlias_nid_no_patch(CertificateDirectoryCandidate);
}

void ClearEnvironment() {
    Require(unsetenv_nid_postfix(CertificateFileVariable) == 0);
    Require(unsetenv_nid_postfix(CertificateDirectoryVariable) == 0);
}


void TestSystemCertificateListWithT61Subject() {
    BlockCandidates();
    ClearEnvironment();
    WriteGuestFile("system-ca.pem", T61Certificate);
    AddPathAlias_nid_no_patch("/system/common/cert/CA_LIST.cer", HostPath("system-ca.pem").c_str());
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(ObjectCount(store) == 1);
    static const unsigned char expected[] = {0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0b,
        't', 'e', 's', 't', ' ', 'c', 'a', ' ', 't', '6', '1'};
    int length = 0;
    const unsigned char* subject = StoredSubject(store, 0, length);
    Require(length == static_cast<int>(sizeof(expected)));
    Require(std::memcmp(subject, expected, sizeof(expected)) == 0);
}

void TestNoCandidateLocations() {
    BlockCandidates();
    BlockPathAlias_nid_no_patch("/system/common/cert/CA_LIST.cer");
    ClearEnvironment();
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    if (access_nid_postfix("/system/common/cert/CA_LIST.cer", 0) == 0) {
        Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
        Require(ObjectCount(store) > 0);
        Require(LookupCount(store) == 1);
        return;
    }
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 0);
    Require(ObjectCount(store) == 0);
    Require(LookupCount(store) == 0);
    X509_STORE_free_nid_postfix(store);
}

void TestDefaultFileCandidate() {
    const std::string bundle = WriteGuestFile("candidate/ca-certificates.crt", std::string(CertificateA) + CertificateB);
    const std::string later = WriteGuestFile("candidate/later.pem", CertificateB);
    AddPathAlias_nid_no_patch(CertificateDirectoryCandidate, HostPath("candidate").c_str());
    AddPathAlias_nid_no_patch(CertificateFileCandidates[0], bundle.c_str());
    AddPathAlias_nid_no_patch(CertificateFileCandidates[2], later.c_str());
    ClearEnvironment();
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(HoldsCertificates(store, 2));
    Require(LookupCount(store) == 2);
    int length = 0;
    const unsigned char* first = StoredSubject(store, 0, length);
    Require(length == static_cast<int>(sizeof(SubjectEncodingA)));
    Require(std::memcmp(first, SubjectEncodingA, sizeof(SubjectEncodingA)) == 0);
    const unsigned char* second = StoredSubject(store, 1, length);
    Require(length == static_cast<int>(sizeof(SubjectEncodingB)));
    Require(std::memcmp(second, SubjectEncodingB, sizeof(SubjectEncodingB)) == 0);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(HoldsCertificates(store, 2));
    Require(LookupCount(store) == 2);
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
}

void TestEnvironmentOverridesCandidates() {
    BlockCandidates();
    const std::string overrideBundle = WriteGuestFile("override/override.pem", CertificateB);
    Require(setenv_nid_postfix(CertificateFileVariable, overrideBundle.c_str(), 1) == 0);
    Require(unsetenv_nid_postfix(CertificateDirectoryVariable) == 0);
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(HoldsCertificates(store, 1));
    Require(LookupCount(store) == 1);
    int length = 0;
    const unsigned char* subject = StoredSubject(store, 0, length);
    Require(length == static_cast<int>(sizeof(SubjectEncodingB)));
    Require(std::memcmp(subject, SubjectEncodingB, sizeof(SubjectEncodingB)) == 0);
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    ClearEnvironment();
}

void TestUnreadableEnvironmentFile() {
    BlockCandidates();
    Require(setenv_nid_postfix(CertificateFileVariable, MissingBundle, 1) == 0);
    Require(unsetenv_nid_postfix(CertificateDirectoryVariable) == 0);
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 0);
    Require(ObjectCount(store) == 0);
    X509_STORE_free_nid_postfix(store);
    ClearEnvironment();
}

void TestEnvironmentDirectory() {
    BlockCandidates();
    BlockPathAlias_nid_no_patch("/system/common/cert/CA_LIST.cer");
    Require(std::string(HashedFileName(SubjectEncodingA, sizeof(SubjectEncodingA))) == HashedNameA);
    WriteGuestFile((std::string("hashed/") + HashedNameA).c_str(), CertificateA);
    AddPathAlias_nid_no_patch("/anyps5-libressl3-default-dir", HostPath("hashed").c_str());
    Require(unsetenv_nid_postfix(CertificateFileVariable) == 0);
    Require(setenv_nid_postfix(CertificateDirectoryVariable, "/anyps5-libressl3-default-dir", 1) == 0);
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(ObjectCount(store) == 0);
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
    const auto bySubject = reinterpret_cast<int (APS5_VABI*)(LibreSslX509Lookup*, int, LibreSslX509Name*,
        LibreSslX509Object*)>(lookup->method->getBySubject);
    Require(bySubject(lookup, LibreSslX509LookupX509, &name, &result) == 1);
    Require(result.type == LibreSslX509LookupX509 && result.data != nullptr);
    Require(HoldsCertificates(store, 1));
    sk_free_nid_postfix(entries);

    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(HoldsCertificates(store, 1));
    Require(LookupCount(store) == 1);
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    ClearEnvironment();
}

void TestNullArguments() {
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(nullptr) == 0);
    LibreSsl3SslCtx context{};
    Require(context.certStore == nullptr);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 0);
}

void TestNegativeControl() {
    BlockCandidates();
    const std::string bundle = WriteGuestFile("control/control.pem", CertificateB);
    Require(setenv_nid_postfix(CertificateFileVariable, bundle.c_str(), 1) == 0);
    LibreSslX509Store* store = NewStore();
    LibreSsl3SslCtx context = ContextFor(store);
    Require(SSL_CTX_set_default_verify_paths_nid_postfix(&context) == 1);
    Require(HoldsCertificates(store, 1));
    auto* stack = const_cast<LibreSslStack*>(static_cast<const LibreSslStack*>(store->objects));
    stack->num -= 1;
    Require(!HoldsCertificates(store, 1));
    stack->num += 1;
    Require(HoldsCertificates(store, 1));
    Require(Throws([store] { X509_STORE_free_nid_postfix(store); }));
    ClearEnvironment();
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    std::error_code ignored;
    std::filesystem::remove_all(TemporaryRoot(), ignored);
    Require(std::filesystem::create_directories(TemporaryRoot()));
    AddPathAlias_nid_no_patch(TemporaryMount, TemporaryRoot().string().c_str());
    TestSystemCertificateListWithT61Subject();
    TestNoCandidateLocations();
    TestDefaultFileCandidate();
    TestEnvironmentOverridesCandidates();
    TestUnreadableEnvironmentFile();
    TestEnvironmentDirectory();
    TestNullArguments();
    TestNegativeControl();
    std::puts("libSceLibreSsl3 SSL_CTX_set_default_verify_paths tests passed");
    return 0;
}
