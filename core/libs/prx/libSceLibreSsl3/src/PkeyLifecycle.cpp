#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <atomic>
#include <cstdint>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int DifferentKeyTypes = 101;
constexpr int MissingParameters = 103;
constexpr int EcType = 408;

} // namespace

extern "C" {

LibreSsl3EvpPkey* APS5_VABI EVP_PKEY_new_nid_disambig1_nid_postfix(void) {
    auto* key = static_cast<LibreSsl3EvpPkey*>(calloc_nid_postfix(1, sizeof(LibreSsl3EvpPkey)));
    if (!key) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, MallocFailure);
        return nullptr;
    }
    key->references = 1;
    key->saveParameters = 1;
    return key;
}

void APS5_VABI EVP_PKEY_free_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* key) {
    if (!key) return;
    auto references = std::atomic_ref<std::int32_t>(key->references);
    if (references.fetch_sub(1, std::memory_order_acq_rel) > 1) return;
    if (key->type == EcType && key->key) {
        EC_KEY_free_nid_postfix(static_cast<EcKey*>(key->key));
        key->key = nullptr;
    }
    if (IsNativePkeyMethod_nid_no_patch(key->method)) {
        const auto* method = static_cast<const LibreSsl3PkeyAsn1Method*>(key->method);
        if (method->keyFree != nullptr) method->keyFree(key);
        key->key = nullptr;
        key->method = nullptr;
    }
    if (key->method || key->key) {
        references.fetch_add(1, std::memory_order_relaxed);
        NotImplemented_nid_no_patch("EVP_PKEY_free of native LibreSSL3 algorithm data");
    }
    auto* bytes = reinterpret_cast<volatile unsigned char*>(key);
    for (std::size_t i = 0; i < sizeof(*key); ++i) bytes[i] = 0;
    free_nid_postfix(key);
}

int APS5_VABI EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(LibreSsl3EvpPkey*, LibreSsl3Rsa*) {
    NotImplemented_nid_no_patch("EVP_PKEY_set1_RSA of native LibreSSL3 algorithm data");
}

int APS5_VABI EVP_PKEY_copy_parameters_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* to,
    const LibreSsl3EvpPkey* from) {
    if (!to || !from) return 0;
    if (to->type != from->type) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, DifferentKeyTypes);
        return 0;
    }
    if (to->type != EcType) NotImplemented_nid_no_patch("EVP_PKEY_copy_parameters for non-EC key types");
    const auto* sourceKey = static_cast<const EcKey*>(from->key);
    auto* destinationKey = static_cast<EcKey*>(to->key);
    if (!sourceKey || !sourceKey->group) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, MissingParameters);
        return 0;
    }
    if (!destinationKey) return 0;
    return EC_KEY_set_group_nid_postfix(destinationKey, sourceKey->group);
}

}
