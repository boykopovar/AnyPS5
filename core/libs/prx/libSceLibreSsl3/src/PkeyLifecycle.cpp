#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <atomic>
#include <cstdint>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;

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

int APS5_VABI EVP_PKEY_copy_parameters_nid_disambig1_nid_postfix(LibreSsl3EvpPkey*, const LibreSsl3EvpPkey*) {
    NotImplemented_nid_no_patch("EVP_PKEY_copy_parameters of native LibreSSL3 algorithm data");
}

}
