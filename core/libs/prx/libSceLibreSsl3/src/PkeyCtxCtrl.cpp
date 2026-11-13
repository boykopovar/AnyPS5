#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"

extern "C" int APS5_VABI EVP_PKEY_CTX_ctrl_nid_postfix(void* context, int keyType, int operationType, int command, int argument, void* pointer) {
    return PkeyContextControl_nid_no_patch(static_cast<PkeyContext*>(context), keyType, operationType, command, argument, pointer);
}
