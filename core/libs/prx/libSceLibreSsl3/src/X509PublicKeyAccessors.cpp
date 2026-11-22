#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <vector>

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
void CopyOutPreservingErrno(T* guest, const T& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

LibreSsl3EvpPkey* DecodeEncodedKey(LibreSsl3X509Pubkey* guestKey, const LibreSsl3Asn1String& encoded, const char* caller) {
    std::vector<unsigned char> bytes;
    if (encoded.length > 0) {
        bytes.resize(static_cast<std::size_t>(encoded.length));
        PreservedErrno guard;
        if (!GuestCopyIn_nid_no_patch(bytes.data(), encoded.data, bytes.size()))
            NotImplemented_nid_no_patch(caller);
    }
    const unsigned char* cursor = bytes.data();
    auto* decoded = d2i_PUBKEY_nid_postfix(nullptr, &cursor, encoded.length);
    if (!decoded) return nullptr;

    auto* slot = reinterpret_cast<LibreSsl3EvpPkey**>(
        reinterpret_cast<unsigned char*>(guestKey) + offsetof(LibreSsl3X509Pubkey, pkey));
    const auto previous = CopyInPreservingErrno<LibreSsl3EvpPkey*>(slot, caller);
    if (previous) {
        EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
        return previous;
    }
    {
        PreservedErrno guard;
        if (!GuestWritable_nid_no_patch(slot, sizeof(*slot))) {
            EVP_PKEY_free_nid_disambig1_nid_postfix(decoded);
            NotImplemented_nid_no_patch(caller);
        }
    }
    CopyOutPreservingErrno(slot, decoded, caller);
    return decoded;
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
    if (!key.pkey && key.publicKey) {
        PreservedErrno guard;
        return DecodeEncodedKey(guestKey, CopyInPreservingErrno<LibreSsl3Asn1String>(key.publicKey, "X509_get0_pubkey"),
            "X509_get0_pubkey");
    }
    return key.pkey;
}
