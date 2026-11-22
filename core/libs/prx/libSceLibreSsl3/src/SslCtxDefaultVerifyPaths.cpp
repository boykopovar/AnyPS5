#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

constexpr std::array<const char*, 7> CertificateFileCandidates{{
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/cert.pem",
    "/opt/homebrew/etc/openssl@3/cert.pem",
    "/usr/local/etc/openssl@3/cert.pem",
    "/usr/local/etc/openssl/cert.pem",
    "/usr/local/etc/ssl/cert.pem"
}};

constexpr const char* SystemCertificateList = "/system/common/cert/CA_LIST.cer";
constexpr const char* CertificateDirectoryCandidate = "/etc/ssl/certs";
constexpr const char* CertificateFileVariable = "SSL_CERT_FILE";
constexpr const char* CertificateDirectoryVariable = "SSL_CERT_DIR";

bool Exists(const char* path) {
    const int savedErrno = errno;
    const bool present = access_nid_postfix(path, 0) == 0;
    errno = savedErrno;
    return present;
}

bool ReadOverride(const char* name, std::string& value, const char* caller) {
    char* const text = getenv_nid_postfix(name);
    if (!text) return false;
    const int savedErrno = errno;
    const int error = CopyGuestString_nid_no_patch(text, 65536, value);
    errno = savedErrno;
    if (error != 0) NotImplemented_nid_no_patch(caller);
    return true;
}

bool Load(LibreSslX509Store* store, const char* file, const char* directory) {
    const int savedErrno = errno;
    bool loaded = false;
    try {
        loaded = X509_STORE_load_locations_nid_postfix(store, file, directory) == 1;
    } catch (const std::exception&) {
        loaded = false;
    }
    errno = savedErrno;
    return loaded;
}

}

extern "C" int APS5_VABI SSL_CTX_set_default_verify_paths_nid_postfix(LibreSslCtx* context) {
    if (!context) return 0;
    LibreSslCtx guestContext{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestContext, context, sizeof(guestContext));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(__func__);
    if (!guestContext.certStore) return 0;
    auto* const store = static_cast<LibreSslX509Store*>(guestContext.certStore);
    std::string file;
    if (!ReadOverride(CertificateFileVariable, file, __func__) || file.empty()) {
        file.clear();
        for (const char* candidate : CertificateFileCandidates) {
            if (Exists(candidate)) {
                file = candidate;
                break;
            }
        }
        if (file.empty() && Exists(SystemCertificateList)) file = SystemCertificateList;
    }
    std::string directory;
    if (!ReadOverride(CertificateDirectoryVariable, directory, __func__) || directory.empty()) {
        directory = Exists(CertificateDirectoryCandidate) ? CertificateDirectoryCandidate : "";
    }
    int loaded = 0;
    if (!file.empty() && Load(store, file.c_str(), nullptr)) loaded = 1;
    if (!directory.empty() && Load(store, nullptr, directory.c_str())) loaded = 1;
    return loaded;
}
