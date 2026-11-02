#include "prx/libkernel/File/include/TrustedRoots.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Certificate trust check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr char Certificate[] = R"(-----BEGIN CERTIFICATE-----
MIIBijCCATGgAwIBAgIUFMQYJ1in3pTJaCqjg6JVgSBNQgkwCgYIKoZIzj0EAwIw
GzEZMBcGA1UEAwwQQW55UFM1LVRlc3QtUm9vdDAeFw0yNjEwMTMwOTE2NTRaFw0z
NjEwMTAwOTE2NTRaMBsxGTAXBgNVBAMMEEFueVBTNS1UZXN0LVJvb3QwWTATBgcq
hkjOPQIBBggqhkjOPQMBBwNCAAQiHlYF9b/zaT5lFC26MOBndhdySsdLsuthNut0
fhQkEHlybL5nUrvOPZrkV1e/vEZ2ykmq7xMkSYeIHnNy4wxMo1MwUTAdBgNVHQ4E
FgQUJKW6nreyN5yBQn0axEJkZnkfZB4wHwYDVR0jBBgwFoAUJKW6nreyN5yBQn0a
xEJkZnkfZB4wDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNHADBEAiBfTER0
UmcQ0u8NzHTODNgVOHZHQFrYjcOyVjzZlSKomQIgDhztVM5obBElAnm22JgWEPk+
Bk30RAcx5xk3rjar3qs=
-----END CERTIFICATE-----
)";

static void CheckWindowsTrust() {
    DWORD size = 0;
    Require(CryptStringToBinaryA(Certificate, sizeof(Certificate) - 1, CRYPT_STRING_BASE64HEADER, nullptr, &size, nullptr, nullptr));
    std::vector<unsigned char> der(size);
    Require(CryptStringToBinaryA(Certificate, sizeof(Certificate) - 1, CRYPT_STRING_BASE64HEADER, der.data(), &size, nullptr, nullptr));
    const auto roots = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
    const auto disallowed = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr);
    Require(roots && disallowed);
    PCCERT_CONTEXT certificate = nullptr;
    Require(CertAddEncodedCertificateToStore(roots, X509_ASN_ENCODING, der.data(), size, CERT_STORE_ADD_NEW, &certificate));
    const auto unrestricted = ExportHostRoots_nid_no_patch(roots, disallowed);
    Require(!unrestricted.empty() && unrestricted.find('\0') == std::string::npos);
    DWORD decodedSize = size;
    std::vector<unsigned char> decoded(decodedSize);
    Require(CryptStringToBinaryA(unrestricted.c_str(), static_cast<DWORD>(unrestricted.size()), CRYPT_STRING_BASE64HEADER,
        decoded.data(), &decodedSize, nullptr, nullptr));
    Require(decoded == der);
    CERT_ENHKEY_USAGE usage{};
    Require(CertSetEnhancedKeyUsage(certificate, &usage));
    Require(ExportHostRoots_nid_no_patch(roots, disallowed).empty());
    char* email = const_cast<char*>(szOID_PKIX_KP_EMAIL_PROTECTION);
    usage = {1, &email};
    Require(CertSetEnhancedKeyUsage(certificate, &usage));
    Require(ExportHostRoots_nid_no_patch(roots, disallowed).empty());
    char* server = const_cast<char*>(szOID_PKIX_KP_SERVER_AUTH);
    usage = {1, &server};
    Require(CertSetEnhancedKeyUsage(certificate, &usage));
    Require(ExportHostRoots_nid_no_patch(roots, disallowed) == unrestricted);
    Require(CertAddCertificateContextToStore(disallowed, certificate, CERT_STORE_ADD_NEW, nullptr));
    Require(ExportHostRoots_nid_no_patch(roots, disallowed).empty());
    Require(CertFreeCertificateContext(certificate));
    Require(CertCloseStore(disallowed, CERT_CLOSE_STORE_CHECK_FLAG));
    Require(CertCloseStore(roots, CERT_CLOSE_STORE_CHECK_FLAG));
}

int main() { CheckWindowsTrust(); }
