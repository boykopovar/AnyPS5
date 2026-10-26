#include "prx/libkernel/File/include/TrustedRoots.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {

void RequireCrypto(bool success, const char* operation) {
    if (!success) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

bool AllowsServerAuthentication(PCCERT_CONTEXT certificate) {
    DWORD size = 0;
    RequireCrypto(CertGetEnhancedKeyUsage(certificate, 0, nullptr, &size), "Reading root certificate usage size");
    std::vector<unsigned char> storage(size);
    auto* usage = reinterpret_cast<PCERT_ENHKEY_USAGE>(storage.data());
    SetLastError(ERROR_SUCCESS);
    RequireCrypto(CertGetEnhancedKeyUsage(certificate, 0, usage, &size), "Reading root certificate usage");
    if (usage->cUsageIdentifier == 0) return GetLastError() == static_cast<DWORD>(CRYPT_E_NOT_FOUND);
    for (DWORD index = 0; index < usage->cUsageIdentifier; ++index) {
        const auto* identifier = usage->rgpszUsageIdentifier[index];
        if (std::strcmp(identifier, szOID_PKIX_KP_SERVER_AUTH) == 0 || std::strcmp(identifier, szOID_ANY_ENHANCED_KEY_USAGE) == 0)
            return true;
    }
    return false;
}

std::string Pem(PCCERT_CONTEXT certificate) {
    DWORD size = 0;
    RequireCrypto(CryptBinaryToStringA(certificate->pbCertEncoded, certificate->cbCertEncoded,
        CRYPT_STRING_BASE64HEADER, nullptr, &size), "Measuring a PEM root certificate");
    std::string pem(size, '\0');
    RequireCrypto(CryptBinaryToStringA(certificate->pbCertEncoded, certificate->cbCertEncoded,
        CRYPT_STRING_BASE64HEADER, pem.data(), &size), "Encoding a PEM root certificate");
    pem.resize(size);
    return pem;
}

}

extern "C" std::string ExportHostRoots_nid_no_patch(HCERTSTORE roots, HCERTSTORE disallowed) {
    if (!roots || !disallowed) throw std::invalid_argument("ExportHostRoots: null certificate store");
    std::string bundle;
    using Certificate = std::unique_ptr<const CERT_CONTEXT, decltype(&CertFreeCertificateContext)>;
    Certificate certificate(nullptr, &CertFreeCertificateContext);
    for (;;) {
        auto* next = CertEnumCertificatesInStore(roots, certificate.release());
        if (!next) {
            const auto error = GetLastError();
            if (error != static_cast<DWORD>(CRYPT_E_NOT_FOUND))
                throw std::system_error(static_cast<int>(error), std::system_category(), "Enumerating trusted roots");
            break;
        }
        certificate.reset(next);
        Certificate rejected(CertFindCertificateInStore(disallowed, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0, CERT_FIND_EXISTING, certificate.get(), nullptr), &CertFreeCertificateContext);
        if (rejected) continue;
        const auto error = GetLastError();
        if (error != static_cast<DWORD>(CRYPT_E_NOT_FOUND))
            throw std::system_error(static_cast<int>(error), std::system_category(), "Checking distrusted roots");
        if (AllowsServerAuthentication(certificate.get())) bundle += Pem(certificate.get());
    }
    return bundle;
}

extern "C" std::string HostTrustedTlsRoots_nid_no_patch() {
    const auto closeStore = [](void* store) { CertCloseStore(store, 0); };
    using Store = std::unique_ptr<void, decltype(closeStore)>;
    const DWORD flags = CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG;
    Store roots(CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, flags, L"ROOT"), closeStore);
    RequireCrypto(roots != nullptr, "Opening the Windows trusted root store");
    Store disallowed(CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, flags, L"Disallowed"), closeStore);
    RequireCrypto(disallowed != nullptr, "Opening the Windows disallowed certificate store");
    return ExportHostRoots_nid_no_patch(roots.get(), disallowed.get());
}
