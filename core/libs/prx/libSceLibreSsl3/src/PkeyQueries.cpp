#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>

namespace {

using PkeyQuery = int (APS5_VABI *)(const LibreSsl3EvpPkey*);
using PkeyCompare = int (APS5_VABI *)(const LibreSsl3EvpPkey*, const LibreSsl3EvpPkey*);

static_assert(sizeof(PkeyQuery) == 8);
static_assert(sizeof(PkeyCompare) == 8);
static_assert(offsetof(LibreSsl3EvpPkey, method) == 8);

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

LibreSsl3EvpPkey CopyKey(const LibreSsl3EvpPkey* key, const char* function) {
    LibreSsl3EvpPkey guestKey{};
    if (!CopyGuestInPreservingErrno(guestKey, key))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return guestKey;
}

template<class TCallback>
TCallback ReadCallback_nid_no_patch(const void* method, std::size_t offset, const char* function) {
    TCallback callback{};
    if (!CopyGuestInPreservingErrno(callback, reinterpret_cast<const TCallback*>(static_cast<const unsigned char*>(method) + offset)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return callback;
}

} // namespace

extern "C" {

int APS5_VABI EVP_PKEY_size_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key)
        return 0;
    const auto guestKey = CopyKey(key, __func__);
    if (!guestKey.method)
        return 0;
    const auto size = ReadCallback_nid_no_patch<PkeyQuery>(guestKey.method, 96, __func__);
    return size ? size(key) : 0;
}

int APS5_VABI EVP_PKEY_security_bits_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key)
        return 0;
    const auto guestKey = CopyKey(key, __func__);
    if (!guestKey.method)
        return -2;
    const auto securityBits = ReadCallback_nid_no_patch<PkeyQuery>(guestKey.method, 112, __func__);
    return securityBits ? securityBits(key) : -2;
}

int APS5_VABI EVP_PKEY_cmp_nid_postfix(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second) {
    if (!first || !second)
        throw std::invalid_argument("EVP_PKEY_cmp requires two keys");
    const auto guestFirst = CopyKey(first, __func__);
    const auto guestSecond = CopyKey(second, __func__);
    if (guestFirst.type != guestSecond.type)
        return -1;
    if (!guestFirst.method)
        return -2;
    const auto compareParameters = ReadCallback_nid_no_patch<PkeyCompare>(guestFirst.method, 160, __func__);
    if (compareParameters) {
        const int result = compareParameters(first, second);
        if (result <= 0)
            return result;
    }
    const auto comparePublic = ReadCallback_nid_no_patch<PkeyCompare>(CopyKey(first, __func__).method, 56, __func__);
    return comparePublic ? comparePublic(first, second) : -2;
}

}
