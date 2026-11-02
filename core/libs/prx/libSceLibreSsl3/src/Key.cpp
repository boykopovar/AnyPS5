#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

using PkeyBitsFunction = int (APS5_VABI *)(const LibreSsl3EvpPkey*);
constexpr std::size_t PkeyBitsOffset = 104;

static_assert(sizeof(PkeyBitsFunction) == 8);
static_assert(offsetof(LibreSsl3EvpPkey, method) == 8);
static_assert(std::atomic_ref<std::int32_t>::required_alignment <= alignof(std::int32_t));

int Retain_nid_no_patch(std::int32_t& references) {
    std::atomic_ref<std::int32_t> count(references);
    auto previous = count.load();
    for (;;) {
        if (previous == std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("LibreSSL3 key reference count overflow");
        if (count.compare_exchange_weak(previous, previous + 1))
            return previous > 0;
    }
}

} // namespace

extern "C" {

int APS5_VABI EVP_PKEY_bits_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key || !key->method)
        return 0;
    PkeyBitsFunction bits;
    std::memcpy(&bits, static_cast<const unsigned char*>(key->method) + PkeyBitsOffset, sizeof(bits));
    return bits ? bits(key) : 0;
}

int APS5_VABI EVP_PKEY_up_ref_nid_postfix(LibreSsl3EvpPkey* key) {
    if (!key)
        throw std::invalid_argument("EVP_PKEY_up_ref requires a key");
    return Retain_nid_no_patch(key->references);
}

int APS5_VABI EC_KEY_up_ref_nid_postfix(LibreSsl3EcKey* key) {
    if (!key)
        throw std::invalid_argument("EC_KEY_up_ref requires a key");
    return Retain_nid_no_patch(key->references);
}

int APS5_VABI RSA_up_ref_nid_postfix(LibreSsl3Rsa* rsa) {
    if (!rsa)
        throw std::invalid_argument("RSA_up_ref requires a key");
    return Retain_nid_no_patch(rsa->references);
}

}
