#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename T>
T CopyInPreservingErrno(const void* guest, const char* caller) {
    PreservedErrno guard;
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, guest, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
    return value;
}

}

extern "C" void* APS5_VABI X509_get0_pubkey_nid_postfix(const LibreSsl3Certificate* certificate) {
    if (!certificate) return nullptr;
    const auto* certInfo = CopyInPreservingErrno<LibreSsl3CertificateInfo*>(
        reinterpret_cast<const unsigned char*>(certificate) + offsetof(LibreSsl3Certificate, certInfo), "X509_get0_pubkey");
    if (!certInfo) return nullptr;
    auto* guestKey = CopyInPreservingErrno<LibreSsl3X509Pubkey*>(
        reinterpret_cast<const unsigned char*>(certInfo) + offsetof(LibreSsl3CertificateInfo, key), "X509_get0_pubkey");
    if (!guestKey) return nullptr;
    const auto key = CopyInPreservingErrno<LibreSsl3X509Pubkey>(guestKey, "X509_get0_pubkey");
    if (!key.pkey && key.publicKey) NotImplemented_nid_no_patch("X509_get0_pubkey");
    return key.pkey;
}
