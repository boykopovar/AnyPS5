#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"

extern "C" void APS5_VABI EVP_PKEY_CTX_free_nid_postfix(void* context) {
    PkeyContextFree_nid_no_patch(static_cast<PkeyContext*>(context));
}
