#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>

namespace {

constexpr int NidUndef = 0;
constexpr int NidRsaEncryption = 6;
constexpr int NidRsa = 19;
constexpr int NidDhKeyAgreement = 28;
constexpr int NidDsaWithSha = 66;
constexpr int NidDsa2 = 67;
constexpr int NidDsaWithSha1Old = 70;
constexpr int NidDsaWithSha1 = 113;
constexpr int NidDsa = 116;
constexpr int NidEcPublicKey = 408;
constexpr int NidHmac = 855;
constexpr int NidCmac = 894;
constexpr int NidRsaPss = 912;
constexpr int NidX25519 = 950;
constexpr int NidEd25519 = 952;

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

int BaseType(int type) {
    switch (type) {
    case NidRsaEncryption:
    case NidRsa:
        return NidRsaEncryption;
    case NidDsa:
    case NidDsa2:
    case NidDsaWithSha:
    case NidDsaWithSha1:
    case NidDsaWithSha1Old:
        return NidDsa;
    case NidDhKeyAgreement:
    case NidEcPublicKey:
    case NidHmac:
    case NidCmac:
    case NidRsaPss:
    case NidX25519:
    case NidEd25519:
        return type;
    default:
        return NidUndef;
    }
}

}

extern "C" int APS5_VABI EVP_PKEY_base_id_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key) throw std::invalid_argument("EVP_PKEY_base_id requires a key");
    LibreSsl3EvpPkey guestKey{};
    if (!CopyGuestInPreservingErrno(guestKey, key))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return BaseType(guestKey.type);
}
