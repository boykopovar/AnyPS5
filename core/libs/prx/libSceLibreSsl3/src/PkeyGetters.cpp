#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t EvpPkeyRsa = 6;
constexpr std::int32_t EvpPkeyRsaPss = 912;
constexpr std::int32_t EvpPkeyDsa = 116;
constexpr std::int32_t EvpPkeyDh = 28;

void* KeyOfType(LibreSsl3EvpPkey* key, std::int32_t first, std::int32_t second, const char* function) {
    if (!key)
        throw std::invalid_argument(std::string(function) + " requires a key");
    if (key->type != first && key->type != second)
        throw std::runtime_error(std::string(function) + ": key type " + std::to_string(key->type) +
                                 " does not match, and the LibreSSL3 error queue is not modelled");
    return key->key;
}

} // namespace

extern "C" {

LibreSsl3Rsa* APS5_VABI EVP_PKEY_get0_RSA_nid_postfix(LibreSsl3EvpPkey* key) {
    return static_cast<LibreSsl3Rsa*>(KeyOfType(key, EvpPkeyRsa, EvpPkeyRsaPss, "EVP_PKEY_get0_RSA"));
}

void* APS5_VABI EVP_PKEY_get0_DSA_nid_postfix(LibreSsl3EvpPkey* key) {
    return KeyOfType(key, EvpPkeyDsa, EvpPkeyDsa, "EVP_PKEY_get0_DSA");
}

void* APS5_VABI EVP_PKEY_get0_DH_nid_postfix(LibreSsl3EvpPkey* key) {
    return KeyOfType(key, EvpPkeyDh, EvpPkeyDh, "EVP_PKEY_get0_DH");
}

}
