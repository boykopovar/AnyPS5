#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

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

template <typename T>
void EnsureWritablePreservingErrno(T* guest, const char* caller) {
    PreservedErrno guard;
    if (!GuestWritable_nid_no_patch(guest, sizeof(*guest)))
        NotImplemented_nid_no_patch(caller);
}

template <typename T>
void CopyOutPreservingErrno(T* guest, const T& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

LibreSsl3X509Pubkey CopyCheckedPubkey(const void* pub, const char* function) {
    if (!pub) throw std::invalid_argument(std::string(function) + ": null public key");
    return CopyInPreservingErrno<LibreSsl3X509Pubkey>(pub, function);
}

}

extern "C" {

int APS5_VABI X509_PUBKEY_get0_param_nid_postfix(void** publicKeyAlgorithm,
    const unsigned char** publicKey, int* publicKeyLength, void** algorithm,
    const void* pub) {
    if (!publicKeyAlgorithm && !publicKey && !algorithm) return 1;
    const auto key = CopyCheckedPubkey(pub, __func__);
    LibreSsl3X509Algor algor{};
    if (publicKeyAlgorithm) {
        if (!key.algor) throw std::invalid_argument(std::string(__func__) + ": null algorithm info");
        algor = CopyInPreservingErrno<LibreSsl3X509Algor>(key.algor, __func__);
    }
    LibreSsl3Asn1String encoded{};
    if (publicKey) {
        if (!key.publicKey) throw std::invalid_argument(std::string(__func__) + ": null encoded public key");
        if (!publicKeyLength) throw std::invalid_argument(std::string(__func__) + ": null public key length");
        encoded = CopyInPreservingErrno<LibreSsl3Asn1String>(key.publicKey, __func__);
    }
    if (publicKeyAlgorithm) EnsureWritablePreservingErrno(publicKeyAlgorithm, __func__);
    if (publicKey) {
        EnsureWritablePreservingErrno(publicKey, __func__);
        EnsureWritablePreservingErrno(publicKeyLength, __func__);
    }
    if (algorithm) EnsureWritablePreservingErrno(algorithm, __func__);
    if (publicKeyAlgorithm) CopyOutPreservingErrno(publicKeyAlgorithm, algor.algorithm, __func__);
    if (publicKey) {
        CopyOutPreservingErrno<const unsigned char*>(publicKey, encoded.data, __func__);
        CopyOutPreservingErrno(publicKeyLength, encoded.length, __func__);
    }
    if (algorithm) CopyOutPreservingErrno(algorithm, static_cast<void*>(key.algor), __func__);
    return 1;
}

}
