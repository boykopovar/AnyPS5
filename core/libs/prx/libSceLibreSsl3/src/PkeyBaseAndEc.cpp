#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

namespace {
constexpr int EvpPkeyEc = 408;
}

extern "C" {

int APS5_VABI EVP_PKEY_base_id_nid_postfix(const LibreSsl3EvpPkey* key) {
    return key ? key->type : 0;
}

void* APS5_VABI EVP_PKEY_get0_EC_KEY_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key || key->type != EvpPkeyEc) return nullptr;
    return key->key;
}

}
