#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSsl3Rsa) == 152);
static_assert(offsetof(LibreSsl3Rsa, n) == 24);
static_assert(offsetof(LibreSsl3Rsa, d) == 40);
static_assert(sizeof(LibreSsl3Dh) == 128);
static_assert(offsetof(LibreSsl3Dh, p) == 8);
static_assert(offsetof(LibreSsl3Dh, g) == 16);
static_assert(offsetof(LibreSsl3Dh, pubKey) == 32);
static_assert(offsetof(LibreSsl3Dh, privKey) == 40);
static_assert(offsetof(LibreSsl3Dh, q) == 64);
static_assert(sizeof(LibreSsl3Dsa) == 112);
static_assert(offsetof(LibreSsl3Dsa, p) == 16);
static_assert(offsetof(LibreSsl3Dsa, g) == 32);
static_assert(offsetof(LibreSsl3Dsa, privKey) == 48);

namespace {

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename TKey, std::size_t N>
void CopyKeyOutputs(const TKey* key, const char* function, const std::pair<const void**, const void* const TKey::*> (&fields)[N]) {
    bool anyOutput = false;
    for (const auto& entry : fields) {
        if (entry.first != nullptr) {
            anyOutput = true;
            break;
        }
    }
    if (!key && anyOutput)
        throw std::invalid_argument(std::string(function) + ": null key");
    if (!anyOutput)
        return;
    TKey guestKey{};
    if (!CopyGuestInPreservingErrno(guestKey, key))
        NotImplemented_nid_no_patch(function);
    const void* previous[N]{};
    bool written[N]{};
    for (std::size_t i = 0; i < N; ++i) {
        const void** const out = fields[i].first;
        if (!out)
            continue;
        const bool hadPrior = CopyGuestInPreservingErrno(previous[i], out);
        if (!CopyGuestOutPreservingErrno(out, guestKey.*(fields[i].second))) {
            for (std::size_t j = i; j > 0; --j) {
                if (written[j - 1])
                    CopyGuestOutPreservingErrno(fields[j - 1].first, previous[j - 1]);
            }
            NotImplemented_nid_no_patch(function);
        }
        written[i] = hadPrior;
    }
}

}

extern "C" {

void APS5_VABI RSA_get0_key_nid_postfix(const LibreSsl3Rsa* rsa, const void** n, const void** e, const void** d) {
    CopyKeyOutputs(rsa, __func__, {{n, &LibreSsl3Rsa::n}, {e, &LibreSsl3Rsa::e}, {d, &LibreSsl3Rsa::d}});
}

void APS5_VABI DH_get0_pqg_nid_postfix(const LibreSsl3Dh* dh, const void** p, const void** q, const void** g) {
    CopyKeyOutputs(dh, __func__, {{p, &LibreSsl3Dh::p}, {q, &LibreSsl3Dh::q}, {g, &LibreSsl3Dh::g}});
}

void APS5_VABI DH_get0_key_nid_postfix(const LibreSsl3Dh* dh, const void** pubKey, const void** privKey) {
    CopyKeyOutputs(dh, __func__, {{pubKey, &LibreSsl3Dh::pubKey}, {privKey, &LibreSsl3Dh::privKey}});
}

void APS5_VABI DSA_get0_pqg_nid_postfix(const LibreSsl3Dsa* dsa, const void** p, const void** q, const void** g) {
    CopyKeyOutputs(dsa, __func__, {{p, &LibreSsl3Dsa::p}, {q, &LibreSsl3Dsa::q}, {g, &LibreSsl3Dsa::g}});
}

void APS5_VABI DSA_get0_key_nid_postfix(const LibreSsl3Dsa* dsa, const void** pubKey, const void** privKey) {
    CopyKeyOutputs(dsa, __func__, {{pubKey, &LibreSsl3Dsa::pubKey}, {privKey, &LibreSsl3Dsa::privKey}});
}

}
