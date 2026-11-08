#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

extern "C" int APS5_VABI EVP_PKEY_base_id_nid_postfix(const LibreSsl3EvpPkey* key) {
    if (!key) return 0;
    return EVP_PKEY_type_nid_postfix(key->type);
}
