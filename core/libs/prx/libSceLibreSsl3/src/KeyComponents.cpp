#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
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

template<typename TKey, typename... TOutputs> const TKey* Key(const TKey* key, const char* function, TOutputs... outputs) {
    if (!key && (... || (outputs != nullptr))) throw std::invalid_argument(std::string(function) + ": null key");
    return key;
}

template<typename TKey> void Store(const void** out, const TKey* key, const void* const TKey::* field) {
    if (out) *out = key->*field;
}

}

extern "C" {

void APS5_VABI RSA_get0_key_nid_postfix(const LibreSsl3Rsa* rsa, const void** n, const void** e, const void** d) {
    const LibreSsl3Rsa* key = Key(rsa, __func__, n, e, d);
    Store(n, key, &LibreSsl3Rsa::n);
    Store(e, key, &LibreSsl3Rsa::e);
    Store(d, key, &LibreSsl3Rsa::d);
}

void APS5_VABI DH_get0_pqg_nid_postfix(const LibreSsl3Dh* dh, const void** p, const void** q, const void** g) {
    const LibreSsl3Dh* key = Key(dh, __func__, p, q, g);
    Store(p, key, &LibreSsl3Dh::p);
    Store(q, key, &LibreSsl3Dh::q);
    Store(g, key, &LibreSsl3Dh::g);
}

void APS5_VABI DH_get0_key_nid_postfix(const LibreSsl3Dh* dh, const void** pubKey, const void** privKey) {
    const LibreSsl3Dh* key = Key(dh, __func__, pubKey, privKey);
    Store(pubKey, key, &LibreSsl3Dh::pubKey);
    Store(privKey, key, &LibreSsl3Dh::privKey);
}

void APS5_VABI DSA_get0_pqg_nid_postfix(const LibreSsl3Dsa* dsa, const void** p, const void** q, const void** g) {
    const LibreSsl3Dsa* key = Key(dsa, __func__, p, q, g);
    Store(p, key, &LibreSsl3Dsa::p);
    Store(q, key, &LibreSsl3Dsa::q);
    Store(g, key, &LibreSsl3Dsa::g);
}

void APS5_VABI DSA_get0_key_nid_postfix(const LibreSsl3Dsa* dsa, const void** pubKey, const void** privKey) {
    const LibreSsl3Dsa* key = Key(dsa, __func__, pubKey, privKey);
    Store(pubKey, key, &LibreSsl3Dsa::pubKey);
    Store(privKey, key, &LibreSsl3Dsa::privKey);
}

}
