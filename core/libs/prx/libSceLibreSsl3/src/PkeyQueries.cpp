#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstring>
#include <stdexcept>

namespace {

using PkeyQuery = int (APS5_VABI *)(const LibreSsl3EvpPkey*);
using PkeyCompare = int (APS5_VABI *)(const LibreSsl3EvpPkey*, const LibreSsl3EvpPkey*);

static_assert(sizeof(PkeyQuery) == 8);
static_assert(sizeof(PkeyCompare) == 8);
static_assert(offsetof(LibreSsl3EvpPkey, method) == 8);

template<class TCallback>
TCallback ReadCallback_nid_no_patch(const void* method, std::size_t offset) {
    TCallback callback;
    std::memcpy(&callback, static_cast<const unsigned char*>(method) + offset, sizeof(callback));
    return callback;
}

} // namespace

extern "C" {

int APS5_VABI EVP_PKEY_size_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key || !key->method)
        return 0;
    const auto size = ReadCallback_nid_no_patch<PkeyQuery>(key->method, 96);
    return size ? size(key) : 0;
}

int APS5_VABI EVP_PKEY_security_bits_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key)
        return 0;
    if (!key->method)
        return -2;
    const auto securityBits = ReadCallback_nid_no_patch<PkeyQuery>(key->method, 112);
    return securityBits ? securityBits(key) : -2;
}

int APS5_VABI EVP_PKEY_cmp_nid_postfix(const LibreSsl3EvpPkey* first, const LibreSsl3EvpPkey* second) {
    if (!first || !second)
        throw std::invalid_argument("EVP_PKEY_cmp requires two keys");
    if (first->type != second->type)
        return -1;
    if (!first->method)
        return -2;
    const auto compareParameters = ReadCallback_nid_no_patch<PkeyCompare>(first->method, 160);
    if (compareParameters) {
        const int result = compareParameters(first, second);
        if (result <= 0)
            return result;
    }
    const auto comparePublic = ReadCallback_nid_no_patch<PkeyCompare>(first->method, 56);
    return comparePublic ? comparePublic(first, second) : -2;
}

}
