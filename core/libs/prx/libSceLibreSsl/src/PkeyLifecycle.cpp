#include <atomic>
#include <cstddef>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {
struct Pkey {
    int type;
    int saveType;
    int references;
    const void* ameth;
    void* engine;
    void* key;
    int saveParameters;
    void* attributes;
};
static_assert(sizeof(int) == 4 && sizeof(void*) == 8, "layout assumes LP64");

constexpr int PkeyRsa = 6;
constexpr int LibraryEvp = 6;
constexpr int FunctionUnknown = 0xfff;
constexpr int ReasonDifferentKeyTypes = 101;
constexpr int ReasonExpectingAnRsaKey = 127;

const int RsaMethod = PkeyRsa;

void FreeKey(Pkey& pkey) {
    if (pkey.engine) NotImplemented_nid_no_patch("EVP_PKEY_free_it of a key with an ENGINE");
    if (pkey.ameth == &RsaMethod) {
        auto* rsa = static_cast<LibreSslRsa*>(pkey.key);
        pkey.key = nullptr;
        RSA_free_nid_postfix(rsa);
    } else if (pkey.ameth) {
        NotImplemented_nid_no_patch("EVP_PKEY_free_it of a key with an unknown algorithm method");
    }
}
}

extern "C" {

void* APS5_VABI EVP_PKEY_new_nid_postfix(void) {
    auto* pkey = static_cast<Pkey*>(malloc_nid_postfix(sizeof(Pkey)));
    if (!pkey) return nullptr;
    std::memset(pkey, 0, sizeof(Pkey));
    pkey->references = 1;
    pkey->saveParameters = 1;
    return pkey;
}

void APS5_VABI EVP_PKEY_free_nid_postfix(void* value) {
    if (!value) return;
    auto* pkey = static_cast<Pkey*>(value);
    auto references = std::atomic_ref<int>(pkey->references);
    if (references.fetch_sub(1, std::memory_order_acq_rel) - 1 > 0) return;
    const bool rsaKey = pkey->ameth == &RsaMethod;
    if ((!rsaKey && (pkey->ameth || pkey->key)) || pkey->engine || pkey->attributes) {
        references.fetch_add(1, std::memory_order_relaxed);
        NotImplemented_nid_no_patch("EVP_PKEY_free of a key that holds non-RSA algorithm data, an engine or attributes");
    }
    FreeKey(*pkey);
    free_nid_postfix(pkey);
}

int APS5_VABI EVP_PKEY_set1_RSA_nid_postfix(void* value, LibreSslRsa* key) {
    if (!value) throw std::invalid_argument(std::string(__func__) + ": null key");
    auto& pkey = *static_cast<Pkey*>(value);
    if (key && pkey.key == key && std::atomic_ref<int>(key->references).load() == 1)
        throw std::invalid_argument(std::string(__func__) + ": the key holds the only reference to this RSA object");
    if (pkey.key) FreeKey(pkey);
    if (pkey.saveType != PkeyRsa || !pkey.ameth) {
        if (pkey.engine) NotImplemented_nid_no_patch("EVP_PKEY_set1_RSA of a key with an ENGINE");
        pkey.ameth = &RsaMethod;
        pkey.type = PkeyRsa;
        pkey.saveType = PkeyRsa;
    }
    pkey.key = key;
    if (!key) return 0;
    std::atomic_ref<int>(key->references).fetch_add(1);
    return 1;
}

LibreSslRsa* APS5_VABI EVP_PKEY_get1_RSA_nid_postfix(void* value) {
    if (!value) throw std::invalid_argument(std::string(__func__) + ": null key");
    const auto& pkey = *static_cast<const Pkey*>(value);
    if (pkey.type != PkeyRsa) {
        LibreSslPutError_nid_no_patch(LibraryEvp, FunctionUnknown, ReasonExpectingAnRsaKey);
        return nullptr;
    }
    auto* rsa = static_cast<LibreSslRsa*>(pkey.key);
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": RSA key without an RSA object");
    std::atomic_ref<int>(rsa->references).fetch_add(1);
    return rsa;
}

int APS5_VABI EVP_PKEY_copy_parameters_nid_postfix(void* to, const void* from) {
    if (!to || !from) throw std::invalid_argument(std::string(__func__) + ": null key");
    const auto& target = *static_cast<const Pkey*>(to);
    const auto& source = *static_cast<const Pkey*>(from);
    if (target.type != source.type) {
        LibreSslPutError_nid_no_patch(LibraryEvp, FunctionUnknown, ReasonDifferentKeyTypes);
        return 0;
    }
    if (source.ameth && source.ameth != &RsaMethod)
        NotImplemented_nid_no_patch("EVP_PKEY_copy_parameters of a key with an unknown algorithm method");
    return 0;
}

}
