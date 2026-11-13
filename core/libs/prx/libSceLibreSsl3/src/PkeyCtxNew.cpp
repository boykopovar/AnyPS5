#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"

extern "C" void* APS5_VABI EVP_PKEY_CTX_new_nid_postfix(void* key, void* engine) {
    return PkeyContextNew_nid_no_patch(key, engine);
}
