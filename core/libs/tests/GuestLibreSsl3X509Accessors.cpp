#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void CheckAccessors() {
    Asn1String notBefore{5, 23, nullptr, 0};
    Asn1String notAfter{7, 23, nullptr, 0};
    LibreSsl3X509Val validity{&notBefore, &notAfter};
    LibreSslStack extensions{2, nullptr, 0, 4, nullptr};
    void* extensionData[4] = {reinterpret_cast<void*>(0x10), reinterpret_cast<void*>(0x20), nullptr, nullptr};
    extensions.data = reinterpret_cast<char**>(extensionData);
    LibreSsl3X509CertInfo certInfo{};
    certInfo.version = nullptr;
    certInfo.validity = &validity;
    certInfo.extensions = &extensions;
    LibreSsl3X509 certificate{};
    certificate.certInfo = &certInfo;
    certificate.sigAlg = reinterpret_cast<void*>(0xABC);
    certificate.signature = reinterpret_cast<void*>(0xDEF);
    certificate.valid = 1;
    certificate.references = 7;

    Require(X509_get0_notBefore_nid_postfix(&certificate) == &notBefore, "notBefore mismatch");
    Require(X509_get0_notAfter_nid_postfix(&certificate) == &notAfter, "notAfter mismatch");
    Require(X509_getm_notBefore_nid_postfix(&certificate) == &notBefore, "mutable notBefore mismatch");
    Require(X509_getm_notAfter_nid_postfix(&certificate) == &notAfter, "mutable notAfter mismatch");

    const Asn1String* signature = nullptr;
    void* algorithm = nullptr;
    X509_get0_signature_nid_postfix(&signature, &algorithm, &certificate);
    Require(signature == reinterpret_cast<const Asn1String*>(certificate.signature), "signature pointer mismatch");
    Require(algorithm == certificate.sigAlg, "algorithm pointer mismatch");

    X509_get0_signature_nid_postfix(nullptr, &algorithm, &certificate);
    Require(algorithm == certificate.sigAlg, "null signature output must not clobber algorithm");
    X509_get0_signature_nid_postfix(&signature, nullptr, &certificate);
    Require(signature == reinterpret_cast<const Asn1String*>(certificate.signature), "null algorithm output must not clobber signature");

    Require(X509_get_ext_count_nid_postfix(&certificate) == 2, "extension count mismatch");
    Require(X509_get_ext_nid_postfix(&certificate, 0) == extensionData[0], "extension 0 mismatch");
    Require(X509_get_ext_nid_postfix(&certificate, 1) == extensionData[1], "extension 1 mismatch");
    Require(X509_get_ext_nid_postfix(&certificate, 2) == nullptr, "out of range extension must be null");
    Require(X509_get_ext_nid_postfix(&certificate, -1) == nullptr, "negative extension index must be null");
    Require(X509_get0_extensions_nid_postfix(&certificate) == &extensions, "extensions stack mismatch");

    certInfo.validity = nullptr;
    Require(X509_get0_notBefore_nid_postfix(&certificate) == nullptr, "null validity must yield null notBefore");
    Require(X509_get0_notAfter_nid_postfix(&certificate) == nullptr, "null validity must yield null notAfter");
    Require(X509_getm_notBefore_nid_postfix(&certificate) == nullptr, "null validity must yield null mutable notBefore");
    Require(X509_getm_notAfter_nid_postfix(&certificate) == nullptr, "null validity must yield null mutable notAfter");

    certInfo.extensions = nullptr;
    Require(X509_get_ext_count_nid_postfix(&certificate) == 0, "null extensions must yield zero count");
    Require(X509_get_ext_nid_postfix(&certificate, 0) == nullptr, "null extensions must yield null extension");
    Require(X509_get0_extensions_nid_postfix(&certificate) == nullptr, "null extensions must yield null stack");

    bool nullThrown = false;
    try {
        X509_get0_notBefore_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "null certificate for notBefore did not throw");

    nullThrown = false;
    try {
        X509_get_ext_count_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "null certificate for ext count did not throw");

    nullThrown = false;
    try {
        X509_get0_signature_nid_postfix(nullptr, nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "null certificate for signature did not throw");
}

int main() {
    CheckAccessors();
    return 0;
}
